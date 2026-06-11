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

    // Per-cam positional weight: inverse variance.  Used to combine AGREEING
    // estimates within a cluster — there, a cam whose sigma says "I can place
    // this tip within 1 mm" rightly outweighs a 5 mm cam 25:1.
    auto posWeight = [](const DartHit& h) {
        const float s = std::max(h.sigma_mm, 0.5f);
        return 1.f / (s * s);
    };

    // Off-board skepticism.  Tip mis-picks (axis overshoot, shadows, flights,
    // wrong-end selection) systematically land OUTWARD, past the rim; the
    // opposite error has no mechanism.  So when at least one camera puts the
    // dart ON the board, a competing off-board story is almost certainly a
    // bad tip and is heavily discounted.  A genuine miss — no cam sees it
    // on-board — keeps its full weight.
    auto offBoard = [](const DartHit& h) {
        return std::sqrt(h.board_xy.x * h.board_xy.x +
                         h.board_xy.y * h.board_xy.y) > board::DOUBLE_OUTER;
    };
    bool any_onboard = false;
    for (int a = 0; a < n; ++a)
        if (!offBoard(*pending_[idx[a]])) any_onboard = true;

    constexpr float OFFBOARD_DOUBT = 0.15f;

    // Detection trust: is this a real dart tip, correctly picked?
    //
    // Primary signal: silhouette quality.  Whether a detection is REAL has
    // nothing to do with how close it landed to a wire, so the zone-margin
    // confidence must NOT dominate here: a dart in a narrow band (triple/
    // double ring) always has a small margin and would lose every tie-break
    // against a junk detection that happens to sit mid-single.
    //
    // Secondary signal (half-weight, floored at 0.5): the label reliability
    // erf(margin/sigma·√2).  When two junk-grade silhouettes duel, the one
    // whose own story is self-consistent (label safe at its claimed spot)
    // is the better bet than one teetering on a boundary.
    auto seedTrust = [&](const DartHit& h) {
        const float rel = std::erf(
            h.zone_margin_mm /
            (std::max(h.sigma_mm, 0.5f) * static_cast<float>(M_SQRT2)));
        float w = h.shape_q * (0.5f + 0.5f * rel);
        if (any_onboard && offBoard(h)) w *= OFFBOARD_DOUBT;
        return w;
    };

    // Label weight: given the detection is real, how likely is its zone
    // label?  That's the margin/sigma confidence — used in the zone vote.
    auto voteWeight = [&](const DartHit& h) {
        float w = h.confidence;
        if (any_onboard && offBoard(h)) w *= OFFBOARD_DOUBT;
        return w;
    };

    // Largest spatially-coherent cluster: for each vote count how many votes
    // (incl. itself) lie within AGREEMENT_RADIUS_MM; the densest seeds it.
    // Ties broken by the summed detection TRUST of the cluster: when cameras
    // tell contradictory stories we must pick the one most likely to be a
    // real dart, and a spurious detection (shadow, fragment) can be both
    // pixel-precise and clear of wires while being plain wrong.
    int   best_seed   = -1;
    int   best_count  = 0;
    float best_seed_w = -1.f;
    for (int a = 0; a < n; ++a) {
        int   count = 0;
        float w_sum = 0.f;
        for (int b = 0; b < n; ++b) {
            if (dist(pending_[idx[a]]->board_xy,
                     pending_[idx[b]]->board_xy) > AGREEMENT_RADIUS_MM)
                continue;
            ++count;
            w_sum += seedTrust(*pending_[idx[b]]);
        }
        if (count > best_count ||
            (count == best_count && w_sum > best_seed_w)) {
            best_count  = count;
            best_seed   = a;
            best_seed_w = w_sum;
        }
    }

    if (best_count < MIN_CAMS_FOR_CONFIRM) {  // nothing usable, drop
        reset();
        return std::nullopt;
    }

    // ── Fused position: inverse-variance weighted mean over the cluster ────
    std::array<bool, NUM_CAMS> in_cluster{};
    cv::Point2f weighted_sum{0.f, 0.f};
    float       w_sum = 0.f;
    for (int b = 0; b < n; ++b) {
        if (dist(pending_[idx[best_seed]]->board_xy,
                 pending_[idx[b]]->board_xy) > AGREEMENT_RADIUS_MM) continue;
        in_cluster[idx[b]] = true;
        const float w = posWeight(*pending_[idx[b]]);
        weighted_sum.x += pending_[idx[b]]->board_xy.x * w;
        weighted_sum.y += pending_[idx[b]]->board_xy.y * w;
        w_sum          += w;
    }
    const cv::Point2f centroid{weighted_sum.x / w_sum, weighted_sum.y / w_sum};

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

    // Fused sigma: 1/√Σw is the textbook combined uncertainty when the cams'
    // errors are independent and honestly modelled.  When the observed spread
    // exceeds what the sigmas predict, the model is too optimistic somewhere
    // (calibration bias, wrong tip) — trust the evidence and inflate.
    const float sigma_fused = std::max(1.f / std::sqrt(w_sum), spread * 0.5f);

    // ── Zone: probability-weighted vote ────────────────────────────────────
    // Every cluster cam votes for its pixel-accurate label with its
    // voteWeight (= confidence — P(label correct), wires, lens distortion and
    // view angle priced in — times off-board doubt).  The fused centroid
    // votes too, through the
    // geometric lookup, weighted by ITS chance of being right — high when
    // the cams agree tightly and the centroid sits clear of any boundary —
    // and discounted because the homography projection ignores the real
    // wires.  One confident well-placed camera therefore beats both a shaky
    // camera AND a centroid that was dragged near a wire by parallax.
    struct Cand {
        ZoneResult zr;
        float      weight   {0.f};   // Σ vote weights for this label
        float      p_none   {1.f};   // Π(1-pᵢ): chance ALL its voters are wrong
    };
    std::array<Cand, NUM_CAMS + 1> cands{};
    int n_cands = 0;
    auto addVote = [&](const ZoneResult& zr, float p) {
        p = std::clamp(p, 0.f, 0.97f);
        for (int i = 0; i < n_cands; ++i) {
            if (cands[i].zr.label != zr.label) continue;
            cands[i].weight += p;
            cands[i].p_none *= (1.f - p);
            return;
        }
        cands[n_cands++] = {zr, p, 1.f - p};
    };

    for (int i = 0; i < NUM_CAMS; ++i)
        if (in_cluster[i])
            addVote({pending_[i]->zone, pending_[i]->score},
                    voteWeight(*pending_[i]));

    {
        constexpr float CENTROID_DISCOUNT = 0.75f;
        const ZoneResult zr_c     = ZoneMapper::lookup(centroid);
        const float      margin_c = ZoneMapper::boundaryMarginMM(centroid);
        const float      p_c      = std::erf(
            margin_c / (std::max(sigma_fused, 0.5f) *
                        static_cast<float>(M_SQRT2)));
        addVote(zr_c, p_c * CENTROID_DISCOUNT);
    }

    const Cand* winner  = &cands[0];
    float       total_w = 0.f;
    for (int i = 0; i < n_cands; ++i) {
        total_w += cands[i].weight;
        if (cands[i].weight > winner->weight) winner = &cands[i];
    }

    // ── Fused confidence ───────────────────────────────────────────────────
    //   p_right   = 1 - Π(1-pᵢ): at least one of the winning label's voters
    //               is correct — grows with each agreeing strong voter.
    //   consensus = winner's share of all vote mass — collapses when the
    //               cams split between labels.
    const float p_right   = 1.f - winner->p_none;
    const float consensus = total_w > 1e-6f ? winner->weight / total_w : 0.f;

    FusedHit f{};
    f.zone       = winner->zr.label;
    f.score      = winner->zr.value;
    f.board_xy   = centroid;
    f.sigma_mm   = sigma_fused;
    f.confidence = std::clamp(p_right * consensus, 0.f, 1.f);
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
