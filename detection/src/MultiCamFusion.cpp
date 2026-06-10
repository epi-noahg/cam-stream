#include "camdetect/MultiCamFusion.hpp"
#include "camdetect/ZoneMapper.hpp"

#include <algorithm>
#include <cmath>

namespace camdetect {

namespace {
float dist(const cv::Point2f& a, const cv::Point2f& b)
{
    const cv::Point2f d = a - b;
    return std::sqrt(d.x * d.x + d.y * d.y);
}
} // anon

MultiCamFusion::MultiCamFusion(double window_seconds)
    : window_seconds_(window_seconds) {}

std::optional<FusedHit> MultiCamFusion::addHit(const DartHit& hit)
{
    if (hit.cam_id < 0 || hit.cam_id >= NUM_CAMS) return std::nullopt;

    if (first_ts_ < 0.0) first_ts_ = hit.timestamp;
    pending_[hit.cam_id] = hit;

    int votes = 0;
    for (const auto& p : pending_) if (p) ++votes;

    // Fast path: every camera has voted — no reason to wait out the window.
    if (votes == NUM_CAMS) return confirm();
    return std::nullopt;
}

std::optional<FusedHit> MultiCamFusion::tick(double now)
{
    if (first_ts_ < 0.0)                   return std::nullopt;
    if (now - first_ts_ < window_seconds_) return std::nullopt;
    return confirm();
}

std::optional<FusedHit> MultiCamFusion::flush()
{
    if (first_ts_ < 0.0) return std::nullopt;
    return confirm();
}

std::optional<FusedHit> MultiCamFusion::confirm()
{
    // Indices of the cameras that voted in this window.
    std::array<int, NUM_CAMS> idx{};
    int n = 0;
    for (int i = 0; i < NUM_CAMS; ++i)
        if (pending_[i]) idx[n++] = i;

    if (n < MIN_CAMS_FOR_CONFIRM) {   // lone camera → unconfirmed, drop
        reset();
        return std::nullopt;
    }

    // Largest spatially-coherent cluster: for each vote count how many votes
    // (incl. itself) lie within AGREEMENT_RADIUS_MM; the densest seeds it.
    // Ties broken by per-vote confidence (relevant when only 1 cam voted, or
    // when multiple cams disagree spatially — we still want the best guess).
    int   best_seed       = -1;
    int   best_count      = 0;
    float best_seed_conf  = -1.f;
    for (int a = 0; a < n; ++a) {
        int count = 0;
        for (int b = 0; b < n; ++b)
            if (dist(pending_[idx[a]]->board_xy,
                     pending_[idx[b]]->board_xy) <= AGREEMENT_RADIUS_MM)
                ++count;
        const float conf_a = pending_[idx[a]]->confidence;
        if (count > best_count ||
            (count == best_count && conf_a > best_seed_conf)) {
            best_count     = count;
            best_seed      = a;
            best_seed_conf = conf_a;
        }
    }

    if (best_count < MIN_CAMS_FOR_CONFIRM) {  // nothing usable, drop
        reset();
        return std::nullopt;
    }

    // Build the agreeing cluster around the densest seed.
    //
    // Centroid is CONFIDENCE-WEIGHTED: a cam looking perpendicular to the
    // dart axis sees a longer, sharper shape (higher aspect_q × tip_sharp),
    // so its position estimate should dominate over a cam seeing the dart
    // end-on with a shorter, blurrier silhouette.  Plain averaging treats all
    // cluster members equally and gets pulled toward poor-angle cams.
    std::array<bool, NUM_CAMS> in_cluster{};
    cv::Point2f weighted_sum{0.f, 0.f};
    cv::Point2f unweighted_sum{0.f, 0.f};
    float       conf_sum = 0.f;
    int         votes = 0;
    for (int b = 0; b < n; ++b) {
        if (dist(pending_[idx[best_seed]]->board_xy,
                 pending_[idx[b]]->board_xy) > AGREEMENT_RADIUS_MM) continue;
        in_cluster[idx[b]] = true;
        const float c = pending_[idx[b]]->confidence;
        weighted_sum.x  += pending_[idx[b]]->board_xy.x * c;
        weighted_sum.y  += pending_[idx[b]]->board_xy.y * c;
        unweighted_sum  += pending_[idx[b]]->board_xy;
        conf_sum        += c;
        ++votes;
    }

    const cv::Point2f centroid = (conf_sum > 1e-3f)
        ? cv::Point2f{weighted_sum.x / conf_sum, weighted_sum.y / conf_sum}
        : cv::Point2f{unweighted_sum.x / votes,  unweighted_sum.y / votes};

    // Max pairwise spread within the cluster (parallax / agreement indicator).
    float spread = 0.f;
    for (int a = 0; a < NUM_CAMS; ++a) {
        if (!in_cluster[a]) continue;
        for (int b = a + 1; b < NUM_CAMS; ++b) {
            if (!in_cluster[b]) continue;
            spread = std::max(spread,
                              dist(pending_[a]->board_xy, pending_[b]->board_xy));
        }
    }

    // Zone: majority vote of the per-cam labels first.  With pixel-accurate
    // ZoneMaps each cam's label was read at its own tip pixel, so two cams
    // agreeing on "T20" beats the homography-projected mm centroid (which
    // ignores wires and lens distortion).  No majority → fall back to the
    // geometric lookup on the fused centroid.
    ZoneResult zr{"", 0};
    {
        int   best_votes = 0;
        float best_conf  = -1.f;
        for (int a = 0; a < NUM_CAMS; ++a) {
            if (!in_cluster[a]) continue;
            int   nv = 0;
            float cv_sum = 0.f;
            for (int b = 0; b < NUM_CAMS; ++b) {
                if (!in_cluster[b]) continue;
                if (pending_[b]->zone != pending_[a]->zone) continue;
                ++nv;
                cv_sum += pending_[b]->confidence;
            }
            if (nv > best_votes || (nv == best_votes && cv_sum > best_conf)) {
                best_votes = nv;
                best_conf  = cv_sum;
                zr = {pending_[a]->zone, pending_[a]->score};
            }
        }
        if (best_votes < 2) zr = ZoneMapper::lookup(centroid);
    }
    const float base_conf       = conf_sum / votes;
    const float agreement_decay = std::exp(-spread / 15.f);  // 15mm = soft knee
    const float count_factor    = std::sqrt(votes / float(NUM_CAMS));

    FusedHit f{};
    f.zone       = zr.label;
    f.score      = zr.value;
    f.confidence = std::clamp(base_conf * agreement_decay * count_factor,
                              0.f, 1.f);
    f.timestamp  = first_ts_;
    for (int i = 0; i < NUM_CAMS; ++i)
        if (in_cluster[i]) f.per_cam[i] = *pending_[i];

    reset();
    return f;
}

void MultiCamFusion::reset()
{
    for (auto& p : pending_) p.reset();
    first_ts_ = -1.0;
}

} // namespace camdetect
