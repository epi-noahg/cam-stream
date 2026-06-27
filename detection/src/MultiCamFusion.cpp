#include "camdetect/MultiCamFusion.hpp"
#include "camdetect/ZoneMapper.hpp"

#include <algorithm>
#include <cmath>

namespace camdetect {

namespace {

/// Board-space error covariance of one camera's tip estimate: loose along
/// the dart's projected axis (mask truncation slides the tip up the shaft),
/// tight across it (the axis fit is pixel-accurate).  Falls back to an
/// isotropic sigma_mm ball when the hit carries no axis information.
cv::Matx22f covOf(const DartHit& h)
{
    const float ux = h.axis_board.x, uy = h.axis_board.y;
    if (ux * ux + uy * uy < 0.25f) {
        const float s2 = std::max(h.sigma_mm, 0.5f) * std::max(h.sigma_mm, 0.5f);
        return {s2, 0.f, 0.f, s2};
    }
    const float vx = -uy, vy = ux;
    const float sa2 = h.sigma_along_mm  * h.sigma_along_mm;
    const float sc2 = h.sigma_across_mm * h.sigma_across_mm;
    return {sa2*ux*ux + sc2*vx*vx, sa2*ux*uy + sc2*vx*vy,
            sa2*ux*uy + sc2*vx*vy, sa2*uy*uy + sc2*vy*vy};
}

float mahaSq(const cv::Point2f& d, const cv::Matx22f& S)
{
    const cv::Matx22f Si = S.inv();
    return d.x * (Si(0,0)*d.x + Si(0,1)*d.y) +
           d.y * (Si(1,0)*d.x + Si(1,1)*d.y);
}

/// Two votes describe the same physical tip when their displacement is
/// explainable by each camera's tip slide ALONG its dart's projected axis:
/// mask truncation slides the estimate toward the tail, shadows and axis
/// overshoot slide it past the point — but neither mechanism moves it
/// sideways.  So
///
///     a.xy − b.xy  =  s_a·axis_a − s_b·axis_b ,   |s| ≤ cap
///
/// must have a solution with both slide amounts bounded by what each cam's
/// silhouette quality makes plausible.  A full Mahalanobis ellipse with the
/// same sigmas is far too permissive — junk detections pass it from random
/// directions and then corrupt the crossing.
float slideCap(const DartHit& h)
{
    return std::min(2.f * h.sigma_along_mm + 10.f, 30.f);
}

bool consistent(const DartHit& a, const DartHit& b)
{
    const cv::Point2f d  = a.board_xy - b.board_xy;
    const cv::Point2f ua = a.axis_board;
    const cv::Point2f ub = b.axis_board;

    // Hits without axis info (legacy / degenerate): isotropic fallback.
    if (ua.dot(ua) < 0.25f || ub.dot(ub) < 0.25f)
        return std::sqrt(d.dot(d)) <
               3.f * (std::max(a.sigma_mm, 1.f) + std::max(b.sigma_mm, 1.f));

    const float cap_a = slideCap(a);
    const float cap_b = slideCap(b);
    const float cross = ua.x * ub.y - ua.y * ub.x;

    if (std::abs(cross) < 0.17f) {
        // Near-parallel axes: project on the common axis; the perpendicular
        // residual must be explained by the across-axis noise.
        const float s     = d.dot(ua);
        const cv::Point2f r = d - s * ua;
        const float tol   = 3.f * (a.sigma_across_mm + b.sigma_across_mm);
        return std::sqrt(r.dot(r)) <= tol && std::abs(s) <= cap_a + cap_b;
    }

    // d = s_a·ua − s_b·ub  →  exact 2×2 solve.
    const float s_a = (d.x * (-ub.y) + ub.x * d.y) / (-cross);
    const float s_b = (ua.x * d.y - ua.y * d.x)    / (-cross);
    return std::abs(s_a) <= cap_a && std::abs(s_b) <= cap_b;
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
    std::vector<DartHit> votes;
    votes.reserve(NUM_CAMS);
    for (auto& p : pending_)
        if (p) votes.push_back(*p);
    reset();
    return fuseVotes(votes);
}

std::optional<FusedHit> MultiCamFusion::fuseVotes(const std::vector<DartHit>& votes)
{
    const int n = static_cast<int>(votes.size());
    if (n < MIN_CAMS_FOR_CONFIRM) return std::nullopt;

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
    for (const auto& v : votes)
        if (!offBoard(v)) any_onboard = true;

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
        // Cross-cam support: peers that can see the claimed spot but show
        // no foreground there make this detection a likely shadow/ghost.
        if (h.support_peers > 0)
            w *= 0.5f + 0.5f * static_cast<float>(h.support_cams) /
                              static_cast<float>(h.support_peers);
        return w;
    };

    // Label weight: given the detection is real, how likely is its zone
    // label?  That's the margin/sigma confidence — used in the zone vote.
    auto voteWeight = [&](const DartHit& h) {
        float w = h.confidence;
        if (any_onboard && offBoard(h)) w *= OFFBOARD_DOUBT;
        return w;
    };

    // Largest cluster of mutually-explainable votes, seeded greedily: for
    // each vote count how many votes (incl. itself) are consistent with it
    // under the anisotropic error models; the densest seeds it.  Ties broken
    // by summed detection trust.
    int   best_seed   = -1;
    int   best_count  = 0;
    float best_seed_w = -1.f;
    for (int a = 0; a < n; ++a) {
        int   count = 0;
        float w_sum = 0.f;
        for (int b = 0; b < n; ++b) {
            if (!consistent(votes[a], votes[b])) continue;
            ++count;
            w_sum += seedTrust(votes[b]);
        }
        if (count > best_count ||
            (count == best_count && w_sum > best_seed_w)) {
            best_count  = count;
            best_seed   = a;
            best_seed_w = w_sum;
        }
    }
    if (best_count < MIN_CAMS_FOR_CONFIRM) return std::nullopt;

    std::vector<int> members;
    for (int b = 0; b < n; ++b)
        if (consistent(votes[best_seed], votes[b])) members.push_back(b);

    // ── Fused position: information-weighted least squares ────────────────
    // Each camera contributes its inverse covariance; the solution is the
    // point that best satisfies every cam's tight ACROSS-axis constraint —
    // i.e. the crossing of the cams' axis lines.  A camera that slid 30 mm
    // up its own shaft still pulls the solution onto its line sideways,
    // exactly where its information actually is.
    auto solve = [&](const std::vector<int>& mem,
                     cv::Matx22f& C_out, cv::Point2f& x_out) {
        cv::Matx22f W_sum = cv::Matx22f::zeros();
        cv::Vec2f   Wx_sum{0.f, 0.f};
        for (int m : mem) {
            const cv::Matx22f W = covOf(votes[m]).inv();
            W_sum  = W_sum + W;
            Wx_sum = Wx_sum + W * cv::Vec2f(votes[m].board_xy.x,
                                            votes[m].board_xy.y);
        }
        C_out = W_sum.inv();
        const cv::Vec2f xv = C_out * Wx_sum;
        x_out = {xv[0], xv[1]};
    };

    cv::Matx22f C_fused;
    cv::Point2f centroid;
    solve(members, C_fused, centroid);

    // Robust refit (leave-one-out).  A single subtly-bad axis can stay inside
    // the slide-consistency cluster yet lever the crossing — and because
    // Pipeline then back-projects the crossing into every camera's zone map,
    // a corrupted crossing would read as a FALSELY UNANIMOUS wrong zone.  With
    // ≤3 votes RANSAC is theatre; instead, if 3+ cams contribute, find the
    // member whose ACROSS-axis residual to the crossing is largest and, if it
    // exceeds a χ²-ish gate, drop it and refit on the rest.
    if (members.size() >= 3) {
        // Score each camera by its across-residual to the crossing of the OTHER
        // cameras (leave-one-out).  Crucially NOT to the full-solve centroid:
        // that is already contaminated by a bad axis (pulled toward it along a
        // good camera's loose along-direction), which masks the very outlier we
        // want to catch.  The leave-one-out crossing is uncontaminated by k, so
        // a genuinely bad camera shows a large honest residual to it.
        int         worst   = -1;
        float       worst_r = 0.f;
        cv::Matx22f best_C   = C_fused;
        cv::Point2f best_X   = centroid;
        for (int k : members) {
            std::vector<int> others;
            for (int m : members) if (m != k) others.push_back(m);
            cv::Matx22f Ck; cv::Point2f Xk;
            solve(others, Ck, Xk);
            const float rr = mahaSq(votes[k].board_xy - Xk, covOf(votes[k]));
            if (rr > worst_r) { worst_r = rr; worst = k; best_C = Ck; best_X = Xk; }
        }
        constexpr float OUTLIER_MAHA = 9.f;   // ~3σ in the tight across direction
        if (worst >= 0 && worst_r > OUTLIER_MAHA) {
            std::vector<int> kept;
            for (int m : members) if (m != worst) kept.push_back(m);
            members.swap(kept);
            C_fused  = best_C;       // the clean crossing of the kept cameras
            centroid = best_X;
        }
    }

    // Fused 1-sigma: worst direction of the fused covariance, inflated when
    // the members' residuals exceed what their error models promised
    // (mean Mahalanobis residual should be ≈ 2 for 2-dof observations).
    const float tr   = C_fused(0,0) + C_fused(1,1);
    const float det  = C_fused(0,0)*C_fused(1,1) - C_fused(0,1)*C_fused(1,0);
    const float disc = std::sqrt(std::max(0.f, tr*tr - 4.f*det));
    float sigma_fused = std::sqrt(std::max(0.25f, (tr + disc) * 0.5f));
    float infl = 1.f;
    if (members.size() >= 2) {
        float r_sum = 0.f;
        for (int m : members)
            r_sum += mahaSq(votes[m].board_xy - centroid, covOf(votes[m]));
        const float r_mean = r_sum / static_cast<float>(members.size());
        if (r_mean > 2.f) infl = std::sqrt(r_mean * 0.5f);
    }
    sigma_fused *= infl;

    // Radial 1-sigma at the crossing: project the fused covariance onto the
    // unit radial r̂.  This is what the ring (single/triple/double) decision
    // must be gated on — when no camera sees the spot near-tangentially,
    // radius lives in the loose along-axis direction and sigma_r is large.
    float sigma_r = sigma_fused;
    {
        const float r = std::sqrt(centroid.x*centroid.x + centroid.y*centroid.y);
        if (r > 1e-3f) {
            const cv::Vec2f rh{centroid.x / r, centroid.y / r};
            const float var_r =
                rh[0]*(C_fused(0,0)*rh[0] + C_fused(0,1)*rh[1]) +
                rh[1]*(C_fused(1,0)*rh[0] + C_fused(1,1)*rh[1]);
            sigma_r = std::sqrt(std::max(0.25f, var_r)) * infl;
        }
    }

    // ── Zone: probability-weighted vote ────────────────────────────────────
    // Every member cam votes for its pixel-accurate label with its
    // voteWeight.  The fused position votes too, through the geometric
    // lookup — but ONLY when it comes from a genuine multi-cam crossing: a
    // single-cam "centroid" is the same measurement twice and would just
    // inflate that cam's own confidence.
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

    for (int m : members)
        addVote({votes[m].zone, votes[m].score}, voteWeight(votes[m]));

    // The crossing votes only to ARBITRATE between disagreeing label reads.
    // When every member read the same zone off its pixel-accurate map, that
    // unanimity outranks a geometric lookup at the fused point: near the
    // bull a few mm of leftover calibration bias in the crossing must not
    // override three agreeing cameras.
    bool unanimous = true;
    for (size_t i = 1; i < members.size(); ++i)
        if (votes[members[i]].zone != votes[members[0]].zone)
            unanimous = false;

    if (members.size() >= 2 && !unanimous) {
        constexpr float CROSSING_DISCOUNT = 0.9f;
        const ZoneResult zr_c     = ZoneMapper::lookup(centroid);
        const float      margin_c = ZoneMapper::boundaryMarginMM(centroid);
        const float      p_c      = std::erf(
            margin_c / (std::max(sigma_fused, 0.5f) *
                        static_cast<float>(M_SQRT2)));
        addVote(zr_c, p_c * CROSSING_DISCOUNT);
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
    f.sigma_r_mm = sigma_r;
    f.confidence = std::clamp(p_right * consensus, 0.f, 1.f);
    f.timestamp  = votes[members.front()].timestamp;
    for (int m : members) {
        f.timestamp = std::min(f.timestamp, votes[m].timestamp);
        f.per_cam[votes[m].cam_id] = votes[m];
    }
    return f;
}

void MultiCamFusion::reset()
{
    for (auto& p : pending_) p.reset();
    first_ts_ = -1.0;
}

} // namespace camdetect
