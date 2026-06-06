#include "camdetect/DartDetector.hpp"
#include "camdetect/ZoneMapper.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace camdetect {

namespace {

constexpr float MIN_BLOB_AREA       =    80.f;
constexpr float MAX_BLOB_AREA       = 40000.f;  // dart from close cam can be big
constexpr float MIN_ASPECT_RATIO    =     2.5f;
constexpr float TIP_STABILITY_PX    =     5.0f;
constexpr float MAX_BOARD_RADIUS_MM =   230.0f;  // allow MISS zone just outside
constexpr float HUMAN_MAX_ASPECT    =     3.0f;  // hands/arms aren't elongated
constexpr int   INPUT_BLUR_KSIZE    =     5;

/// Convert a BGR uint8 frame to CIELAB float32 (each channel 0..255-ish).
/// Input is blurred first to kill H.264 macroblock noise.
cv::Mat toLab32(const cv::Mat& bgr)
{
    cv::Mat blurred, lab8, lab32;
    cv::GaussianBlur(bgr, blurred,
                     {INPUT_BLUR_KSIZE, INPUT_BLUR_KSIZE}, 0);
    cv::cvtColor(blurred, lab8, cv::COLOR_BGR2Lab);
    lab8.convertTo(lab32, CV_32FC3);
    return lab32;
}

/// Per-pixel L2 distance between two LAB float images, return CV_32F single-channel.
cv::Mat labDistance(const cv::Mat& a, const cv::Mat& b)
{
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    std::vector<cv::Mat> ch;
    cv::split(diff, ch);
    cv::Mat dist;
    cv::sqrt(ch[0].mul(ch[0]) + ch[1].mul(ch[1]) + ch[2].mul(ch[2]), dist);
    return dist;
}

/// Project the two endpoints of @p contour along the principal axis @p dir,
/// returning {endpoint_a, endpoint_b, unit_dir}.
struct Endpoints { cv::Point2f a, b, dir; };

/// Fit a line through @p seed_contour, then EXTEND its endpoints by scanning
/// the entire @p full_mask for any foreground pixel within @p merge_perp_px
/// of that line.  Captures collinear fragments of a dart whose middle is
/// invisible (black shaft on black sector).
Endpoints lineExtendByMask(const std::vector<cv::Point>& seed_contour,
                           const cv::Mat&                full_mask,
                           float                         merge_perp_px)
{
    Endpoints r{};
    if (seed_contour.empty()) return r;

    cv::Vec4f line;
    cv::fitLine(seed_contour, line, cv::DIST_L2, 0, 0.01, 0.01);
    const cv::Point2f dir   {line[0], line[1]};
    const cv::Point2f origin{line[2], line[3]};
    const cv::Point2f perp  {-dir.y, dir.x};

    float t_min =  std::numeric_limits<float>::max();
    float t_max = -std::numeric_limits<float>::max();
    for (int y = 0; y < full_mask.rows; ++y) {
        const uint8_t* row = full_mask.ptr<uint8_t>(y);
        for (int x = 0; x < full_mask.cols; ++x) {
            if (!row[x]) continue;
            const float dx = x - origin.x;
            const float dy = y - origin.y;
            const float d_perp = dx * perp.x + dy * perp.y;
            if (std::abs(d_perp) > merge_perp_px) continue;
            const float t = dx * dir.x + dy * dir.y;
            if (t < t_min) t_min = t;
            if (t > t_max) t_max = t;
        }
    }
    if (t_min > t_max) {
        for (const auto& p : seed_contour) {
            const float t = (p.x - origin.x) * dir.x + (p.y - origin.y) * dir.y;
            t_min = std::min(t_min, t);
            t_max = std::max(t_max, t);
        }
    }
    r.a   = origin + dir * t_min;
    r.b   = origin + dir * t_max;
    r.dir = dir;
    return r;
}

} // anon

DartDetector::DartDetector(int cam_id, BoardCalibration calib)
    : cam_id_(cam_id),
      calib_(std::move(calib))
{
    // A camera with weaker contrast (e.g. a dimmer/steeper third view) can set
    // its own LAB-diff threshold in its calibration file; otherwise the
    // detector default applies.
    if (calib_.diff_threshold > 0.f) diff_threshold_ = calib_.diff_threshold;
}

namespace {

/// Polygon of the board outer edge (with mm @p margin) reprojected to image.
cv::Mat buildRoiMask(const BoardCalibration& calib, const cv::Size& sz,
                     float margin_mm = 25.f)
{
    cv::Mat mask = cv::Mat::zeros(sz, CV_8U);
    if (!calib.isValid()) {
        mask.setTo(255);
        return mask;
    }
    const float r = board::DOUBLE_OUTER + margin_mm;
    std::vector<cv::Point> poly;
    poly.reserve(72);
    for (int i = 0; i < 72; ++i) {
        const float a = i * (2.f * static_cast<float>(M_PI) / 72.f);
        const cv::Point2f bp{r * std::sin(a), r * std::cos(a)};
        const auto ip = calib.boardToImage(bp);
        poly.emplace_back(cvRound(ip.x), cvRound(ip.y));
    }
    const cv::Point* p = poly.data();
    const int        n = static_cast<int>(poly.size());
    cv::fillPoly(mask, &p, &n, 1, cv::Scalar(255));
    return mask;
}

} // anon

void DartDetector::refreshBackground()
{
    bg_lab_.release();
    bg_acc_.release();
    warmup_count_ = 0;
    warmup_done_  = false;
    reset();
}

void DartDetector::reset()
{
    stable_frames_     = 0;
    quiet_frames_      = 0;
    has_candidate_     = false;
    emitted_           = false;
    last_tip_pixel_    = {};
    last_viz_          = {};
    logged_tips_px_.clear();
    clean_frames_      = 0;
    human_seen_        = false;
    consecutive_small_ = 0;
    artifact_frames_   = 0;
}

bool DartDetector::boardLooksCleared() const
{
    return warmup_done_ && clean_frames_ > CLEAN_FRAMES_FOR_RESET;
}

std::optional<DartHit> DartDetector::processFrame(const cv::Mat& frame,
                                                  double         timestamp)
{
    if (frame.empty()) return std::nullopt;
    const cv::Mat lab = toLab32(frame);
    last_viz_.timestamp = timestamp;   // always update; UI relies on it

    // ── Warmup: accumulate mean background ──────────────────────────────────
    if (!warmup_done_) {
        if (bg_acc_.empty()) bg_acc_ = cv::Mat::zeros(lab.size(), CV_32FC3);
        cv::add(bg_acc_, lab, bg_acc_);
        if (++warmup_count_ >= WARMUP_FRAMES_REQUIRED) {
            bg_lab_ = bg_acc_ / static_cast<float>(warmup_count_);
            warmup_done_ = true;
            bg_acc_.release();
        }
        last_viz_.timestamp     = timestamp;
        last_viz_.has_detection = false;
        last_viz_.state         = DetectorState::Warmup;
        return std::nullopt;
    }

    // ── Build ROI mask lazily on first frame ───────────────────────────────
    if (roi_mask_.empty() || roi_mask_.size() != frame.size())
        roi_mask_ = buildRoiMask(calib_, frame.size());

    // ── Diff: blurred LAB distance, thresholded ────────────────────────────
    cv::Mat dist = labDistance(lab, bg_lab_);
    cv::Mat dist8;
    dist.convertTo(dist8, CV_8U, 1.0, 0.0);
    cv::GaussianBlur(dist8, dist8, {5, 5}, 0);

    cv::Mat mask;
    cv::threshold(dist8, mask, diff_threshold_, 255, cv::THRESH_BINARY);

    // Clean: open removes speckle, close fills the dart shaft holes
    const cv::Mat ker3 = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    const cv::Mat ker7 = cv::getStructuringElement(cv::MORPH_RECT, {7, 7});
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  ker3);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, ker7);

    // Expose for debug UI (no ROI clip — board surround is uniform colour, so
    // any dart that lands a bit outside the wire still gets caught).
    last_viz_.mask           = mask.clone();
    last_viz_.logged_tips_px = logged_tips_px_;

    // ── Find contours once (reused for gating + candidate picking) ─────────
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

    // Lighting/artifact discriminator: a real object (human, dart) changes the
    // CHROMA (a*, b*) of the region; a lighting drift mostly shifts LUMA (L).
    // sqrt(da^2 + db^2) < CHROMA_DIFF_THRESH ⇒ artifact, not a real object.
    auto isLightingArtifact = [&](const std::vector<cv::Point>& c) -> bool {
        if (c.empty()) return false;
        cv::Mat reg = cv::Mat::zeros(mask.size(), CV_8U);
        std::vector<std::vector<cv::Point>> v{c};
        cv::drawContours(reg, v, 0, cv::Scalar(255), -1);
        if (cv::countNonZero(reg) < 100) return false;
        const cv::Scalar cur = cv::mean(lab,     reg);
        const cv::Scalar bg  = cv::mean(bg_lab_, reg);
        const double da = cur[1] - bg[1];
        const double db = cur[2] - bg[2];
        return std::sqrt(da*da + db*db) < CHROMA_DIFF_THRESH;
    };

    // Solidity helper: cv::contourArea(c) / cv::contourArea(convexHull(c))
    // ~1.0 for a coherent blob (human), ~0.2-0.4 for scattered specks.
    auto solidity = [](const std::vector<cv::Point>& c) -> float {
        if (c.size() < 3) return 0.f;
        std::vector<cv::Point> hull;
        cv::convexHull(c, hull);
        const float ha = static_cast<float>(cv::contourArea(hull));
        return ha > 0.f ? static_cast<float>(cv::contourArea(c)) / ha : 0.f;
    };

    // Human gating: big + non-elongated + chroma diff + SOLID (continuous mass).
    bool huge_now = false;
    for (const auto& c : contours) {
        const float area = static_cast<float>(cv::contourArea(c));
        if (area < HUGE_CONTOUR_AREA) continue;
        cv::RotatedRect r = cv::minAreaRect(c);
        const float w = std::max(r.size.width, r.size.height);
        const float h = std::min(r.size.width, r.size.height);
        const float aspect = (h > 0.f) ? w / h : 0.f;
        if (aspect >= HUMAN_MAX_ASPECT)         continue;
        if (isLightingArtifact(c))              continue;
        if (solidity(c) < HUMAN_MIN_SOLIDITY)   continue;   // ← speckles, not human
        huge_now = true;
        break;
    }

    // Watchdog: residual foreground that is all artifact AND no round is
    // in progress AND no elongated blob anywhere → snap bg.  The elongated
    // check is critical: a dark dart on a dark sector has low chroma diff
    // and would otherwise be classified as artifact, which would absorb it
    // into the bg.
    {
        bool any_real_big        = false;
        bool any_elongated_blob  = false;
        for (const auto& c : contours) {
            const float area = static_cast<float>(cv::contourArea(c));
            if (area < 400.f) continue;
            cv::RotatedRect r = cv::minAreaRect(c);
            const float w = std::max(r.size.width, r.size.height);
            const float h = std::min(r.size.width, r.size.height);
            const float aspect = (h > 0.f) ? w / h : 0.f;
            if (aspect >= MIN_ASPECT_RATIO) {
                any_elongated_blob = true;
                any_real_big       = true;
                break;
            }
            if (area < 800.f) continue;
            if (!isLightingArtifact(c)) { any_real_big = true; break; }
        }
        const int fg_count = cv::countNonZero(mask);
        if (fg_count > CLEAN_FG_PIXELS && !any_real_big) ++artifact_frames_;
        else                                              artifact_frames_ = 0;

        if (artifact_frames_ > ARTIFACT_SNAP_FRAMES
            && logged_tips_px_.empty()
            && !any_elongated_blob) {
            lab.copyTo(bg_lab_);
            artifact_frames_ = 0;
        }
    }

    if (huge_now) {
        human_seen_             = true;
        consecutive_small_      = 0;
        has_candidate_          = false;
        stable_frames_          = 0;
        clean_frames_           = 0;
        last_viz_.has_detection = false;
        last_viz_.state         = DetectorState::HumanBlob;
        return std::nullopt;
    }

    // ── Just-exited: wait until mask is REALLY quiet before validating ─────
    // Avoids false clears when the human is mid-grab and the blob shrinks
    // momentarily but is still very much present.
    if (human_seen_) {
        const int fg_now = cv::countNonZero(mask);
        if (fg_now < POST_HUMAN_QUIET_FG_PIXELS) ++consecutive_small_;
        else                                      consecutive_small_ = 0;

        if (consecutive_small_ < POST_HUMAN_QUIET_FRAMES) {
            last_viz_.has_detection = false;
            last_viz_.state         = DetectorState::HumanBlob;
            return std::nullopt;
        }

        // A tip is "still there" only if backed by a real (solid, sized)
        // contour nearby — speckles around the location don't count.
        auto tipBackedByRealBlob = [&](const cv::Point2f& tip) -> bool {
            for (const auto& c : contours) {
                const float area = static_cast<float>(cv::contourArea(c));
                if (area < TIP_BACKING_MIN_AREA) continue;
                cv::Rect bb = cv::boundingRect(c);
                bb.x      -= cvRound(TIP_BACKING_SEARCH_PX);
                bb.y      -= cvRound(TIP_BACKING_SEARCH_PX);
                bb.width  += 2 * cvRound(TIP_BACKING_SEARCH_PX);
                bb.height += 2 * cvRound(TIP_BACKING_SEARCH_PX);
                if (!bb.contains(cv::Point(cvRound(tip.x), cvRound(tip.y))))
                    continue;
                if (solidity(c) >= TIP_BACKING_MIN_SOLIDITY) return true;
            }
            return false;
        };

        int still_present = 0;
        for (const auto& tip : logged_tips_px_)
            if (tipBackedByRealBlob(tip)) ++still_present;
        const int needed = static_cast<int>(logged_tips_px_.size()) / 2 + 1;
        const bool darts_gone = logged_tips_px_.empty() ||
                                still_present < needed;

        if (darts_gone) {
            lab.copyTo(bg_lab_);
            logged_tips_px_.clear();
            stable_frames_          = 0;
            has_candidate_          = false;
            human_seen_             = false;
            consecutive_small_      = 0;
            clean_frames_           = CLEAN_FRAMES_FOR_RESET + 1;
            last_viz_.has_detection = false;
            last_viz_.state         = DetectorState::BoardClean;
            last_viz_.logged_tips_px.clear();
            return std::nullopt;
        }
        // Logged darts still match the mask → false alarm, resume.
        human_seen_        = false;
        consecutive_small_ = 0;
    }

    // ── Adaptive bg update: blend current LAB into bg wherever mask == 0
    // AND not near any logged tip (extra safety: never erode confirmed darts).
    {
        cv::Mat update_mask;
        cv::bitwise_not(mask, update_mask);   // start: background regions
        for (const auto& tip : logged_tips_px_) {
            cv::circle(update_mask, tip, 20, cv::Scalar(0), -1);
        }
        cv::accumulateWeighted(lab, bg_lab_, BG_UPDATE_ALPHA, update_mask);
    }

    const int fg_pixels = cv::countNonZero(mask);
    if (fg_pixels < CLEAN_FG_PIXELS) ++clean_frames_;
    else                              clean_frames_ = 0;

    constexpr float LOGGED_MATCH_PX = 18.f;
    const std::vector<cv::Point>* best = nullptr;
    float                         best_area = 0.f;
    Endpoints                     best_ep{};
    cv::Point2f                   best_tip{}, best_board_xy{};
    cv::Point2f                   best_dir{};

    for (const auto& c : contours) {
        const float area = static_cast<float>(cv::contourArea(c));
        if (area < MIN_BLOB_AREA || area > MAX_BLOB_AREA) continue;
        cv::RotatedRect r = cv::minAreaRect(c);
        const float w = std::max(r.size.width, r.size.height);
        const float h = std::min(r.size.width, r.size.height);
        const float aspect = (h > 0.f) ? w / h : 0.f;
        if (aspect < MIN_ASPECT_RATIO) continue;

        const Endpoints ep = lineExtendByMask(c, mask, line_merge_perp_px_);
        const cv::Point2f a_board = calib_.imageToBoard(ep.a);
        const cv::Point2f b_board = calib_.imageToBoard(ep.b);
        const float a_r2 = a_board.x*a_board.x + a_board.y*a_board.y;
        const float b_r2 = b_board.x*b_board.x + b_board.y*b_board.y;
        const bool a_is_tip = a_r2 < b_r2;
        const cv::Point2f tip      = a_is_tip ? ep.a     : ep.b;
        const cv::Point2f board_xy = a_is_tip ? a_board  : b_board;
        cv::Point2f dir = a_is_tip ? ep.dir : -ep.dir;

        const float r_mm = std::sqrt(board_xy.x*board_xy.x + board_xy.y*board_xy.y);
        if (r_mm > MAX_BOARD_RADIUS_MM) continue;

        // Skip if this tip matches an already-logged dart
        bool is_logged = false;
        for (const auto& lt : logged_tips_px_) {
            const cv::Point2f d = tip - lt;
            if (d.x*d.x + d.y*d.y < LOGGED_MATCH_PX*LOGGED_MATCH_PX) {
                is_logged = true; break;
            }
        }
        if (is_logged) continue;

        if (area > best_area) {
            best          = &c;
            best_area     = area;
            best_ep       = ep;
            best_tip      = tip;
            best_board_xy = board_xy;
            best_dir      = dir;
        }
    }

    last_viz_.timestamp     = timestamp;
    last_viz_.has_detection = false;
    last_viz_.state         = boardLooksCleared() ? DetectorState::BoardClean
                                                  : DetectorState::Normal;

    if (!best) {
        ++quiet_frames_;
        has_candidate_ = false;
        stable_frames_ = 0;
        return std::nullopt;
    }
    quiet_frames_ = 0;
    last_viz_.state = DetectorState::Normal;

    // Tail = the other endpoint
    const cv::Point2f tail_pixel = (best_tip == best_ep.a) ? best_ep.b : best_ep.a;

    // Live viz
    last_viz_.contour       = *best;
    last_viz_.tip_pixel     = best_tip;
    last_viz_.tail_pixel    = tail_pixel;
    last_viz_.dart_dir      = best_dir;
    last_viz_.board_xy      = best_board_xy;
    last_viz_.has_detection = true;

    // Stability tracking
    if (has_candidate_) {
        const cv::Point2f d = best_tip - last_tip_pixel_;
        const float movement = std::sqrt(d.x*d.x + d.y*d.y);
        if (movement < TIP_STABILITY_PX) ++stable_frames_;
        else                              stable_frames_ = 1;
    } else {
        stable_frames_ = 1;
    }
    last_tip_pixel_ = best_tip;
    has_candidate_  = true;

    if (stable_frames_ < STABLE_FRAMES_REQUIRED) return std::nullopt;

    // Emit + log this dart so the next frame finds the NEXT dart.  ALWAYS
    // record the tip — otherwise a dart seen after the list is full is never
    // suppressed and re-emits every STABLE_FRAMES_REQUIRED frames.  Cap is a
    // memory bound (oldest drops); the "3 darts per round" limit lives in
    // Pipeline, not here.
    logged_tips_px_.push_back(best_tip);
    if (static_cast<int>(logged_tips_px_.size()) > MAX_LOGGED_TIPS)
        logged_tips_px_.erase(logged_tips_px_.begin());
    stable_frames_ = 0;
    has_candidate_ = false;

    const float r_mm = std::sqrt(best_board_xy.x*best_board_xy.x +
                                 best_board_xy.y*best_board_xy.y);
    const ZoneResult zr = ZoneMapper::lookup(best_board_xy);
    DartHit hit{};
    hit.cam_id     = cam_id_;
    hit.board_xy   = best_board_xy;
    hit.zone       = zr.label;
    hit.score      = zr.value;
    hit.confidence = std::clamp(1.f - r_mm / (2.f * board::DOUBLE_OUTER),
                                0.3f, 1.f);
    hit.timestamp  = timestamp;
    return hit;
}

} // namespace camdetect
