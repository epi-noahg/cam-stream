#include "camdetect/DartDetector.hpp"
#include "camdetect/ZoneMapper.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace camdetect {

namespace {

bool traceEnabled()
{
    static const bool on = [] {
        const char* v = std::getenv("CAMDETECT_TRACE");
        return v && v[0] && v[0] != '0';
    }();
    return on;
}

#define CAMTRACE(...) do { if (traceEnabled()) { \
    std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } } while (0)

/// CAMDETECT_PERF=1 prints coarse throughput / latency stats to stdout: a
/// per-camera processed-fps line (~1/s, from DetectionService) and a per-emit
/// settle line (here).  Env-gated, cached — zero cost when unset.
bool perfEnabled()
{
    static const bool on = [] {
        const char* v = std::getenv("CAMDETECT_PERF");
        return v && v[0] && v[0] != '0';
    }();
    return on;
}

/// CAMDETECT_DUMP=<dir> writes an annotated crop of every emitted hit there:
/// mask edges (green), claimed dart region (blue), axis (yellow), tip (red).
const char* dumpDir()
{
    static const char* dir = std::getenv("CAMDETECT_DUMP");
    return (dir && dir[0]) ? dir : nullptr;
}

void dumpEmit(const cv::Mat& frame, const cv::Mat& mask,
              const cv::Mat& region, const cv::Point2f& a,
              const cv::Point2f& b, const cv::Point2f& tip,
              int cam_id, double timestamp, const std::string& zone)
{
    const char* dir = dumpDir();
    if (!dir) return;
    cv::Mat vis = frame.clone();
    if (!region.empty()) vis.setTo(cv::Scalar(255, 120, 0), region);
    cv::Mat edges;
    cv::morphologyEx(mask, edges, cv::MORPH_GRADIENT,
                     cv::getStructuringElement(cv::MORPH_RECT, {3, 3}));
    vis.setTo(cv::Scalar(0, 255, 0), edges);
    cv::line(vis, a, b, {0, 255, 255}, 1, cv::LINE_AA);
    cv::circle(vis, tip, 6, {0, 0, 255}, 2, cv::LINE_AA);

    cv::Rect roi(cvRound(tip.x) - 220, cvRound(tip.y) - 220, 440, 440);
    roi &= cv::Rect(0, 0, vis.cols, vis.rows);
    char path[512];
    std::snprintf(path, sizeof(path), "%s/f%04d_cam%d_%s.png", dir,
                  static_cast<int>(timestamp * 30 + 0.5), cam_id,
                  zone.c_str());
    if (!roi.empty()) cv::imwrite(path, vis(roi));
}

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
    /// RMS perpendicular residual (px) of the cross-section midpoints to the
    /// fitted axis line.  Small for a real (straight, collinear) dart, large
    /// for an occluded / forked / blob-like silhouette.  -1 when not measured
    /// (too few midpoints).  A band-independent silhouette-straightness signal.
    float       residual_rms {-1.f};
    /// Cross-section midpoints in IMAGE space (the points the axis line was fit
    /// through).  Mapped to board space by the caller to estimate the dart
    /// line's angular uncertainty (slope standard error) for the fusion
    /// across-axis covariance.  Empty when too few slices were available.
    std::vector<cv::Point2f> midpts_img;
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
    float       residual_rms = -1.f;
    if (midpts.size() >= 3) {
        cv::Vec4f line1;
        cv::fitLine(midpts, line1, cv::DIST_L2, 0, 0.01, 0.01);
        dir1    = {line1[0], line1[1]};
        origin1 = {line1[2], line1[3]};
        // RMS perpendicular residual of the midpoints to the fitted axis —
        // a real dart's slice centres are collinear (small), a fork/blob's
        // are not.  Band-independent silhouette-straightness signal.
        const cv::Point2f perpf{-dir1.y, dir1.x};
        double s2 = 0.0;
        for (const auto& m : midpts) {
            const float d = (m.x - origin1.x) * perpf.x +
                            (m.y - origin1.y) * perpf.y;
            s2 += static_cast<double>(d) * d;
        }
        residual_rms = static_cast<float>(
            std::sqrt(s2 / static_cast<double>(midpts.size())));
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
    r.width_mid    = width_mid;
    r.residual_rms = residual_rms;
    r.midpts_img   = std::move(midpts);
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
    // Reused every frame (mask cleanup + the emit-time refit); build once.
    ker3_ = cv::getStructuringElement(cv::MORPH_RECT, {3, 3});
    ker7_ = cv::getStructuringElement(cv::MORPH_RECT, {7, 7});
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
    candidate_gap_           = 0;
    quiet_frames_            = 0;
    stable_since_ts_         = 0.0;
    quiet_since_ts_          = 0.0;
    candidate_since_ts_      = 0.0;
    jitter_sq_sum_           = 0.f;
    jitter_n_                = 0;
    has_candidate_           = false;
    dist_acc_.release();
    dist_acc_n_              = 0;
    emitted_                 = false;
    last_tip_pixel_          = {};
    last_viz_                = {};
    logged_tips_px_.clear();
    committed_regions_.release();
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

bool DartDetector::cumSupportValid() const
{
    return warmup_done_ && cum_mask_clean_ && !latest_cum_mask_.empty();
}

bool DartDetector::hasForegroundNear(const cv::Point2f& px,
                                     float              radius_px) const
{
    if (latest_cum_mask_.empty()) return false;
    const int r = cvRound(radius_px);
    cv::Rect roi(cvRound(px.x) - r, cvRound(px.y) - r, 2 * r + 1, 2 * r + 1);
    roi &= cv::Rect(0, 0, latest_cum_mask_.cols, latest_cum_mask_.rows);
    if (roi.empty()) return false;
    // A handful of speckle pixels shouldn't count as a witness.
    return cv::countNonZero(latest_cum_mask_(roi)) >= 20;
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

    auto maskFromDist = [&](const cv::Mat& dist) {
        cv::Mat dist8;
        dist.convertTo(dist8, CV_8U, 1.0, 0.0);
        cv::GaussianBlur(dist8, dist8, {5, 5}, 0);
        cv::Mat m;
        cv::threshold(dist8, m, diff_threshold_, 255, cv::THRESH_BINARY);
        cv::morphologyEx(m, m, cv::MORPH_OPEN,  ker3_);
        cv::morphologyEx(m, m, cv::MORPH_CLOSE, ker7_);
        return m;
    };
    auto buildMask = [&](const cv::Mat& reference) {
        return maskFromDist(labDistance(lab, reference));
    };

    const bool use_throw_bg = !throw_bg_lab_.empty();
    // Retain the raw dart-reference distance: it is accumulated across the
    // stability window so the EMIT-time axis/tip can be re-fit on a temporally
    // denoised silhouette that recovers dark-on-dark tip pixels (below the
    // per-frame threshold but consistent across frames).
    const cv::Mat dart_dist = labDistance(lab,
                                  use_throw_bg ? throw_bg_lab_ : bg_lab_);
    cv::Mat mask       = maskFromDist(dart_dist);
    cv::Mat human_mask = use_throw_bg ? buildMask(bg_lab_) : mask;

    // Keep the cumulative mask queryable by peers (cross-cam support).
    // Flagged clean only once we know no human is occluding the board —
    // set further down, after the huge-blob gating has run.
    latest_cum_mask_ = human_mask;
    cum_mask_clean_  = false;

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
                         cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        human_contours_ptr = &human_contours_storage;
    }
    const auto& human_contours = *human_contours_ptr;

    // Lighting/artifact discriminator: a real object (human, dart) changes the
    // CHROMA (a*, b*) of the region; a lighting drift mostly shifts LUMA (L).
    // sqrt(da^2 + db^2) < CHROMA_DIFF_THRESH ⇒ artifact, not a real object.
    auto isLightingArtifact = [&](const std::vector<cv::Point>& c) -> bool {
        if (c.empty()) return false;
        // Scope to the contour's bounding box: the fill + the two cv::mean()
        // calls then cost ~ROI area, not full-frame area (this runs per large
        // contour while a player is in frame).
        cv::Rect bb = cv::boundingRect(c) & cv::Rect(0, 0, mask.cols, mask.rows);
        if (bb.empty()) return false;
        cv::Mat reg = cv::Mat::zeros(bb.size(), CV_8U);
        std::vector<std::vector<cv::Point>> v{c};
        cv::drawContours(reg, v, 0, cv::Scalar(255), -1, cv::LINE_8,
                         cv::noArray(), 0, -bb.tl());
        if (cv::countNonZero(reg) < 100) return false;
        const cv::Scalar cur = cv::mean(lab(bb),     reg);
        const cv::Scalar bg  = cv::mean(bg_lab_(bb), reg);
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
        if (huge_contour && (static_cast<int>(timestamp * 30) % 30) == 0)
            CAMTRACE("[trace] t=%.2f cam%d HUGE area=%.0f static=%d",
                     timestamp, cam_id_,
                     cv::contourArea(*huge_contour), static_big_frames_);
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
        // Only chroma-divergent blobs count as "human still here".  After
        // the player walks off, auto-exposure / shadow drift can leave a
        // luma-only blob well above HUMAN_PRESENT_AREA for many seconds —
        // without the artifact filter the detector waits forever and the
        // whole next round is thrown away blind.
        float largest_now = 0.f;
        for (const auto& c : human_contours) {
            const float area = static_cast<float>(cv::contourArea(c));
            if (area <= HUMAN_PRESENT_AREA) continue;
            if (isLightingArtifact(c))      continue;
            largest_now = std::max(largest_now, area);
        }
        const bool human_blob_present = (largest_now > HUMAN_PRESENT_AREA);
        if (human_blob_present) {
            consecutive_small_ = 0;
        } else {
            if (consecutive_small_ == 0) quiet_since_ts_ = timestamp;
            ++consecutive_small_;
        }

        if ((timestamp - quiet_since_ts_) < POST_HUMAN_QUIET_SECONDS ||
            consecutive_small_ < POST_HUMAN_QUIET_MIN_FRAMES) {
            if ((static_cast<int>(timestamp * 30) % 30) == 0)
                CAMTRACE("[trace] t=%.2f cam%d POST-HUMAN largest=%.0f small=%d",
                         timestamp, cam_id_, largest_now, consecutive_small_);
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
            committed_regions_.release();
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

    // No human in frame from here on — the cumulative mask is trustworthy.
    cum_mask_clean_ = true;

    // ── Adaptive bg updates ────────────────────────────────────────────────
    //
    // bg_lab_ tracks the EMPTY-BOARD reference: gate it on human_mask so
    // pixels covered by darts or a hand don't pollute the "empty" model.
    //
    // throw_bg_lab_ tracks the POST-COMMIT reference: gate it on the dart
    // mask (anything NEW since the snap shouldn't be absorbed).
    // Freeze both references while a dart candidate is being tracked: the
    // marginal sub-threshold tip/shaft pixels the temporal refit needs to
    // recover are, by definition, OUTSIDE the per-frame mask and so would be
    // ABSORBED into the reference over exactly the accumulation window
    // (~20% at α·N), monotonically weakening the very signal we sum.  The
    // window is ≤17 frames; adaptation resumes the next non-candidate frame.
    if (!has_candidate_) {
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
            cv::accumulateWeighted(lab, throw_bg_lab_, BG_UPDATE_ALPHA,
                                   update_mask);
        }
    }

    // Board-clean must be judged against the EMPTY-BOARD reference
    // (human_mask), never the per-throw mask: the latter goes blank right
    // after every commit (the dart is burned into throw_bg_lab_), which
    // would auto-reset the round after every single dart.  The cumulative
    // mask keeps every dart of the throw visible until they are physically
    // removed.
    const int fg_pixels = cv::countNonZero(human_mask);
    if (fg_pixels < CLEAN_FG_PIXELS) ++clean_frames_;
    else                              clean_frames_ = 0;
    last_viz_.fg_px_cumulative = fg_pixels;
    last_viz_.clean_frames     = clean_frames_;

    const std::vector<cv::Point>* best = nullptr;
    float                         best_area = 0.f;
    Endpoints                     best_ep{};
    cv::Point2f                   best_tip{}, best_board_xy{};
    cv::Point2f                   best_dir{};
    cv::Mat                       best_region;

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

        // Ghost check against the committed darts' individual regions.  A
        // tip-distance test can't work here: players aim at one spot, so a
        // real second dart can land 10-15 mm from the first.  What separates
        // a new dart from a ghost (committed dart settling / lighting shift)
        // is WHERE its mask pixels live: a ghost's region lies almost
        // entirely on top of the committed region, a new dart's is mostly
        // fresh material.
        if (!committed_regions_.empty()) {
            const int cand_px = cv::countNonZero(scratch_region);
            if (cand_px > 0) {
                cv::Mat overlap;
                cv::bitwise_and(scratch_region, committed_regions_, overlap);
                const float frac = static_cast<float>(cv::countNonZero(overlap)) /
                                   static_cast<float>(cand_px);
                if (frac > GHOST_OVERLAP_FRAC) continue;
            }
        }

        if (area > best_area) {
            best          = &c;
            best_area     = area;
            best_ep       = ep;
            best_tip      = tip;
            best_board_xy = board_xy;
            best_dir      = dir;
            best_region   = scratch_region.clone();
        }
    }

    last_viz_.timestamp     = timestamp;
    last_viz_.has_detection = false;
    last_viz_.state         = boardLooksCleared() ? DetectorState::BoardClean
                                                  : DetectorState::Normal;
    last_viz_.dart_region   = region_union;   // covers logged + new candidates

    if (!best) {
        ++quiet_frames_;
        // Keep the stability window alive through short mask dropouts.
        if (!has_candidate_ || ++candidate_gap_ > CANDIDATE_GAP_GRACE) {
            has_candidate_ = false;
            stable_frames_ = 0;
            candidate_gap_ = 0;
        }
        return std::nullopt;
    }
    quiet_frames_  = 0;
    candidate_gap_ = 0;
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

    // Stability tracking.  The accumulated per-frame tip movement over the
    // stable window doubles as a measured pixel-noise estimate for the
    // confidence model below.
    auto resetAccum = [&] { dist_acc_ = dart_dist.clone(); dist_acc_n_ = 1; };
    if (has_candidate_) {
        const cv::Point2f d = best_tip - last_tip_pixel_;
        const float movement = std::sqrt(d.x*d.x + d.y*d.y);
        if (movement < TIP_STABILITY_PX) {
            ++stable_frames_;
            jitter_sq_sum_ += movement * movement;
            ++jitter_n_;
            if (dist_acc_.empty() || dist_acc_.size() != dart_dist.size())
                resetAccum();
            else { dist_acc_ += dart_dist; ++dist_acc_n_; }
        } else {
            stable_frames_ = 1;
            stable_since_ts_ = timestamp;
            jitter_sq_sum_ = 0.f;
            jitter_n_      = 0;
            resetAccum();
        }
    } else {
        stable_frames_ = 1;
        stable_since_ts_    = timestamp;
        candidate_since_ts_ = timestamp;
        jitter_sq_sum_ = 0.f;
        jitter_n_      = 0;
        resetAccum();
    }
    last_tip_pixel_ = best_tip;
    has_candidate_  = true;

    if ((timestamp - stable_since_ts_) < STABLE_SECONDS_REQUIRED ||
        stable_frames_ < STABLE_MIN_FRAMES)
        return std::nullopt;
    // ── Temporal silhouette refit ───────────────────────────────────────────
    // Re-fit the axis & tip on the stability-window AVERAGE of the raw LAB
    // distance.  Averaging suppresses per-frame sensor noise (~√N), so a
    // modestly lower threshold recovers the temporally-consistent low-contrast
    // (dark-on-dark) tip/shaft pixels that single-frame thresholding drops —
    // extending the "farthest in-band pixel" tip out toward the true contact
    // point and undoing the inward radius bias.  Detection / stability / ghost
    // logic already ran on the per-frame mask; this only sharpens the EMITTED
    // geometry, and only when the refit agrees with the per-frame tip (sanity).
    if (dist_acc_n_ >= 4 && !dist_acc_.empty()) {
        cv::Mat avg = dist_acc_ * (1.0 / static_cast<double>(dist_acc_n_));
        cv::Mat ad8, amask;
        avg.convertTo(ad8, CV_8U);
        cv::GaussianBlur(ad8, ad8, {5, 5}, 0);
        cv::threshold(ad8, amask, diff_threshold_ * 0.75f, 255,
                      cv::THRESH_BINARY);
        cv::morphologyEx(amask, amask, cv::MORPH_OPEN,  ker3_);
        cv::morphologyEx(amask, amask, cv::MORPH_CLOSE, ker7_);

        std::vector<std::vector<cv::Point>> acont;
        cv::findContours(amask, acont, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_NONE);
        const std::vector<cv::Point>* seed = nullptr;
        double best_d2 = 1e18;
        for (const auto& c : acont) {
            if (cv::contourArea(c) < MIN_BLOB_AREA) continue;
            const double d  = cv::pointPolygonTest(c, best_tip, true);
            const double d2 = d >= 0.0 ? 0.0 : d * d;   // 0 when tip is inside
            if (d2 < best_d2) { best_d2 = d2; seed = &c; }
        }
        if (seed && best_d2 <= 25.0 * 25.0) {
            const Endpoints rep =
                dartAxisByMidpoints(*seed, amask, line_merge_perp_px_);
            if (cv::norm(rep.a - rep.b) > 1.f) {
                const cv::Point2f a_board = calib_.imageToBoard(rep.a);
                const cv::Point2f b_board = calib_.imageToBoard(rep.b);
                const float a_r2 = a_board.dot(a_board);
                const float b_r2 = b_board.dot(b_board);
                bool a_is_tip;
                if (rep.width_a > 0.f && rep.width_b > 0.f &&
                    std::abs(rep.width_a - rep.width_b) >
                        0.10f * std::max(rep.width_a, rep.width_b))
                    a_is_tip = rep.width_a < rep.width_b;
                else
                    a_is_tip = a_r2 < b_r2;
                const cv::Point2f new_tip = a_is_tip ? rep.a : rep.b;
                const cv::Point2f new_xy  = a_is_tip ? a_board : b_board;
                const float new_r = std::sqrt(new_xy.dot(new_xy));
                // Accept only a sane refinement near the per-frame tip.
                if (new_r <= MAX_BOARD_RADIUS_MM &&
                    cv::norm(new_tip - best_tip) <= 30.f) {
                    best_ep       = rep;
                    best_tip      = new_tip;
                    best_board_xy = new_xy;
                    best_dir      = a_is_tip ? rep.dir : -rep.dir;
                    // NOTE: deliberately do NOT rebuild best_region from the
                    // lower-threshold averaged silhouette — committed_regions_
                    // (ghost suppression) must stay on the TIGHT per-frame
                    // footprint, else a denser refit region overlaps a genuine
                    // nearby same-sector dart >GHOST_OVERLAP_FRAC and wrongly
                    // suppresses it.  The refit only sharpens tip/axis/zone.
                }
            }
        }
    }

    if (perfEnabled())
        std::fprintf(stdout, "[perf] cam%d emit settle=%.3fs frames=%d\n",
                     cam_id_, timestamp - candidate_since_ts_, stable_frames_);

    // Emit + log this dart so the next frame finds the NEXT dart.  ALWAYS
    // record the tip — otherwise a dart seen after the list is full is never
    // suppressed and re-emits every STABLE_FRAMES_REQUIRED frames.  Cap is a
    // memory bound (oldest drops); the "3 darts per round" limit lives in
    // Pipeline, not here.
    logged_tips_px_.push_back(best_tip);
    if (static_cast<int>(logged_tips_px_.size()) > MAX_LOGGED_TIPS)
        logged_tips_px_.erase(logged_tips_px_.begin());

    // Record this dart's individual region (dilated) so later frames can
    // tell its ghosts apart from a genuinely new dart landing next to it.
    if (!best_region.empty()) {
        cv::Mat dilated;
        const cv::Mat ker = cv::getStructuringElement(
            cv::MORPH_ELLIPSE,
            {2 * COMMITTED_REGION_DILATE_PX + 1,
             2 * COMMITTED_REGION_DILATE_PX + 1});
        cv::dilate(best_region, dilated, ker);
        if (committed_regions_.empty())
            committed_regions_ = dilated;
        else
            cv::bitwise_or(committed_regions_, dilated, committed_regions_);
    }
    stable_frames_ = 0;
    has_candidate_ = false;
    const float jitter_sq_sum = jitter_sq_sum_;   // consumed by the confidence
    const int   jitter_n      = jitter_n_;        // model below, then cleared
    jitter_sq_sum_ = 0.f;
    jitter_n_      = 0;

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

    // ── Confidence model ───────────────────────────────────────────────────
    // The per-cam confidence is an estimate of P(this cam's zone label is
    // correct), assembled from three independent observations:
    //
    //  1. SHAPE — is the mask actually dart-like?
    //       aspect_q  ∝ length / mid-width   (penalises blob-like masks)
    //       tip_sharp ∝ 1 - tip-width / mid-width  (penalises blunt ends —
    //                   typically a wrong-endpoint pick or an end-on view)
    //
    //  2. GEOMETRY — how well-placed is this camera for THIS board spot?
    //     The local homography Jacobian at the tip converts pixel error to
    //     board error: a head-on cam sits at ~0.5 mm/px everywhere, a
    //     grazing-angle cam can exceed 3 mm/px in its depth direction.
    //     Combined with the measured tip jitter this yields sigma_mm, the
    //     1-sigma positional uncertainty of board_xy.  Shape doubt inflates
    //     it further (a blobby mask means the tip itself is suspect, beyond
    //     frame-to-frame jitter).
    //
    //  3. MARGIN — how far is the tip from the nearest wire?  Pixel-accurate
    //     via the ZoneMap when available (lens distortion included),
    //     geometric otherwise.
    //
    // confidence = erf(margin / (sigma·√2)) — the probability that a Gaussian
    // positional error of sigma_mm leaves the zone label unchanged —
    // attenuated by shape_q.  A cam that sees the dart perfectly AND from a
    // good angle AND well clear of wires scores ~1; any weak link drags it
    // down proportionally.
    const float length  = static_cast<float>(cv::norm(best_ep.a - best_ep.b));
    const float w_mid   = std::max(1.f, best_ep.width_mid);
    const float aspect  = length / w_mid;
    // Silhouette quality from two BAND-INDEPENDENT signals (the old
    // tip-taper term measured width inside the ±perp_tol core where a dart
    // is uniform, so it always collapsed to its floor and pinned shape_q at
    // ~0.20 — see ARCH notes).  Now:
    //   aspect_q   elongation: length / mid-width (blobs score low).
    //   straight_q collinearity of the cross-section midpoints: a real dart's
    //              slice centres lie on a line, a fork / occlusion / blob's
    //              scatter.  Independent of the perp band.
    const float aspect_q   = std::clamp((aspect - 2.f) / 8.f, 0.10f, 1.f);
    const float straight_q = best_ep.residual_rms >= 0.f
        ? std::clamp(1.f - best_ep.residual_rms / 3.f, 0.15f, 1.f)
        : 0.5f;   // too few midpoints to judge → neutral
    const float shape_q    = std::clamp(aspect_q * straight_q, 0.05f, 1.f);

    // Local mm-per-pixel scale at the tip ({min, max} singular values).
    constexpr float DEFAULT_MM_PER_PX = 0.6f;   // ~uncalibrated fallback
    cv::Vec2f scale = calib_.localScaleMmPerPx(best_tip);
    if (scale[1] <= 0.f) scale = {DEFAULT_MM_PER_PX, DEFAULT_MM_PER_PX};

    // Measured tip noise: RMS frame-to-frame movement over the stable window
    // plus an irreducible localisation floor (mask quantisation, blur).
    constexpr float BASE_TIP_NOISE_PX = 1.5f;
    const float jitter_px =
        jitter_n > 0 ? std::sqrt(jitter_sq_sum / jitter_n) : 0.f;
    const float sigma_mm =
        scale[1] * (BASE_TIP_NOISE_PX + jitter_px) * (2.f - shape_q);

    // Pixel-accurate zone map beats the homography projection when present:
    // the label is read at the tip pixel itself, so wires / fisheye are
    // already accounted for.  Same source for the margin; px→mm uses the
    // conservative (min) scale.
    ZoneResult zr;
    float      zone_margin_mm;
    if (!zone_map_.empty()) {
        zr             = zone_map_.lookup(best_tip);
        zone_margin_mm = zone_map_.boundaryDistancePx(best_tip) * scale[0];
    } else {
        zr             = ZoneMapper::lookup(best_board_xy);
        zone_margin_mm = ZoneMapper::boundaryMarginMM(best_board_xy);
    }

    const float z = zone_margin_mm /
                    (std::max(sigma_mm, 0.5f) * static_cast<float>(M_SQRT2));
    const float zone_reliability = std::erf(z);

    // ── Anisotropic error model ────────────────────────────────────────────
    // Across the dart's image axis the tip is pinned by the axis fit
    // (~BASE_TIP_NOISE_PX); along it, mask truncation can slide the estimate
    // tens of pixels up the shaft, the more so the worse the silhouette.
    // Propagate both directions through the local homography Jacobian.
    cv::Point2f hit_axis_board{0.f, 1.f};
    float       hit_sigma_along_mm  = 25.f;
    float       hit_sigma_across_mm = 3.f;
    {
        cv::Point2f dir_img = best_dir;
        const float n = std::sqrt(dir_img.x*dir_img.x + dir_img.y*dir_img.y);
        if (n > 1e-6f) dir_img *= 1.f / n;
        const cv::Point2f perp_img{-dir_img.y, dir_img.x};

        const cv::Matx22f J = calib_.localJacobian(best_tip);
        const cv::Vec2f along_b  = J * cv::Vec2f(dir_img.x,  dir_img.y);
        const cv::Vec2f across_b = J * cv::Vec2f(perp_img.x, perp_img.y);
        const float along_scale  = std::sqrt(along_b.dot(along_b));
        const float across_scale = std::sqrt(across_b.dot(across_b));

        constexpr float ALONG_BASE_PX  = 12.f;  // irreducible truncation risk
        constexpr float ALONG_SHAPE_PX = 40.f;  // worst-case extra when blobby
        const float sigma_along_px =
            ALONG_BASE_PX + jitter_px + (1.f - shape_q) * ALONG_SHAPE_PX;
        const float sigma_across_px = BASE_TIP_NOISE_PX + jitter_px;

        // A homography maps a straight image line to a straight board line, so
        // the Jacobian tangent at the tip is ALREADY exactly along that board
        // line — i.e. the true tip→tail chord direction (verified: 0.000° vs
        // the full chord).  Crossing these board lines across cameras therefore
        // recovers the 3D contact point exactly; the along-axis tip slide only
        // moves each board_xy along its own line, never the crossing.
        if (along_scale > 1e-6f)
            hit_axis_board = {along_b[0] / along_scale,
                              along_b[1] / along_scale};

        hit_sigma_along_mm  = std::max(2.f,
            (along_scale  > 0.f ? along_scale  : scale[1]) * sigma_along_px);

        // Base across error: the axis-fit lateral noise at the tip, floored at
        // the residual calibration bias (crossing two lines amplifies offsets).
        const float across_base_mm = std::max(2.5f,
            (across_scale > 0.f ? across_scale : scale[0]) * sigma_across_px);

        // ── Angular (line-orientation) uncertainty → across error ───────────
        // A short / foreshortened / fragmented silhouette pins the tip ACROSS
        // its axis but leaves the axis ANGLE poorly determined.  That angular
        // error, levered from the fit CENTROID out to the contact point (~half
        // the shaft away), is a real across displacement of the board line at
        // the tip — exactly what corrupts the multi-cam crossing for grazing
        // far-rim darts.  Estimate the board-space slope standard error from the
        // cross-section midpoints mapped through the homography (so the
        // anisotropic px→mm scaling is handled exactly): sigma_phi =
        // sqrt(resid² + base²)·sqrt(12/N)/span, with the sqrt(12/N) slope-SE
        // factor so a sparse 2-3-point fit reads honestly uncertain instead of
        // falsely collinear.  Add lever·sigma_phi in quadrature.
        float across_extra_mm = 12.f;   // N<3 → angle unconstrained → loose
        const int Nmp = static_cast<int>(best_ep.midpts_img.size());
        if (Nmp >= 3 && calib_.isValid()) {
            std::vector<cv::Point2f> mb;
            cv::perspectiveTransform(best_ep.midpts_img, mb,
                                     calib_.homography_img_to_board);
            cv::Point2f c{0.f, 0.f};
            for (const auto& p : mb) c += p;
            c *= 1.f / static_cast<float>(Nmp);
            const cv::Point2f perp_b{-hit_axis_board.y, hit_axis_board.x};
            double s2 = 0.0; float tmin = 1e9f, tmax = -1e9f;
            for (const auto& p : mb) {
                const cv::Point2f d = p - c;
                const float resid = d.x*perp_b.x + d.y*perp_b.y;
                s2 += static_cast<double>(resid) * resid;
                const float t = d.x*hit_axis_board.x + d.y*hit_axis_board.y;
                tmin = std::min(tmin, t); tmax = std::max(tmax, t);
            }
            const float rms_mm  = static_cast<float>(std::sqrt(s2 / Nmp));
            const float span_mm = std::max(5.f, tmax - tmin);
            constexpr float BASE_RESID_MM = 0.6f;   // irreducible board-fit noise
            const float sigma_phi = std::sqrt(rms_mm*rms_mm +
                                              BASE_RESID_MM*BASE_RESID_MM) *
                                    std::sqrt(12.f / static_cast<float>(Nmp)) /
                                    span_mm;                              // rad
            const float lever_mm = static_cast<float>(
                cv::norm(best_board_xy - c));   // fit centroid → contact (≈ tip)
            across_extra_mm = lever_mm * sigma_phi;
        }
        hit_sigma_across_mm = std::sqrt(across_base_mm * across_base_mm +
                                        across_extra_mm * across_extra_mm);
    }

    dumpEmit(frame, mask, best_region, best_ep.a, best_ep.b, best_tip,
             cam_id_, timestamp, zr.label);

    DartHit hit{};
    hit.cam_id          = cam_id_;
    hit.board_xy        = best_board_xy;
    hit.tip_pixel       = best_tip;
    hit.zone            = zr.label;
    hit.score           = zr.value;
    hit.sigma_mm        = sigma_mm;
    hit.zone_margin_mm  = zone_margin_mm;
    hit.shape_q         = shape_q;
    hit.view_q          = std::clamp(DEFAULT_MM_PER_PX / scale[1], 0.f, 1.f);
    hit.confidence      = std::clamp(zone_reliability * shape_q, 0.01f, 1.f);
    hit.timestamp       = timestamp;
    hit.axis_board      = hit_axis_board;
    hit.sigma_along_mm  = hit_sigma_along_mm;
    hit.sigma_across_mm = hit_sigma_across_mm;
    return hit;
}

} // namespace camdetect
