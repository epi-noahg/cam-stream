#include "camdetect/Pipeline.hpp"
#include "camdetect/ZoneMapper.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace camdetect {

namespace {

/// CAMDETECT_TRACE=1 dumps every per-cam hit, fusion emission and dedup
/// decision to stderr — the raw material for debugging a failed test run.
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

int fusedVoteCount(const FusedHit& h)
{
    int n = 0;
    for (int i = 0; i < NUM_CAMS; ++i)
        if (h.per_cam[i].cam_id >= 0) ++n;
    return n;
}

// Same-dart association against committed round hits.
//
// A single physical dart can produce several fused-hit emissions when slow
// cameras stabilise in later fusion windows; with rim parallax and
// along-axis tip slide their board projections can disagree by tens of
// millimetres, so neither space nor time alone identifies "same dart".
//
// Two committed signals decide it:
//
//   * VOTER OVERLAP — a detector never re-emits a dart it already reported
//     (ghost regions are suppressed at the source), so a new fused hit that
//     shares a voting camera with a committed hit MUST be a new physical
//     dart, however close it landed.  This is what lets two darts sit 10 mm
//     apart in the same sector and still both score.
//
//   * Otherwise (disjoint voters) a new hit inside the space/time gates is a
//     late re-observation by cameras that hadn't reported that dart yet —
//     it is MERGED into the committed hit and the union re-fused, so a
//     well-sighted straggler can move a misread zone to the right one.
constexpr double ROUND_DEDUP_HARD_TIME_S =  1.0;
constexpr float  ROUND_DEDUP_MM          = 30.f;
constexpr double ROUND_DEDUP_TIME_S      =  1.8;
constexpr float  ROUND_DEDUP_TIME_MM     = 80.f;

bool votersOverlap(const FusedHit& a, const FusedHit& b)
{
    for (int i = 0; i < NUM_CAMS; ++i)
        if (a.per_cam[i].cam_id >= 0 && b.per_cam[i].cam_id >= 0)
            return true;
    return false;
}

} // anon

Pipeline::Pipeline(std::array<BoardCalibration, NUM_CAMS> calibrations,
                   double                                  fusion_window_seconds)
    : fusion_(fusion_window_seconds)
{
    for (int i = 0; i < NUM_CAMS; ++i)
        detectors_[i] = std::make_unique<DartDetector>(i, std::move(calibrations[i]));
}

void Pipeline::setOnHit(HitCallback cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    on_hit_ = std::move(cb);
}

void Pipeline::setOnHitUpdated(HitUpdateCallback cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    on_hit_updated_ = std::move(cb);
}

void Pipeline::setZoneMap(int cam_id, ZoneMap zm)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;
    std::lock_guard<std::mutex> lk(mtx_);
    detectors_[cam_id]->setZoneMap(std::move(zm));
}

void Pipeline::setCalibration(int cam_id, BoardCalibration calib)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;
    std::lock_guard<std::mutex> lk(mtx_);
    detectors_[cam_id] = std::make_unique<DartDetector>(cam_id, std::move(calib));
}

void Pipeline::feedFrame(int cam_id, const cv::Mat& frame, double timestamp)
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return;

    std::optional<DartHit>  hit;
    std::optional<FusedHit> fused;
    HitCallback             cb;

    {
        std::lock_guard<std::mutex> lk(mtx_);

        // Advance the shared fusion clock with the newest timestamp seen on any
        // camera.  This is what lets an open fusion window close on time even
        // when one camera lags or never reports the current dart.
        if (timestamp > fusion_clock_) fusion_clock_ = timestamp;

        hit = detectors_[cam_id]->processFrame(frame, timestamp);

        // Cross-cam support: a dart that really sticks at hit->board_xy must
        // leave foreground in the peers' cumulative masks at the projected
        // pixel.  A shadow or reflection fools one camera but has no backing
        // anywhere else — fusion uses this to break junk-vs-real ties.
        if (hit) {
            for (int i = 0; i < NUM_CAMS; ++i) {
                if (i == cam_id) continue;
                const auto& det = *detectors_[i];
                if (!det.cumSupportValid() || !det.calib().isValid()) continue;
                ++hit->support_peers;
                const cv::Point2f px = det.calib().boardToImage(hit->board_xy);
                if (det.hasForegroundNear(px, 18.f)) ++hit->support_cams;
            }
        }

        if (hit)
            CAMTRACE("[trace] t=%.2f f=%d cam%d HIT zone=%s xy=(%.0f,%.0f) "
                     "conf=%.2f sigma=%.1f shape_q=%.2f margin=%.1f "
                     "axis=(%.2f,%.2f) s_along=%.0f s_across=%.1f sup=%d/%d",
                     timestamp, static_cast<int>(timestamp * 30 + 0.5), cam_id,
                     hit->zone.c_str(), hit->board_xy.x, hit->board_xy.y,
                     hit->confidence, hit->sigma_mm, hit->shape_q,
                     hit->zone_margin_mm, hit->axis_board.x, hit->axis_board.y,
                     hit->sigma_along_mm, hit->sigma_across_mm,
                     hit->support_cams, hit->support_peers);
        if (hit && darts_in_round_ >= MAX_DARTS_PER_ROUND)
            CAMTRACE("[trace] t=%.2f cam%d hit DROPPED (round full)",
                     timestamp, cam_id);

        if (darts_in_round_ < MAX_DARTS_PER_ROUND) {
            if (hit) fused = fusion_.addHit(*hit);          // all-cams fast path
            if (!fused) fused = fusion_.tick(fusion_clock_); // time-driven close
            if (fused) {
                // Determine the zone from the triangulated crossing rather
                // than each camera's own tip — far more robust near wires and
                // ring edges, where a few px of per-cam tip slide flips the
                // label.  Also recomputes an honest confidence.
                refineFusedZone_(*fused);
                if (traceEnabled()) {
                    int nv = fusedVoteCount(*fused);
                    CAMTRACE("[trace] t=%.2f f=%d FUSED zone=%s xy=(%.0f,%.0f) "
                             "conf=%.2f votes=%d sigma=%.1f sigma_r=%.1f",
                             fused->timestamp,
                             static_cast<int>(fused->timestamp * 30 + 0.5),
                             fused->zone.c_str(), fused->board_xy.x,
                             fused->board_xy.y, fused->confidence, nv,
                             fused->sigma_mm, fused->sigma_r_mm);
                }
                bool is_dup           = false;
                bool updated          = false;
                const cv::Point2f cur = fused->board_xy;
                for (auto& prev : round_hits_) {
                    const double dt = std::abs(fused->timestamp - prev.timestamp);
                    const cv::Point2f p  = prev.board_xy;
                    const float       dx = cur.x - p.x;
                    const float       dy = cur.y - p.y;
                    const float       d2 = dx * dx + dy * dy;

                    const bool same_dart_gate =
                        dt < ROUND_DEDUP_HARD_TIME_S ||
                        d2 < ROUND_DEDUP_MM * ROUND_DEDUP_MM ||
                        (dt < ROUND_DEDUP_TIME_S &&
                         d2 < ROUND_DEDUP_TIME_MM * ROUND_DEDUP_TIME_MM);
                    if (!same_dart_gate) continue;

                    // Shared voter ⇒ that camera's detector saw NEW mask
                    // material (its ghosts are suppressed at the source), so
                    // this is a new physical dart landing close by — keep
                    // looking, and commit if no other hit claims it.
                    if (votersOverlap(prev, *fused)) {
                        CAMTRACE("[trace]   near %s (dt=%.2f d=%.0fmm) but "
                                 "voters overlap -> NEW dart",
                                 prev.zone.c_str(), dt, std::sqrt(d2));
                        continue;
                    }

                    // Disjoint voters: late re-observation of this committed
                    // dart.  Merge the vote sets and re-fuse — the union may
                    // both sharpen the position (axis crossing) and flip the
                    // zone to the better-sighted camera's label.
                    std::vector<DartHit> union_votes;
                    for (int i = 0; i < NUM_CAMS; ++i) {
                        if (prev.per_cam[i].cam_id >= 0)
                            union_votes.push_back(prev.per_cam[i]);
                        else if (fused->per_cam[i].cam_id >= 0)
                            union_votes.push_back(fused->per_cam[i]);
                    }
                    if (auto refused = MultiCamFusion::fuseVotes(union_votes)) {
                        refused->timestamp = prev.timestamp;
                        refineFusedZone_(*refused);
                        const bool changed = refused->zone != prev.zone;
                        CAMTRACE("[trace]   MERGE into %s -> %s (conf=%.2f, "
                                 "votes=%d)",
                                 prev.zone.c_str(), refused->zone.c_str(),
                                 refused->confidence,
                                 fusedVoteCount(*refused));
                        prev    = *refused;
                        updated = changed || true;
                        *fused  = *refused;   // callback reports the merged hit
                    }
                    is_dup = true;
                    break;
                }
                if (!is_dup) {
                    ++darts_in_round_;
                    round_hits_.push_back(*fused);
                    cb = on_hit_;
                    CAMTRACE("[trace]   COMMIT dart %d: %s",
                             darts_in_round_, fused->zone.c_str());
                } else if (updated) {
                    cb = on_hit_updated_;
                }
            }
        }
        // Watchdog counters are in master frames: tick them on one camera's
        // feed only, otherwise every threshold fires NUM_CAMS× too early —
        // bad enough to force a background warmup while the player is still
        // pulling darts, which poisons the reference for the whole next round.
        if (cam_id == 0) watchdogStuckHuman();
        maybeAutoReset();
    }

    if (cb && fused) cb(*fused);
}

void Pipeline::refineFusedZone_(FusedHit& f) const
{
    // How many cameras actually voted for this hit?  Two or more means the
    // position is TRIANGULATED (the weighted-LS crossing pins the tip across
    // every camera's axis); one means the tip can still be slid arbitrarily
    // far along that lone camera's dart axis — the zone is then only as good
    // as that single silhouette.
    int n_voters = 0;
    for (int i = 0; i < NUM_CAMS; ++i)
        if (f.per_cam[i].cam_id >= 0) ++n_voters;

    const cv::Point2f X = f.board_xy;

    // Crossing trust ∈ (0,1]: high only when the tip is genuinely triangulated
    // (≥2 voters) with a tight fused covariance.  Scales the CROSSING-coupled
    // reads (geometric + back-projection) so that when the crossing is weak the
    // independent own-tip reads carry the decision instead — a single bad axis
    // can then never make a back-projected wrong zone look falsely unanimous.
    const float cq = (n_voters >= 2)
        ? std::clamp(1.f - f.sigma_mm / 12.f, 0.15f, 1.f) : 0.15f;

    // Candidate zone reads, each weighted by how clearly it sits inside its
    // zone (boundary margin in mm) times the source's viewing quality.
    struct Acc {
        std::string label;
        int         score      {0};
        float       weight     {0.f};   // Σ weights across agreeing reads
        float       best_margin{0.f};   // mm to nearest boundary, best source
    };
    std::array<Acc, 2 * NUM_CAMS + 2> acc{};
    int n_acc = 0;
    auto addW = [&](const std::string& label, int score,
                    float weight, float margin_mm) {
        if (weight <= 0.f) return;
        for (int i = 0; i < n_acc; ++i) {
            if (acc[i].label != label) continue;
            acc[i].weight     += weight;
            acc[i].best_margin = std::max(acc[i].best_margin, margin_mm);
            return;
        }
        acc[n_acc++] = {label, score, weight, margin_mm};
    };

    constexpr float DEFAULT_MM_PER_PX = 0.6f;

    // ── Channel A — CROSSING (coupled): geometric + back-projection ─────────
    // 1) Geometric lookup at the crossing — the canonical board reference,
    //    treated as one head-on "virtual camera".
    addW(ZoneMapper::lookup(X).label, ZoneMapper::lookup(X).value,
         std::max(ZoneMapper::boundaryMarginMM(X), 0.25f) * cq,
         ZoneMapper::boundaryMarginMM(X));
    // 2) Every camera with usable geometry reads its pixel-accurate map at X
    //    projected back into that camera — all using the SAME triangulated
    //    point, so good crossings read near-unanimously regardless of each
    //    camera's own tip slide.
    for (int i = 0; i < NUM_CAMS; ++i) {
        const auto& det = *detectors_[i];
        if (!det.calib().isValid() || det.zoneMap().empty()) continue;
        const cv::Point2f px = det.calib().boardToImage(X);
        const ZoneResult  zr = det.zoneMap().lookup(px);
        const cv::Vec2f   sc = det.calib().localScaleMmPerPx(px);
        const float mm_per_px = sc[0] > 0.f ? sc[0] : DEFAULT_MM_PER_PX;
        const float margin_mm = det.zoneMap().boundaryDistancePx(px) * mm_per_px;
        const float viewq = sc[1] > 0.f
            ? std::clamp(DEFAULT_MM_PER_PX / sc[1], 0.05f, 1.f) : 0.3f;
        addW(zr.label, zr.value, std::max(margin_mm, 0.25f) * viewq * cq,
             margin_mm);
    }

    // ── Channel B — OWN-TIP (independent): each voting camera's direct label ─
    // The detector already read its pixel-accurate map at its OWN measured tip.
    // That read is independent of the crossing, so it provides dissent if the
    // crossing is corrupted.  Up-weighted as crossing trust falls (so it owns
    // single-camera hits) and kept secondary when the crossing is strong (so a
    // lone wrong tip never overturns a clean triangulation).
    const float own_scale = 0.6f * (1.3f - cq);
    for (int i = 0; i < NUM_CAMS; ++i) {
        const DartHit& h = f.per_cam[i];
        if (h.cam_id < 0 || h.zone.empty()) continue;
        const float viewq = std::clamp(h.view_q, 0.05f, 1.f);
        addW(h.zone, h.score,
             std::max(h.zone_margin_mm, 0.25f) * viewq * own_scale,
             h.zone_margin_mm);
    }

    if (n_acc == 0) return;   // no usable geometry; leave fusion's own label

    int   wi = 0;
    float total = 0.f;
    for (int i = 0; i < n_acc; ++i) {
        total += acc[i].weight;
        if (acc[i].weight > acc[wi].weight) wi = i;
    }
    if (total <= 0.f) return;

    f.zone  = acc[wi].label;
    f.score = acc[wi].score;

    // ── Honest confidence ───────────────────────────────────────────────────
    //   consensus  — winner's share of the total vote weight (collapses when
    //                sources split between labels: a fragile sector call).
    //   placement  — erf(margin / (sigma·√2)): chance a Gaussian positional
    //                error leaves the label intact.  sigma is the small fused
    //                value when triangulated, inflated to the along-slide for a
    //                lone camera (whose tip is free to be wrong along its axis).
    //   ring_mass  — for a narrow ring band (triple/double/bull/25): the actual
    //                Gaussian probability mass of the radius inside that band,
    //                from the fused RADIAL sigma.  Parameter-free: an
    //                un-triangulated or radius-uncertain T/D earns near-zero
    //                confidence and gets flagged for review, without ever
    //                downgrading a correct-but-edge label.
    float sigma_eff = std::max(f.sigma_mm, 0.5f);
    if (n_voters < 2) {
        for (int i = 0; i < NUM_CAMS; ++i)
            if (f.per_cam[i].cam_id >= 0)
                sigma_eff = std::max(sigma_eff, f.per_cam[i].sigma_along_mm);
    }
    const float consensus = acc[wi].weight / total;
    const float placement = std::erf(
        acc[wi].best_margin / (sigma_eff * static_cast<float>(M_SQRT2)));

    float ring_mass = 1.f;
    {
        float lo = 0.f, hi = 0.f;
        bool narrow = false;
        const std::string& z = f.zone;
        if (!z.empty() && z[0] == 'T') {
            lo = board::TRIPLE_INNER; hi = board::TRIPLE_OUTER; narrow = true;
        } else if (!z.empty() && z[0] == 'D') {
            lo = board::DOUBLE_INNER; hi = board::DOUBLE_OUTER; narrow = true;
        } else if (z == "Bull") {
            lo = 0.f; hi = board::BULLSEYE_RADIUS; narrow = true;
        } else if (z == "25") {
            lo = board::BULLSEYE_RADIUS; hi = board::BULL_RADIUS; narrow = true;
        }
        if (narrow) {
            const float r  = std::sqrt(X.x * X.x + X.y * X.y);
            const float sr = std::max(f.sigma_r_mm, 0.5f);
            auto Phi = [](float zz) {
                return 0.5f * (1.f + std::erf(zz * static_cast<float>(M_SQRT1_2)));
            };
            ring_mass = std::clamp(Phi((hi - r) / sr) - Phi((lo - r) / sr),
                                   0.f, 1.f);
        }
    }
    f.confidence = std::clamp(consensus * placement * ring_mass, 0.f, 1.f);
}

void Pipeline::watchdogStuckHuman()
{
    // Cross-cam watchdog: if 2+ peers say OK and one is HumanBlob, the
    // outlier is almost certainly seeing lighting noise rather than a real
    // human → force a bg refresh on it after STUCK_HUMAN_FRAMES (~2s).
    int ok_count    = 0;
    int human_count = 0;
    for (int i = 0; i < NUM_CAMS; ++i) {
        const auto s = detectors_[i]->state();
        if (s == DetectorState::Normal || s == DetectorState::BoardClean)
            ++ok_count;
        if (s == DetectorState::HumanBlob)
            ++human_count;
    }

    for (int i = 0; i < NUM_CAMS; ++i) {
        const bool is_human = detectors_[i]->state() == DetectorState::HumanBlob;
        const bool peers_ok = ok_count >= 2;
        if (is_human && peers_ok) {
            if (++stuck_human_frames_[i] > STUCK_HUMAN_FRAMES) {
                detectors_[i]->refreshBackground();
                stuck_human_frames_[i] = 0;
            }
        } else {
            stuck_human_frames_[i] = 0;
        }
    }

    // Post-round backstop: when the round is complete (collect phase) and
    // ANY cam is still HUMAN, after POST_ROUND_STUCK_FRAMES (~5s) we force a
    // global bg refresh.  Covers the all-cams-stuck case (rim noise after
    // collect) that the per-cam watchdog can't break out of.
    const bool round_complete = (darts_in_round_ >= MAX_DARTS_PER_ROUND);
    if (round_complete && human_count > 0) {
        if (++post_round_human_frames_ > POST_ROUND_STUCK_FRAMES) {
            for (auto& d : detectors_) d->refreshBackground();
            post_round_human_frames_ = 0;
        }
    } else {
        post_round_human_frames_ = 0;
    }
}

RoundStatus Pipeline::roundStatus() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return computeRoundStatus_();
}

RoundStatus Pipeline::computeRoundStatus_() const
{
    bool any_human  = false;
    bool any_warmup = false;
    for (const auto& d : detectors_) {
        const auto s = d->state();
        if (s == DetectorState::HumanBlob) any_human  = true;
        if (s == DetectorState::Warmup)    any_warmup = true;
    }

    RoundStatus rs;
    if (any_warmup) {
        rs.phase   = RoundPhase::Resyncing;
        rs.message = "Initialising — please wait";
    } else if (any_human) {
        rs.phase   = RoundPhase::Resyncing;
        rs.message = "Wait — board is being cleaned";
    } else if (darts_in_round_ >= MAX_DARTS_PER_ROUND) {
        rs.phase   = RoundPhase::Complete;
        rs.message = "Collect your darts";
    } else {
        rs.phase     = RoundPhase::WaitingDart;
        rs.next_dart = darts_in_round_ + 1;
        rs.message   = "Throw dart " + std::to_string(rs.next_dart);
    }
    return rs;
}

void Pipeline::resetRound()
{
    std::lock_guard<std::mutex> lk(mtx_);
    fusion_.reset();
    for (auto& d : detectors_) d->reset();
    darts_in_round_ = 0;
    round_hits_.clear();
}

void Pipeline::setDiffThreshold(float v)
{
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& d : detectors_) d->setDiffThreshold(v);
}

float Pipeline::diffThreshold() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return detectors_[0] ? detectors_[0]->diffThreshold() : 0.f;
}

void Pipeline::setLineMergePerpPx(float v)
{
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& d : detectors_) d->setLineMergePerpPx(v);
}

float Pipeline::lineMergePerpPx() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return detectors_[0] ? detectors_[0]->lineMergePerpPx() : 0.f;
}

std::vector<FusedHit> Pipeline::roundHits() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return round_hits_;
}

void Pipeline::refreshBackground(int cam_id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (cam_id < 0) {
        for (auto& d : detectors_) d->refreshBackground();
    } else if (cam_id < NUM_CAMS) {
        detectors_[cam_id]->refreshBackground();
    }
}

DetectorViz Pipeline::camViz(int cam_id) const
{
    if (cam_id < 0 || cam_id >= NUM_CAMS) return {};
    std::lock_guard<std::mutex> lk(mtx_);
    return detectors_[cam_id]->lastViz();
}

int Pipeline::dartsInRound() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return darts_in_round_;
}

void Pipeline::maybeAutoReset()
{
    if (darts_in_round_ == 0) return;

    if (traceEnabled()) {
        static double last_log = -1e9;
        if (fusion_clock_ - last_log >= 1.0) {
            last_log = fusion_clock_;
            char buf[256];
            int  off = std::snprintf(buf, sizeof(buf),
                                     "[trace] t=%.2f reset-watch darts=%d",
                                     fusion_clock_, darts_in_round_);
            for (int i = 0; i < NUM_CAMS; ++i) {
                const auto viz = detectors_[i]->lastViz();
                off += std::snprintf(buf + off, sizeof(buf) - off,
                                     "  cam%d{st=%d fg=%d cln=%d}",
                                     i, static_cast<int>(viz.state),
                                     viz.fg_px_cumulative, viz.clean_frames);
            }
            CAMTRACE("%s", buf);
        }
    }

    for (const auto& d : detectors_)
        if (!d->boardLooksCleared()) return;

    fusion_.reset();
    for (auto& d : detectors_) d->reset();
    darts_in_round_ = 0;
    round_hits_.clear();
}

} // namespace camdetect
