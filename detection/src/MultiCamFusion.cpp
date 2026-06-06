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
    int best_seed = -1, best_count = 0;
    for (int a = 0; a < n; ++a) {
        int count = 0;
        for (int b = 0; b < n; ++b)
            if (dist(pending_[idx[a]]->board_xy,
                     pending_[idx[b]]->board_xy) <= AGREEMENT_RADIUS_MM)
                ++count;
        if (count > best_count) { best_count = count; best_seed = a; }
    }

    if (best_count < MIN_CAMS_FOR_CONFIRM) {  // cams disagree → phantom, drop
        reset();
        return std::nullopt;
    }

    // Build the agreeing cluster around the densest seed.
    std::array<bool, NUM_CAMS> in_cluster{};
    cv::Point2f sum{0.f, 0.f};
    float       conf_sum = 0.f;
    int         votes = 0;
    for (int b = 0; b < n; ++b) {
        if (dist(pending_[idx[best_seed]]->board_xy,
                 pending_[idx[b]]->board_xy) > AGREEMENT_RADIUS_MM) continue;
        in_cluster[idx[b]] = true;
        sum      += pending_[idx[b]]->board_xy;
        conf_sum += pending_[idx[b]]->confidence;
        ++votes;
    }

    const cv::Point2f centroid{sum.x / votes, sum.y / votes};

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

    const ZoneResult zr = ZoneMapper::lookup(centroid);
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
