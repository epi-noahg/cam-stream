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

/// Endpoints of a dart's axis plus approximate cross-section widths.  Widths
/// are in pixels and let the caller decide which end is the (narrower) tip,
/// independent of the dart's pose on the board.
struct Endpoints {
    cv::Point2f a, b, dir;
    float       width_a   {0.f};   // cross-section near `a`
    float       width_b   {0.f};   // cross-section near `b`
    float       width_mid {0.f};   // cross-section near the dart's middle
};

/// Expand `seed_bb` along `dir` by `axial_pad` pixels on each side, and by
/// `perp_tol_px + 4` perpendicular.  Used so the axis sweep sees fragments
/// that sit collinear with the seed but well beyond its bounding rect.
cv::Rect extendBboxAlongAxis(const cv::Rect& seed_bb,
                             const cv::Point2f& dir,
                             float perp_tol_px,
                             const cv::Size& image_sz,
                             float axial_pad)
{
    const int pad_x = static_cast<int>(std::ceil(std::abs(dir.x) * axial_pad)) +
                      static_cast<int>(std::ceil(perp_tol_px)) + 4;
    const int pad_y = static_cast<int>(std::ceil(std::abs(dir.y) * axial_pad)) +
                      static_cast<int>(std::ceil(perp_tol_px)) + 4;
    cv::Rect bb;
    bb.x      = std::max(0, seed_bb.x - pad_x);
    bb.y      = std::max(0, seed_bb.y - pad_y);
    bb.width  = std::min(image_sz.width  - bb.x, seed_bb.width  + 2 * pad_x);
    bb.height = std::min(image_sz.height - bb.y, seed_bb.height + 2 * pad_y);
    return bb;
}

/// Extract a dart's axis by sampling perpendicular cross-section centroids
/// along its principal direction.  The midpoints of those slices lie on the
/// dart's geometric axis (independent of contour noise on the edges); fitting
/// a line through them gives a sharper direction than fitLine on the contour.
/// The tip is then the last in-mask pixel walking along the refined axis past
/// the extreme midpoint, with small mask gaps tolerated so a fragmented blob
/// (black shaft over black sector) still extends to its true tip.
///
/// @p perp_tol_px sets the half-width band around the rough axis used for
/// inclusion — also user-tunable via the line_merge_px slider.
Endpoints dartAxisByMidpoints(const std::vector<cv::Point>& seed_contour,
                              const cv::Mat&                full_mask,
                              float                         perp_tol_px)
{
    Endpoints r{};
    if (seed_contour.empty()) return r;

    // ── 1. Rough axis from the contour outline ─────────────────────────────
    cv::Vec4f line0;
    cv::fitLine(seed_contour, line0, cv::DIST_L2, 0, 0.01, 0.01);
    const cv::Point2f dir0   {line0[0], line0[1]};
    const cv::Point2f origin0{line0[2], line0[3]};
    const cv::Point2f perp0  {-dir0.y, dir0.x};

    // ── 2. Search region: extended along the seed's axis ───────────────────
    // We pad generously along ±dir0 so collinear fragments well past the
    // seed contour (e.g. a tip blob detached from the shaft by a black
    // sector gap of 30-80 px) are still reachable by the band sweep.  120 px
    // floor; otherwise 3× the seed's longest side.
    const cv::Rect seed_bb = cv::boundingRect(seed_contour);
    const float axial_pad = std::max(
        120.f,
        std::max(static_cast<float>(seed_bb.width),
                 static_cast<float>(seed_bb.height)) * 3.f);
    const cv::Rect bb = extendBboxAlongAxis(seed_bb, dir0, perp_tol_px,
                                            full_mask.size(), axial_pad);

    // ── 3. Compute axis range of all in-band mask pixels ───────────────────
    float t_min =  std::numeric_limits<float>::max();
    float t_max = -std::numeric_limits<float>::max();
    for (int y = bb.y; y < bb.y + bb.height; ++y) {
        const uint8_t* row = full_mask.ptr<uint8_t>(y);
        for (int x = bb.x; x < bb.x + bb.width; ++x) {
            if (!row[x]) continue;
            const float dx = x - origin0.x;
            const float dy = y - origin0.y;
            if (std::abs(dx * perp0.x + dy * perp0.y) > perp_tol_px) continue;
            const float t = dx * dir0.x + dy * dir0.y;
            if (t < t_min) t_min = t;
            if (t > t_max) t_max = t;
        }
    }
    if (!(t_min < t_max)) {
        // Fall back to contour-only range.
        for (const auto& p : seed_contour) {
            const float t = (p.x - origin0.x) * dir0.x +
                            (p.y - origin0.y) * dir0.y;
            t_min = std::min(t_min, t);
            t_max = std::max(t_max, t);
        }
        r.a   = origin0 + dir0 * t_min;
        r.b   = origin0 + dir0 * t_max;
        r.dir = dir0;
        return r;
    }

    // ── 4. Bin into N slices, accumulate per-slice centroid (the midpoints) ─
    constexpr int N_BINS = 24;
    const float bin_w = (t_max - t_min) / static_cast<float>(N_BINS);
    if (bin_w < 0.5f) {
        r.a   = origin0 + dir0 * t_min;
        r.b   = origin0 + dir0 * t_max;
        r.dir = dir0;
        return r;
    }
    std::array<double, N_BINS> sx{}, sy{};
    std::array<int,    N_BINS> cnt{};
    for (int y = bb.y; y < bb.y + bb.height; ++y) {
        const uint8_t* row = full_mask.ptr<uint8_t>(y);
        for (int x = bb.x; x < bb.x + bb.width; ++x) {
            if (!row[x]) continue;
            const float dx = x - origin0.x;
            const float dy = y - origin0.y;
            if (std::abs(dx * perp0.x + dy * perp0.y) > perp_tol_px) continue;
            const float t = dx * dir0.x + dy * dir0.y;
            int bi = static_cast<int>((t - t_min) / bin_w);
            if (bi < 0)        bi = 0;
            if (bi >= N_BINS)  bi = N_BINS - 1;
            sx[bi]  += x;
            sy[bi]  += y;
            cnt[bi] += 1;
        }
    }
    std::vector<cv::Point2f> midpts;
    std::vector<int>         midpts_bin;     // which bin each midpoint came from
    midpts.reserve(N_BINS);
    midpts_bin.reserve(N_BINS);
    for (int b = 0; b < N_BINS; ++b) {
        if (cnt[b] < 2) continue;
        midpts.emplace_back(static_cast<float>(sx[b] / cnt[b]),
                            static_cast<float>(sy[b] / cnt[b]));
        midpts_bin.push_back(b);
    }

    // Cross-section widths: bin pixel count divided by bin axis length is an
    // approximation of the perpendicular extent.  We average over a couple of
    // bins at each end to dampen single-bin noise.
    auto avgWidthAtEnd = [&](bool first_end) -> float {
        constexpr int K = 2;
        double sum = 0.0;
        int    n   = 0;
        if (first_end) {
            for (int b = 0; b < N_BINS && n < K; ++b)
                if (cnt[b] > 0) { sum += cnt[b]; ++n; }
        } else {
            for (int b = N_BINS - 1; b >= 0 && n < K; --b)
                if (cnt[b] > 0) { sum += cnt[b]; ++n; }
        }
        if (n == 0) return 0.f;
        return static_cast<float>(sum / n / std::max(0.5f, bin_w));
    };
    const float width_a = avgWidthAtEnd(true);
    const float width_b = avgWidthAtEnd(false);
    float width_mid = 0.f;
    {
        double sum = 0.0;
        int    n   = 0;
        for (int off = -2; off <= 2; ++off) {
            const int b = N_BINS / 2 + off;
            if (b < 0 || b >= N_BINS) continue;
            if (cnt[b] > 0) { sum += cnt[b]; ++n; }
        }
        if (n > 0) width_mid = static_cast<float>(sum / n /
                                                  std::max(0.5f, bin_w));
    }

    // ── 5. Refit line through cross-section midpoints ──────────────────────
    cv::Point2f dir1, origin1;
    if (midpts.size() >= 3) {
        cv::Vec4f line1;
        cv::fitLine(midpts, line1, cv::DIST_L2, 0, 0.01, 0.01);
        dir1    = {line1[0], line1[1]};
        origin1 = {line1[2], line1[3]};
    } else {
        dir1    = dir0;
        origin1 = origin0;
    }

    // ── 6. Find absolute axis range of all in-band fg pixels along refined
    // axis.  No walk-to-edge with gap limit — fragments separated by ANY
    // amount of dark sector are naturally included as long as they fall
    // within perp_tol of the refined line.  This is what makes the tip
    // estimate honest on fragmented black-on-black darts: the endpoint is
    // the FARTHEST in-band pixel, not the end of the closest fragment.
    const cv::Point2f perp1{-dir1.y, dir1.x};
    float final_t_min =  std::numeric_limits<float>::max();
    float final_t_max = -std::numeric_limits<float>::max();
    for (int y = bb.y; y < bb.y + bb.height; ++y) {
        const uint8_t* row = full_mask.ptr<uint8_t>(y);
        for (int x = bb.x; x < bb.x + bb.width; ++x) {
            if (!row[x]) continue;
            const float dx = x - origin1.x;
            const float dy = y - origin1.y;
            if (std::abs(dx * perp1.x + dy * perp1.y) > perp_tol_px) continue;
            const float t = dx * dir1.x + dy * dir1.y;
            if (t < final_t_min) final_t_min = t;
            if (t > final_t_max) final_t_max = t;
        }
    }
    if (!(final_t_min < final_t_max)) {
        // No in-band fg found along refined axis — fall back to midpoint
        // range so the caller still gets meaningful endpoints.
        if (midpts.empty()) {
            final_t_min = t_min;
            final_t_max = t_max;
        } else {
            final_t_min =  std::numeric_limits<float>::max();
            final_t_max = -std::numeric_limits<float>::max();
            for (const auto& m : midpts) {
                const float t = (m.x - origin1.x) * dir1.x +
                                (m.y - origin1.y) * dir1.y;
                final_t_min = std::min(final_t_min, t);
                final_t_max = std::max(final_t_max, t);
            }
        }
    }

    r.a         = origin1 + dir1 * final_t_min;
    r.b         = origin1 + dir1 * final_t_max;
    r.dir       = dir1;
    r.width_a   = width_a;
    r.width_b   = width_b;
    r.width_mid = width_mid;
    return r;
}

/// Build a binary mask of every foreground pixel that belongs to the dart:
///   1) the original seed contour drawn filled (catches wide flights / tail
///      that sit OUTSIDE the perp band around the axis), AND
///   2) every fg pixel inside the axis band [a..b] within perp_tol of the
///      refined line (catches fragments separated by dark sectors and glued
///      back to the shaft by the line-extend logic).
///
/// The output covers the whole hitbox — used by the debug UI to draw the
/// bounding box AND by the detector to burn the dart into the background so
/// the next throw is analysed in isolation.
void fillDartRegion(cv::Mat&        out,
                    const cv::Mat&  full_mask,
                    const Endpoints& ep,
                    float           perp_tol_px,
                    const std::vector<cv::Point>* seed_contour = nullptr)
{
    if (full_mask.empty()) return;
    if (out.size() != full_mask.size() || out.type() != CV_8U)
        out = cv::Mat::zeros(full_mask.size(), CV_8U);
    else
        out.setTo(0);

    // 1) Seed contour as a filled region — catches the wide flights.
    if (seed_contour && !seed_contour->empty()) {
        std::vector<std::vector<cv::Point>> v{*seed_contour};
        cv::drawContours(out, v, 0, cv::Scalar(255), -1);
    }

    // Search bbox is extended along the dart axis so collinear fragments
    // (tip blob detached by a dark sector) are picked up too.
    const cv::Rect seed_bb = seed_contour && !seed_contour->empty()
                              ? cv::boundingRect(*seed_contour)
                              : cv::boundingRect(
                                  std::vector<cv::Point>{
                                      {cvRound(ep.a.x), cvRound(ep.a.y)},
                                      {cvRound(ep.b.x), cvRound(ep.b.y)}});
    const float axial_pad = std::max(
        120.f,
        std::max(static_cast<float>(seed_bb.width),
                 static_cast<float>(seed_bb.height)) * 3.f);
    const cv::Rect scan_bb = extendBboxAlongAxis(seed_bb, ep.dir, perp_tol_px,
                                                  full_mask.size(), axial_pad);

    const cv::Point2f a       = ep.a;
    const cv::Point2f dir     = ep.dir;
    const cv::Point2f perp{-dir.y, dir.x};
    const float       total_l = static_cast<float>(cv::norm(ep.b - ep.a));
    if (total_l <= 0.f) return;

    for (int y = scan_bb.y; y < scan_bb.y + scan_bb.height; ++y) {
        const uint8_t* mrow = full_mask.ptr<uint8_t>(y);
        uint8_t*       orow = out.ptr<uint8_t>(y);
        for (int x = scan_bb.x; x < scan_bb.x + scan_bb.width; ++x) {
            if (!mrow[x]) continue;
            const float dx = x - a.x;
            const float dy = y - a.y;
            const float t  = dx * dir.x + dy * dir.y;
            if (t < 0.f || t > total_l) continue;
            const float p  = dx * perp.x + dy * perp.y;
            if (std::abs(p) > perp_tol_px) continue;
            orow[x] = 255;
        }
    }
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
    throw_bg_lab_.release();
    bg_acc_.release();
    warmup_count_ = 0;
    warmup_done_  = false;
    reset();
}

void DartDetector::reset()
{
    stable_frames_           = 0;
    quiet_frames_            = 0;
    has_candidate_           = false;
    emitted_                 = false;
    last_tip_pixel_          = {};
    last_viz_                = {};
    logged_tips_px_.clear();
    clean_frames_            = 0;
    human_seen_              = false;
    consecutive_small_       = 0;
    artifact_frames_         = 0;
    prev_big_centroid_       = {};
    has_prev_big_centroid_   = false;
    static_big_frames_       = 0;
    throw_bg_lab_.release();
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

    // ── Dual-reference diff ────────────────────────────────────────────────
    //
    // `bg_lab_` tracks the "empty board" via slow adaptive update.  We use it
    // for HUMAN / BoardClean reasoning so that:
    //   - a player walking in is detected as a big chroma-divergent blob,
    //   - the board reads as "clean" again only when the diff really returns
    //     to ~0 (i.e. all darts removed and lighting close to the empty ref).
    //
    // `throw_bg_lab_` is snapped at every commit so that NEW-dart detection
    // sees the board exactly as it was just after the previous commit — old
    // darts (and their plumes / fragments) sit at zero diff and don't
    // pollute the mask.  Empty until the first commit of the round.
    const cv::Mat ker3 = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    const cv::Mat ker7 = cv::getStructuringElement(cv::MORPH_RECT, {7, 7});

    auto buildMask = [&](const cv::Mat& reference) {
        cv::Mat dist = labDistance(lab, reference);
        cv::Mat dist8;
        dist.convertTo(dist8, CV_8U, 1.0, 0.0);
        cv::GaussianBlur(dist8, dist8, {5, 5}, 0);
        cv::Mat m;
        cv::threshold(dist8, m, diff_threshold_, 255, cv::THRESH_BINARY);
        cv::morphologyEx(m, m, cv::MORPH_OPEN,  ker3);
        cv::morphologyEx(m, m, cv::MORPH_CLOSE, ker7);
        return m;
    };

    const bool use_throw_bg = !throw_bg_lab_.empty();
    cv::Mat mask       = buildMask(use_throw_bg ? throw_bg_lab_ : bg_lab_);
    cv::Mat human_mask = use_throw_bg ? buildMask(bg_lab_) : mask;

    // The user-facing MASK view shows the dart-detection mask: that's the
    // cleaner per-throw view, and it's what determines BBs and tip picks.
    last_viz_.mask           = mask.clone();
    last_viz_.logged_tips_px = logged_tips_px_;

    // ── Find contours once for each mask ───────────────────────────────────
    // dart_contours drive NEW-DART logic (candidate selection, BB viz union).
    // human_contours drive HUMAN/CLEAN logic (huge-blob check, artifact
    // watchdog, post-human darts-still-there check).
    std::vector<std::vector<cv::Point>> contours;        // = dart contours
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

    std::vector<std::vector<cv::Point>> human_contours_storage;
    const std::vector<std::vector<cv::Point>>* human_contours_ptr = &contours;
    if (use_throw_bg) {
        cv::findContours(human_mask, human_contours_storage,
                         cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
        human_contours_ptr = &human_contours_storage;
    }
    const auto& human_contours = *human_contours_ptr;

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
    // Iterate human_contours so we see the player even when throw_bg_lab_
    // suppresses already-committed darts from the dart mask.
    bool huge_now = false;
    const std::vector<cv::Point>* huge_contour = nullptr;
    for (const auto& c : human_contours) {
        const float area = static_cast<float>(cv::contourArea(c));
        if (area < HUGE_CONTOUR_AREA) continue;
        cv::RotatedRect r = cv::minAreaRect(c);
        const float w = std::max(r.size.width, r.size.height);
        const float h = std::min(r.size.width, r.size.height);
        const float aspect = (h > 0.f) ? w / h : 0.f;
        if (aspect >= HUMAN_MAX_ASPECT)         continue;
        if (isLightingArtifact(c))              continue;
        if (solidity(c) < HUMAN_MIN_SOLIDITY)   continue;   // ← speckles, not human
        huge_now     = true;
        huge_contour = &c;
        break;
    }

    // Motion check: a real human's centroid always wobbles a few pixels frame
    // to frame; a static blob that survives chroma + solidity gating is almost
    // certainly residual lighting drift the bg model hasn't caught up with.
    // We track the huge-contour centroid and demote "huge_now" to false when
    // it's been pixel-stable for too long.
    if (huge_now && huge_contour) {
        const cv::Moments m = cv::moments(*huge_contour);
        if (m.m00 > 0.0) {
            const cv::Point2f c{static_cast<float>(m.m10 / m.m00),
                                static_cast<float>(m.m01 / m.m00)};
            if (has_prev_big_centroid_) {
                const cv::Point2f d = c - prev_big_centroid_;
                const float move = std::sqrt(d.x*d.x + d.y*d.y);
                if (move < STATIC_BLOB_MOTION_PX) ++static_big_frames_;
                else                              static_big_frames_ = 0;
            }
            prev_big_centroid_     = c;
            has_prev_big_centroid_ = true;
        }
        if (static_big_frames_ > STATIC_BLOB_FRAMES_REQ) {
            huge_now = false;
            // Snap bg if no committed darts to protect.  After a long stretch
            // (2× the threshold), force the snap regardless — the round_hits
            // are already captured upstream in Pipeline, so clearing the
            // local logged_tips here only forfeits dart-re-emit suppression.
            if (logged_tips_px_.empty()) {
                lab.copyTo(bg_lab_);
                static_big_frames_ = 0;
            } else if (static_big_frames_ > STATIC_BLOB_FRAMES_REQ * 2) {
                lab.copyTo(bg_lab_);
                logged_tips_px_.clear();
                static_big_frames_ = 0;
            }
        }
    } else {
        // No huge candidate this frame → reset the motion tracker.
        has_prev_big_centroid_ = false;
        static_big_frames_     = 0;
    }

    // Watchdog: residual foreground that is all artifact AND no round is
    // in progress AND no elongated blob anywhere → snap bg.  The elongated
    // check is critical: a dark dart on a dark sector has low chroma diff
    // and would otherwise be classified as artifact, which would absorb it
    // into the bg.
    {
        bool any_real_big        = false;
        bool any_elongated_blob  = false;
        for (const auto& c : human_contours) {
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
        const int fg_count = cv::countNonZero(human_mask);
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

    // ── Just-exited: gate on whether a HUMAN-SHAPED coherent blob is still
    // present, NOT on total foreground pixel count.  After collection, bg
    // drift can leave a speckle storm across the whole rim that keeps
    // total-fg high indefinitely — but if no single contour is human-sized,
    // there's clearly no person standing there.
    if (human_seen_) {
        float largest_now = 0.f;
        for (const auto& c : human_contours)
            largest_now = std::max(largest_now,
                                   static_cast<float>(cv::contourArea(c)));
        const bool human_blob_present = (largest_now > HUMAN_PRESENT_AREA);
        if (human_blob_present) consecutive_small_ = 0;
        else                    ++consecutive_small_;

        if (consecutive_small_ < POST_HUMAN_QUIET_FRAMES) {
            last_viz_.has_detection = false;
            last_viz_.state         = DetectorState::HumanBlob;
            return std::nullopt;
        }

        // A tip is "still there" only if backed by a real (solid, sized)
        // contour nearby — speckles around the location don't count.  Use
        // human_contours: the dart mask hides committed darts (they're in
        // throw_bg), but they're real foreground against bg_lab_, so
        // human_contours is where their shape actually exists.
        auto tipBackedByRealBlob = [&](const cv::Point2f& tip) -> bool {
            for (const auto& c : human_contours) {
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
            throw_bg_lab_.release();           // next round detects against the fresh bg
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
        // Logged darts still match the mask → false alarm, resume.  Re-snap
        // throw_bg_lab_ so the post-human reference reflects the CURRENT
        // board state (lighting may have drifted while the human was here).
        if (!logged_tips_px_.empty()) lab.copyTo(throw_bg_lab_);
        human_seen_        = false;
        consecutive_small_ = 0;
    }

    // ── Adaptive bg updates ────────────────────────────────────────────────
    //
    // bg_lab_ tracks the EMPTY-BOARD reference: gate it on human_mask so
    // pixels covered by darts or a hand don't pollute the "empty" model.
    //
    // throw_bg_lab_ tracks the POST-COMMIT reference: gate it on the dart
    // mask (anything NEW since the snap shouldn't be absorbed).
    {
        cv::Mat update_mask;
        cv::bitwise_not(human_mask, update_mask);
        for (const auto& tip : logged_tips_px_) {
            cv::circle(update_mask, tip, 20, cv::Scalar(0), -1);
        }
        cv::accumulateWeighted(lab, bg_lab_, BG_UPDATE_ALPHA, update_mask);
    }
    if (!throw_bg_lab_.empty()) {
        cv::Mat update_mask;
        cv::bitwise_not(mask, update_mask);
        cv::accumulateWeighted(lab, throw_bg_lab_, BG_UPDATE_ALPHA, update_mask);
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

    // Union dart-band region across ALL elongated contours (including darts
    // we've already logged) so the debug UI can draw a hitbox around every
    // visible dart, not just the newest one.
    cv::Mat region_union = cv::Mat::zeros(mask.size(), CV_8U);
    cv::Mat scratch_region;

    for (const auto& c : contours) {
        const float area = static_cast<float>(cv::contourArea(c));
        if (area < MIN_BLOB_AREA || area > MAX_BLOB_AREA) continue;
        cv::RotatedRect r = cv::minAreaRect(c);
        const float w = std::max(r.size.width, r.size.height);
        const float h = std::min(r.size.width, r.size.height);
        const float aspect = (h > 0.f) ? w / h : 0.f;
        if (aspect < MIN_ASPECT_RATIO) continue;

        const Endpoints ep = dartAxisByMidpoints(c, mask, line_merge_perp_px_);

        // Visualise every elongated candidate — even already-logged ones —
        // so the user can see the hitbox of every dart on the board.  Pass
        // the seed contour so the flights are included, not just the band.
        fillDartRegion(scratch_region, mask, ep, line_merge_perp_px_, &c);
        if (!scratch_region.empty() &&
            scratch_region.size() == region_union.size())
            cv::bitwise_or(region_union, scratch_region, region_union);

        const cv::Point2f a_board = calib_.imageToBoard(ep.a);
        const cv::Point2f b_board = calib_.imageToBoard(ep.b);
        const float a_r2 = a_board.x*a_board.x + a_board.y*a_board.y;
        const float b_r2 = b_board.x*b_board.x + b_board.y*b_board.y;

        // Tip = the narrower end (dart point is sharp, tail with flights is
        // wide).  Only use width when both ends measured and the difference
        // is meaningful — otherwise fall back to "endpoint closer to board
        // centre".
        bool a_is_tip;
        if (ep.width_a > 0.f && ep.width_b > 0.f &&
            std::abs(ep.width_a - ep.width_b) >
            0.10f * std::max(ep.width_a, ep.width_b)) {
            a_is_tip = ep.width_a < ep.width_b;
        } else {
            a_is_tip = a_r2 < b_r2;
        }
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
    last_viz_.dart_region   = region_union;   // covers logged + new candidates

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

    // ── Snap the ENTIRE board into throw_bg_lab_ ──────────────────────────
    // The whole-frame snap goes to throw_bg_lab_ (used only for new-dart
    // detection), NOT bg_lab_ (used for HUMAN / BoardClean against the
    // empty-board reference).  Keeps the clean-cycle intact: when the
    // player removes the darts at end-of-round, bg_lab_ still represents
    // the empty board, so the mask returns to ~empty and BoardClean fires
    // correctly.  Old darts are invisible to dart detection because they
    // live in throw_bg_lab_; new darts pop up clean and isolated.
    lab.copyTo(throw_bg_lab_);
    artifact_frames_       = 0;
    static_big_frames_     = 0;
    has_prev_big_centroid_ = false;

    // ── Confidence: based on dart-shape quality, NOT board radius ─────────
    // The old `1 - r/2R` formula systematically rewarded cams whose tip
    // projected nearer to the bullseye — even when the tip was wrong (e.g.
    // wrong endpoint picked, or dart still mid-flight with a long streak).
    //
    //   aspect_q   ∝ length / mid-width — penalises blob-like masks
    //   tip_sharp  ∝ 1 - tip-width / mid-width — penalises blunt or noisy ends
    //
    // A real dart at rest sits at aspect ~6-12 and tip much narrower than mid
    // → near 1.0.  A mid-flight or fragment-shaped mask → much lower.
    const float length  = static_cast<float>(cv::norm(best_ep.a - best_ep.b));
    const float w_mid   = std::max(1.f, best_ep.width_mid);
    const float aspect  = length / w_mid;
    const bool  a_was_tip = (best_tip.x == best_ep.a.x &&
                             best_tip.y == best_ep.a.y);
    const float tip_w   = a_was_tip ? best_ep.width_a : best_ep.width_b;
    const float aspect_q   = std::clamp(aspect / 8.f,            0.3f, 1.f);
    const float tip_sharp  = std::clamp(1.f - tip_w / w_mid,     0.3f, 1.f);

    // Pixel-accurate zone map beats the homography projection when present:
    // the label is read at the tip pixel itself, so wires / fisheye are
    // already accounted for.
    const ZoneResult zr = zone_map_.empty()
        ? ZoneMapper::lookup(best_board_xy)
        : zone_map_.lookup(best_tip);
    DartHit hit{};
    hit.cam_id     = cam_id_;
    hit.board_xy   = best_board_xy;
    hit.zone       = zr.label;
    hit.score      = zr.value;
    hit.confidence = std::clamp(aspect_q * tip_sharp, 0.3f, 1.f);
    hit.timestamp  = timestamp;
    return hit;
}

} // namespace camdetect
