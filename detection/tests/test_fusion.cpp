// camdetect_test_fusion
// ─────────────────────────────────────────────────────────────────────────────
// INVARIANCE tests for the multi-camera fusion estimator.
//
// With only one recorded session and zone-only ground truth, the session replay
// cannot prove the estimator generalises.  These tests instead assert PROPERTIES
// that must hold for ANY input — properties cannot be overfit to a session:
//
//   A. along-slide invariance   the crossing recovers the true contact point
//                               even when every camera's tip slid arbitrarily
//                               along its own dart axis (the core reason the
//                               2-line crossing beats any single camera).
//   B. outlier robustness       one camera with a bad axis / off position does
//                               not corrupt the crossing (clustering + LOO).
//   C. radial-sigma honesty     sigma_r is LARGER when the cameras' axes are
//                               near-parallel (radius ill-constrained) than
//                               when they cross cleanly — so the ring decision
//                               degrades honestly instead of guessing.
//   D. consistency exclusion    a wildly inconsistent vote is dropped, not
//                               averaged into the position.
//   E. single-camera passthrough one vote still produces a hit at its own point.
//
// Exit 0 = all pass, 1 = any fail.

#include "camdetect/MultiCamFusion.hpp"
#include "camdetect/Types.hpp"
#include "camdetect/ZoneMapper.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace camdetect;

namespace {

int g_fail = 0;

void check(bool ok, const std::string& name, const std::string& detail = "")
{
    std::printf("%-44s %s%s%s\n", name.c_str(), ok ? "PASS" : "FAIL",
                detail.empty() ? "" : "  — ", detail.c_str());
    if (!ok) ++g_fail;
}

cv::Point2f unit(float deg)
{
    const float r = deg * static_cast<float>(M_PI) / 180.f;
    return {std::cos(r), std::sin(r)};
}

// Observe true contact point P with board-axis direction u (unit, tip→tail),
// the tip slid `slide` mm along u, anisotropic error (tight across, loose along).
DartHit mk(int cam, const cv::Point2f& P, const cv::Point2f& u, float slide,
           float s_along = 25.f, float s_across = 2.5f)
{
    DartHit h;
    h.cam_id          = cam;
    h.board_xy        = P + u * slide;
    h.axis_board      = u;
    h.sigma_along_mm  = s_along;
    h.sigma_across_mm = s_across;
    h.sigma_mm        = s_across;
    const ZoneResult zr = ZoneMapper::lookup(h.board_xy);
    h.zone            = zr.label;
    h.score           = zr.value;
    h.zone_margin_mm  = ZoneMapper::boundaryMarginMM(h.board_xy);
    h.shape_q         = 0.8f;
    h.view_q          = 0.8f;
    h.confidence      = 0.7f;
    h.timestamp       = 0.0;
    return h;
}

float dist(const cv::Point2f& a, const cv::Point2f& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

} // namespace

int main()
{
    // ── A. Along-slide invariance ───────────────────────────────────────────
    // True point well inside single-20 (top).  Three cameras ~120° apart in
    // board-axis direction, each slid hard along its own axis.  The crossing
    // must still land on the true point — the whole premise of the rig.
    {
        const cv::Point2f P{8.f, 70.f};   // single 20 region, clear of wires
        std::vector<DartHit> v{
            mk(0, P, unit(100.f),  18.f),
            mk(1, P, unit(225.f), -22.f),
            mk(2, P, unit(345.f),  30.f),
        };
        auto f = MultiCamFusion::fuseVotes(v);
        const bool got = f.has_value();
        const float d = got ? dist(f->board_xy, P) : 1e9f;
        check(got && d < 8.f, "A. along-slide invariance",
              "crossing err=" + std::to_string(d) + "mm (per-cam slid 18-30mm)");
    }

    // ── B. Outlier robustness ───────────────────────────────────────────────
    // Two consistent cameras + one with a large ACROSS-axis error (a bad axis
    // fit / wrong silhouette).  The crossing must track the two good cameras.
    {
        const cv::Point2f P{8.f, 70.f};
        DartHit bad = mk(2, P, unit(345.f), 5.f);
        const cv::Point2f perp{-unit(345.f).y, unit(345.f).x};
        bad.board_xy = P + perp * 18.f;          // 18mm sideways = inconsistent
        std::vector<DartHit> v{
            mk(0, P, unit(100.f),  15.f),
            mk(1, P, unit(225.f), -12.f),
            bad,
        };
        auto f = MultiCamFusion::fuseVotes(v);
        const bool got = f.has_value();
        const float d = got ? dist(f->board_xy, P) : 1e9f;
        check(got && d < 10.f, "B. one-bad-axis robustness",
              "crossing err=" + std::to_string(d) + "mm (outlier 18mm across)");
    }

    // ── C. Radial-sigma honesty ─────────────────────────────────────────────
    // Crossed axes (good radial geometry) vs near-parallel axes (radius lives
    // in the loose along-direction).  sigma_r must be larger for the parallel
    // case so the ring decision degrades honestly.
    {
        const cv::Point2f P{8.f, 103.f};   // triple-20 radius
        const cv::Point2f rad = P * (1.f / std::sqrt(P.dot(P)));  // radial dir
        // Crossed: axes 60° apart, neither aligned with the radial → radius
        // well observed across at least one axis.
        std::vector<DartHit> crossed{
            mk(0, P, unit(60.f),  5.f),
            mk(1, P, unit(120.f), -5.f),
        };
        // Parallel & RADIAL: both axes aligned with rad → radius lives in the
        // loose along-direction of both, so it is poorly constrained.
        std::vector<DartHit> parallel{
            mk(0, P,  rad,  5.f),
            mk(1, P, -rad, -5.f),
        };
        auto fc = MultiCamFusion::fuseVotes(crossed);
        auto fp = MultiCamFusion::fuseVotes(parallel);
        const bool got = fc && fp;
        const float src = got ? fc->sigma_r_mm : 0.f;
        const float srp = got ? fp->sigma_r_mm : 1e9f;
        check(got && srp > src + 2.f, "C. radial-sigma honesty",
              "sigma_r crossed=" + std::to_string(src) +
              " parallel=" + std::to_string(srp));
    }

    // ── D. Consistency exclusion ────────────────────────────────────────────
    // A vote at a totally different board location must not be averaged in.
    {
        const cv::Point2f P{8.f, 70.f};
        std::vector<DartHit> v{
            mk(0, P, unit(100.f),  10.f),
            mk(1, P, unit(225.f), -10.f),
            mk(2, cv::Point2f{-120.f, -40.f}, unit(10.f), 0.f),  // far away
        };
        auto f = MultiCamFusion::fuseVotes(v);
        const bool got = f.has_value();
        const float d = got ? dist(f->board_xy, P) : 1e9f;
        check(got && d < 10.f, "D. inconsistent-vote exclusion",
              "crossing err=" + std::to_string(d) + "mm (3rd vote 130mm off)");
    }

    // ── E. Single-camera passthrough ────────────────────────────────────────
    {
        const cv::Point2f P{40.f, 40.f};
        std::vector<DartHit> v{ mk(0, P, unit(200.f), 0.f) };
        auto f = MultiCamFusion::fuseVotes(v);
        const bool got = f.has_value();
        const float d = got ? dist(f->board_xy, P) : 1e9f;
        check(got && d < 1.f, "E. single-camera passthrough",
              got ? "" : "no hit");
    }

    std::printf("\n[test_fusion] %s\n", g_fail == 0 ? "ALL PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
