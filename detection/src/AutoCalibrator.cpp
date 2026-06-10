#include "camdetect/AutoCalibrator.hpp"
#include "camdetect/Types.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace camdetect {

namespace {

constexpr float DEG = 180.f / static_cast<float>(M_PI);

/// One red/green connected component, with its place on the board.
struct Blob {
    cv::Rect    rect;
    cv::Point2f centroid;
    double      area   {0};
    bool        is_red {false};
    int         cc_label {0};      // label in its colour's CC image

    float r_n   {0.f};             // normalised radius (1.0 ≈ double outer)
    float angle {0.f};             // deg, 0 = image up, clockwise
    int   band  {-1};              // 0 bull-area, 1 triple, 2 double, -1 out
    int   slot  {-1};              // angular slot 0..19 (arbitrary origin)
};

float angDiff(float a, float b)
{
    float d = std::fmod(a - b + 540.f, 360.f) - 180.f;
    return std::abs(d);
}

/// Connected components of @p mask → blobs appended to @p out.
/// @p cc_labels receives the CV_32S label image (kept to extract masks later).
void collectBlobs(const cv::Mat& mask, bool is_red, int min_area,
                  cv::Mat& cc_labels, std::vector<Blob>& out)
{
    cv::Mat stats, centroids;
    const int n = cv::connectedComponentsWithStats(mask, cc_labels, stats,
                                                   centroids, 8, CV_32S);
    for (int i = 1; i < n; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < min_area) continue;
        Blob b;
        b.rect = cv::Rect(stats.at<int>(i, cv::CC_STAT_LEFT),
                          stats.at<int>(i, cv::CC_STAT_TOP),
                          stats.at<int>(i, cv::CC_STAT_WIDTH),
                          stats.at<int>(i, cv::CC_STAT_HEIGHT));
        b.centroid = {static_cast<float>(centroids.at<double>(i, 0)),
                      static_cast<float>(centroids.at<double>(i, 1))};
        b.area     = area;
        b.is_red   = is_red;
        b.cc_label = i;
        out.push_back(b);
    }
}

/// Binary mask of one blob (full-frame size).
cv::Mat blobMask(const Blob& b, const cv::Mat& red_cc, const cv::Mat& green_cc)
{
    const cv::Mat& cc = b.is_red ? red_cc : green_cc;
    cv::Mat m(cc.size(), CV_8U, cv::Scalar(0));
    cv::Mat roi_cc = cc(b.rect), roi_m = m(b.rect);
    roi_m.setTo(255, roi_cc == b.cc_label);
    return m;
}

/// Normalised coords wrt the fitted board ellipse: unit radius at the
/// ellipse boundary, angles measured in (circularised) image orientation so
/// "0° = up on screen, clockwise positive".
void toNormalized(const cv::RotatedRect& el, const cv::Point2f& p,
                  float& r_n, float& angle_deg)
{
    const float th = el.angle / DEG;
    const float ca = std::cos(th), sa = std::sin(th);
    const cv::Point2f d = p - el.center;
    const float xr = ( ca * d.x + sa * d.y) / (el.size.width  * 0.5f);
    const float yr = (-sa * d.x + ca * d.y) / (el.size.height * 0.5f);
    r_n = std::sqrt(xr * xr + yr * yr);
    // rotate back so the angle is an image-space direction, circularised
    const float xi = ca * xr - sa * yr;
    const float yi = sa * xr + ca * yr;
    angle_deg = std::fmod(std::atan2(xi, -yi) * DEG + 360.f, 360.f);
}

/// Board-space (mm) point at @p deg clockwise from sector 20, radius @p r.
cv::Point2f boardPt(float deg, float r)
{
    const float a = deg / DEG;
    return {r * std::sin(a), r * std::cos(a)};
}

} // namespace

AutoCalibrator::Result AutoCalibrator::run(const cv::Mat& frame_bgr,
                                           const Options& opt) const
{
    Result res;
    if (frame_bgr.empty() || frame_bgr.type() != CV_8UC3) {
        res.error = "expected a non-empty BGR frame";
        return res;
    }

    // ── 1. Colour segmentation (LAB a* channel, lighting-robust) ─────────
    cv::Mat blurred, lab;
    cv::GaussianBlur(frame_bgr, blurred, {3, 3}, 0);
    cv::cvtColor(blurred, lab, cv::COLOR_BGR2Lab);
    cv::Mat ch[3];
    cv::split(lab, ch);

    cv::Mat af, bf;
    ch[1].convertTo(af, CV_32F, 1.0, -128.0);
    ch[2].convertTo(bf, CV_32F, 1.0, -128.0);
    cv::Mat chroma;
    cv::magnitude(af, bf, chroma);

    // b* > -5 keeps blue/cyan out: blue scores a* ≈ +68 in LAB (would pass a
    // naive "red" test) but its b* is strongly negative, while board red and
    // green both sit at positive b*.
    cv::Mat red_mask   = (af >  opt.red_a_delta)   & (chroma > opt.min_chroma)
                       & (bf > -5);
    cv::Mat green_mask = (af < -opt.green_a_delta) & (chroma > opt.min_chroma)
                       & (bf > -5);

    // CLOSE only — an OPEN would erase the triple arcs, which are just a few
    // pixels wide when the board is seen at a grazing angle.  Sensor speckle
    // is handled by the min-area filter instead.
    const cv::Mat k3 = cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3});
    for (cv::Mat* m : {&red_mask, &green_mask})
        cv::morphologyEx(*m, *m, cv::MORPH_CLOSE, k3);
    res.red_mask   = red_mask;
    res.green_mask = green_mask;

    cv::Mat red_cc, green_cc;
    std::vector<Blob> blobs;
    collectBlobs(red_mask,   true,  opt.min_blob_area, red_cc,   blobs);
    collectBlobs(green_mask, false, opt.min_blob_area, green_cc, blobs);
    if (blobs.size() < 10) {
        res.error = "not enough red/green blobs found (" +
                    std::to_string(blobs.size()) +
                    ") — adjust colour thresholds / lighting";
        return res;
    }

    // ── 1b. Second pass: keep only the dominant spatial cluster ──────────
    // Stray red/green elsewhere in the frame (posters, LEDs, reflections on
    // the table) would corrupt the ellipse fit below, before any geometry
    // exists to reject them.  The board is by far the dominant colour mass,
    // so anchor on the pixel-mass-weighted median of blob centroids and drop
    // everything beyond a few times the weighted median distance.  Second
    // round runs tighter, with the anchor re-centred on the cleaned set.
    for (int pass = 0; pass < 2; ++pass) {
        const auto wmedian = [](std::vector<std::pair<float, double>> v) {
            std::sort(v.begin(), v.end());
            double total = 0;
            for (const auto& p : v) total += p.second;
            double acc = 0;
            for (const auto& p : v) {
                acc += p.second;
                if (acc >= total / 2) return p.first;
            }
            return v.back().first;
        };
        std::vector<std::pair<float, double>> xs, ys, ds;
        for (const Blob& b : blobs) {
            xs.emplace_back(b.centroid.x, b.area);
            ys.emplace_back(b.centroid.y, b.area);
        }
        const cv::Point2f anchor{wmedian(xs), wmedian(ys)};
        for (const Blob& b : blobs)
            ds.emplace_back(static_cast<float>(cv::norm(b.centroid - anchor)),
                            b.area);
        // Board blobs sit between the bull and the double ring, so their
        // weighted median distance ≈ 0.7-0.9 of the board radius; the
        // farthest legit blob is well under 2× that.
        const float keep = (pass == 0 ? 3.5f : 2.5f)
                           * std::max(wmedian(ds), 1.f);
        std::vector<Blob> kept;
        for (Blob& b : blobs)
            if (cv::norm(b.centroid - anchor) <= keep)
                kept.push_back(std::move(b));
        if (kept.size() < 10) break;        // suspiciously aggressive: skip
        const bool converged = kept.size() == blobs.size();
        blobs = std::move(kept);
        if (converged) break;
    }

    // ── 2a. Locate the double ring: RANSAC ellipse on blob centroids ─────
    // The doubles are ~20 blobs lying on one ellipse — structure that no
    // artefact reproduces.  A hull fit over all pixels would swallow any
    // leftover junk near the board, so instead sample 5 centroids, fit, and
    // score by blobs landing ON the ring plus a bonus for a concentric ring
    // at ~0.62R (the triples): only the true double ellipse sees both.
    // Everything off the winning board structure is then discarded.
    {
        const auto unitRadius = [](const cv::RotatedRect& el,
                                   const cv::Point2f& p) {
            float r, a;
            toNormalized(el, p, r, a);
            return r;
        };

        cv::RNG rng(0x5EED);
        const int       n = static_cast<int>(blobs.size());
        cv::RotatedRect best;
        double          best_score = -1.0;
        for (int it = 0; it < 500; ++it) {
            std::array<int, 5> idx{};
            for (int k = 0; k < 5; ++k) {
                bool dup = true;
                while (dup) {
                    idx[k] = rng.uniform(0, n);
                    dup = false;
                    for (int j = 0; j < k; ++j) dup |= (idx[j] == idx[k]);
                }
            }
            std::vector<cv::Point2f> sample;
            for (int k : idx) sample.push_back(blobs[k].centroid);
            cv::RotatedRect el;
            try { el = cv::fitEllipse(sample); } catch (const cv::Exception&) {
                continue;
            }
            if (el.size.width  < 40.f || el.size.height < 40.f ||
                el.size.width  > 3.f * frame_bgr.cols ||
                el.size.height > 3.f * frame_bgr.cols) continue;

            int ring = 0, inner = 0;
            for (const Blob& b : blobs) {
                const float r = unitRadius(el, b.centroid);
                if      (std::abs(r - 1.f)   < 0.07f) ++ring;
                else if (std::abs(r - 0.62f) < 0.08f) ++inner;
            }
            if (ring < 8) continue;
            const double score = ring + 1.5 * inner;
            if (score > best_score) { best_score = score; best = el; }
        }
        if (best_score < 0) {
            res.error = "double ring not found (no consistent blob ellipse)";
            return res;
        }

        // Refit on all ring inliers for stability, then keep only blobs on
        // the board: junk outside the rim disappears here.
        std::vector<cv::Point2f> ring_pts;
        for (const Blob& b : blobs)
            if (std::abs(unitRadius(best, b.centroid) - 1.f) < 0.07f)
                ring_pts.push_back(b.centroid);
        if (ring_pts.size() >= 5) best = cv::fitEllipse(ring_pts);

        std::vector<Blob> kept;
        for (Blob& b : blobs)
            if (unitRadius(best, b.centroid) <= 1.10f)
                kept.push_back(std::move(b));
        if (kept.size() >= 10) blobs = std::move(kept);
    }

    // ── 2b. Board ellipse from the cleaned cloud, with outlier-reject ────
    auto fitBoardEllipse = [&](const std::vector<const Blob*>& use)
            -> cv::RotatedRect {
        std::vector<cv::Point> pts;
        for (const Blob* b : use) {
            cv::Mat m = blobMask(*b, red_cc, green_cc);
            std::vector<cv::Point> nz;
            cv::findNonZero(m, nz);
            pts.insert(pts.end(), nz.begin(), nz.end());
        }
        std::vector<cv::Point> hull;
        cv::convexHull(pts, hull);
        return cv::fitEllipse(hull);
    };

    std::vector<const Blob*> all;
    for (const Blob& b : blobs) all.push_back(&b);
    cv::RotatedRect ellipse = fitBoardEllipse(all);

    for (int pass = 0; pass < 2; ++pass) {
        std::vector<const Blob*> kept;
        for (const Blob& b : blobs) {
            float r, a;
            toNormalized(ellipse, b.centroid, r, a);
            if (r <= 1.08f) kept.push_back(&b);
        }
        if (kept.size() < 10) break;
        ellipse = fitBoardEllipse(kept);
    }

    // Fill normalised coords; drop blobs clearly outside the board.
    {
        std::vector<Blob> kept;
        for (Blob& b : blobs) {
            toNormalized(ellipse, b.centroid, b.r_n, b.angle);
            if (b.r_n <= 1.08f) kept.push_back(b);
        }
        blobs = std::move(kept);
    }

    // ── 3. Bullseye / bull = innermost red / green blobs ─────────────────
    // Under strong perspective the board centre lands well away from the
    // ellipse centre, so allow a generous search radius.
    const Blob* bullseye = nullptr;
    const Blob* bull     = nullptr;
    for (const Blob& b : blobs) {
        if (b.r_n > 0.45f) continue;
        if (b.is_red)  { if (!bullseye || b.r_n < bullseye->r_n) bullseye = &b; }
        else           { if (!bull     || b.r_n < bull->r_n)     bull     = &b; }
    }
    if (!bullseye || !bull) {
        res.error = "bullseye / bull not found near board centre";
        return res;
    }

    // ── 3b. Re-normalise radii ALONG THE RAY from the bullseye ────────────
    // The ellipse centre sits far from the true board centre under strong
    // perspective, which crushes the far-side radii (triples there fell
    // below any fixed cutoff).  Measuring "fraction of the distance from the
    // bullseye to the ellipse along this ray" is tilt-robust: doubles stay
    // ≈0.97, triples ≈0.6 all around the board.
    const float el_th = ellipse.angle / DEG;
    const float el_ca = std::cos(el_th), el_sa = std::sin(el_th);
    const auto toUnit = [&](const cv::Point2f& p) {
        const cv::Point2f d = p - ellipse.center;
        return cv::Point2f(
            ( el_ca * d.x + el_sa * d.y) / (ellipse.size.width  * 0.5f),
            (-el_sa * d.x + el_ca * d.y) / (ellipse.size.height * 0.5f));
    };
    const cv::Point2f c_u = toUnit(bullseye->centroid);
    const auto rayNorm = [&](const cv::Point2f& p, float& r_n, float& angle) {
        const cv::Point2f d = toUnit(p) - c_u;
        const float dn = std::sqrt(d.x * d.x + d.y * d.y);
        if (dn < 1e-4f) { r_n = 0.f; angle = 0.f; return; }
        const cv::Point2f u = d / dn;
        // |c_u + t·u| = 1  →  t = -c·u + sqrt((c·u)² - |c|² + 1)
        const float cu   = c_u.x * u.x + c_u.y * u.y;
        const float disc = cu * cu - (c_u.x * c_u.x + c_u.y * c_u.y) + 1.f;
        const float t    = -cu + std::sqrt(std::max(disc, 1e-6f));
        r_n = dn / std::max(t, 1e-4f);
        // direction back in image orientation, circularised
        const float xi = el_ca * u.x - el_sa * u.y;
        const float yi = el_sa * u.x + el_ca * u.y;
        angle = std::fmod(std::atan2(xi, -yi) * DEG + 360.f, 360.f);
    };
    for (Blob& b : blobs) rayNorm(b.centroid, b.r_n, b.angle);

    // ── 4. Triple vs double bands: largest radial gap splits the rings ───
    std::vector<Blob*> rings;
    for (Blob& b : blobs) {
        if (&b == bullseye || &b == bull) continue;
        if (b.r_n > 0.40f && b.r_n <= 1.10f) rings.push_back(&b);
    }
    if (rings.size() < 8) {
        res.error = "too few ring segments (" + std::to_string(rings.size()) +
                    ") to classify triples/doubles";
        return res;
    }
    {
        std::vector<float> rs;
        for (const Blob* b : rings) rs.push_back(b->r_n);
        std::sort(rs.begin(), rs.end());
        float split = 0.8f, best_gap = 0.f;
        for (size_t i = 0; i + 1 < rs.size(); ++i) {
            const float gap = rs[i + 1] - rs[i];
            if (gap > best_gap) { best_gap = gap; split = (rs[i] + rs[i+1]) / 2; }
        }
        for (Blob* b : rings) b->band = (b->r_n < split) ? 1 : 2;
    }

    // ── 5. Angular grid: offset that best matches centres to k·18° ───────
    float best_off = 0.f;
    {
        double best_cost = 1e18;
        for (float off = 0.f; off < 18.f; off += 0.25f) {
            double cost = 0;
            for (const Blob* b : rings) {
                const float rel = std::fmod(b->angle - off + 360.f, 18.f);
                const float d   = std::min(rel, 18.f - rel);
                cost += d * d * b->area;
            }
            if (cost < best_cost) { best_cost = cost; best_off = off; }
        }
        for (Blob* b : rings) {
            b->slot = static_cast<int>(std::lround((b->angle - best_off) / 18.f))
                      % 20;
            if (b->slot < 0) b->slot += 20;
        }
    }

    // ── 6. Orientation: which slot is sector 20? ──────────────────────────
    // Default: the slot whose centre is closest to image-up, snapped to red
    // parity (sector 20's rings are red, colours alternate every sector).
    int slot20;
    if (opt.sector20_hint.x >= 0.f) {
        float r, a;
        rayNorm(opt.sector20_hint, r, a);
        slot20 = static_cast<int>(std::lround((a - best_off) / 18.f)) % 20;
        if (slot20 < 0) slot20 += 20;
    } else {
        slot20 = static_cast<int>(std::lround((360.f - best_off) / 18.f)) % 20;
        // Colour-parity vote: relative slot even ⇒ red expected.
        int agree = 0, total = 0;
        for (const Blob* b : rings) {
            const int rel = (b->slot - slot20 + 20) % 20;
            const bool expect_red = (rel % 2 == 0);
            ++total;
            if (b->is_red == expect_red) ++agree;
        }
        if (total > 0 && agree * 2 < total) {
            // Parity wrong: sector 20 is the *neighbour* slot; pick the one
            // whose centre is nearer to image-up.
            const float a_prev = angDiff(best_off + (slot20 - 1) * 18.f, 0.f);
            const float a_next = angDiff(best_off + (slot20 + 1) * 18.f, 0.f);
            slot20 = (a_prev < a_next) ? slot20 - 1 : slot20 + 1;
            slot20 = (slot20 % 20 + 20) % 20;
        }
    }
    slot20 = ((slot20 + opt.sector20_offset) % 20 + 20) % 20;

    // ── 7. RANSAC homography from blob centroids ↔ board positions ───────
    constexpr float TRIPLE_MID = (board::TRIPLE_INNER + board::TRIPLE_OUTER) / 2;
    constexpr float DOUBLE_MID = (board::DOUBLE_INNER + board::DOUBLE_OUTER) / 2;

    // Rebase b->slot to "relative to sector 20" once and for all.
    for (Blob* b : rings) b->slot = (b->slot - slot20 + 20) % 20;

    // One correspondence per (band, sector): a segment often fragments into
    // several blobs (wire shadows), so merge them with an area-weighted
    // centroid instead of feeding every fragment as its own point.
    std::vector<cv::Point2f> img_pts, brd_pts;
    int n_triple = 0, n_double = 0;
    const auto buildCorrespondences = [&]() {
        std::array<std::array<cv::Point2d, 20>, 2> seg_sum{};
        std::array<std::array<double, 20>, 2>      seg_w{};
        for (const Blob* b : rings) {
            if (b->band < 1) continue;
            seg_sum[b->band - 1][b->slot] += cv::Point2d(b->centroid) * b->area;
            seg_w  [b->band - 1][b->slot] += b->area;
        }
        img_pts.clear(); brd_pts.clear();
        n_triple = n_double = 0;
        for (int band = 1; band <= 2; ++band) {
            const float mid = (band == 1) ? TRIPLE_MID : DOUBLE_MID;
            for (int rel = 0; rel < 20; ++rel) {
                const double w = seg_w[band - 1][rel];
                if (w <= 0) continue;
                img_pts.emplace_back(
                    static_cast<float>(seg_sum[band - 1][rel].x / w),
                    static_cast<float>(seg_sum[band - 1][rel].y / w));
                brd_pts.push_back(boardPt(rel * 18.f, mid));
                (band == 1 ? n_triple : n_double)++;
            }
        }
        img_pts.push_back(bullseye->centroid);
        brd_pts.push_back({0.f, 0.f});
    };

    buildCorrespondences();
    cv::Mat inlier_mask;
    cv::Mat H = cv::findHomography(img_pts, brd_pts, cv::RANSAC, 10.0,
                                   inlier_mask);
    if (H.empty()) {
        res.error = "homography estimation failed";
        return res;
    }

    // Refine: the initial band/slot assignment leaned on the ellipse
    // approximation, whose angles drift under strong perspective (the board
    // centre does NOT project to the ellipse centre).  Re-assign every ring
    // blob through the homography itself — true board mm radius + angle —
    // drop whatever falls outside the two ring bands (rim logos, text), and
    // re-fit.  Two rounds are plenty.
    for (int it = 0; it < 2; ++it) {
        std::vector<cv::Point2f> cents, proj;
        for (const Blob* b : rings) cents.push_back(b->centroid);
        cv::perspectiveTransform(cents, proj, H);
        for (size_t k = 0; k < rings.size(); ++k) {
            Blob* b = rings[k];
            const float r_mm = std::sqrt(proj[k].x * proj[k].x +
                                         proj[k].y * proj[k].y);
            const float ang  = std::fmod(
                std::atan2(proj[k].x, proj[k].y) * DEG + 360.f, 360.f);
            b->slot = static_cast<int>(std::lround(ang / 18.f)) % 20;
            if      (r_mm >  85.f && r_mm < 120.f) b->band = 1;
            else if (r_mm > 145.f && r_mm < 182.f) b->band = 2;
            else                                   b->band = -1;  // logo/noise
        }
        buildCorrespondences();
        H = cv::findHomography(img_pts, brd_pts, cv::RANSAC, 6.0, inlier_mask);
        if (H.empty()) {
            res.error = "homography refinement failed";
            return res;
        }
    }
    rings.erase(std::remove_if(rings.begin(), rings.end(),
                               [](const Blob* b) { return b->band < 1; }),
                rings.end());

    BoardCalibration calib;
    calib.homography_img_to_board = H;
    calib.homography_board_to_img = H.inv();
    calib.image_width             = frame_bgr.cols;
    calib.image_height            = frame_bgr.rows;
    calib.orientation_deg         = 0.f;
    {
        std::vector<cv::Point2f> in{{0.f, 0.f}}, out;
        cv::perspectiveTransform(in, out, calib.homography_board_to_img);
        calib.bullseye_pixel = out.front();
    }

    // Mean reprojection error over inliers (board mm → px scale ≈ ellipse).
    {
        std::vector<cv::Point2f> proj;
        cv::perspectiveTransform(brd_pts, proj, calib.homography_board_to_img);
        double sum = 0; int n = 0;
        for (size_t i = 0; i < proj.size(); ++i) {
            if (!inlier_mask.empty() && !inlier_mask.at<uchar>(int(i))) continue;
            sum += cv::norm(proj[i] - img_pts[i]);
            ++n;
        }
        res.mean_reproj_err_px = n ? static_cast<float>(sum / n) : -1.f;
    }

    // ── 8. Watershed markers ──────────────────────────────────────────────
    // Ring blobs seed their own zones (pixel-accurate already); singles are
    // seeded from homography-projected safe interiors; everything well
    // outside the board seeds MISS.  Watershed then snaps all boundaries to
    // the real wire gradients.
    constexpr int OUTSIDE_ID = 9999;
    cv::Mat markers(frame_bgr.size(), CV_32S, cv::Scalar(0));

    auto seedBlob = [&](const Blob& b, int id) {
        cv::Mat m = blobMask(b, red_cc, green_cc);
        cv::Mat er;
        cv::erode(m, er, k3);
        if (cv::countNonZero(er) < 4) er = m;   // tiny blob: keep as-is
        markers.setTo(id, er);
    };

    std::array<std::array<bool, 20>, 2> band_seeded{};   // [band-1][rel slot]
    for (const Blob* b : rings) {
        const int val = board::SECTORS[b->slot];
        const int id  = (b->band == 1 ? ZoneMap::TRIPLE_BASE
                                      : ZoneMap::DOUBLE_BASE) + val;
        seedBlob(*b, id);
        band_seeded[b->band - 1][b->slot] = true;
    }
    seedBlob(*bullseye, ZoneMap::BULL);
    seedBlob(*bull,     ZoneMap::OUTER_BULL);

    // Sector polygon (board mm) → image polygon via the homography.
    auto projectSectorPatch = [&](int rel, float r0, float r1,
                                  float ang_margin) {
        const float a0 = rel * 18.f - 9.f + ang_margin;
        const float a1 = rel * 18.f + 9.f - ang_margin;
        std::vector<cv::Point2f> poly;
        for (float a = a0; a <= a1 + 0.01f; a += 3.f) poly.push_back(boardPt(a, r0));
        for (float a = a1; a >= a0 - 0.01f; a -= 3.f) poly.push_back(boardPt(a, r1));
        std::vector<cv::Point2f> img;
        cv::perspectiveTransform(poly, img, calib.homography_board_to_img);
        std::vector<cv::Point> ipoly;
        for (const auto& p : img) ipoly.emplace_back(cvRound(p.x), cvRound(p.y));
        return ipoly;
    };

    for (int rel = 0; rel < 20; ++rel) {
        const int val = board::SECTORS[rel];
        // Singles: inner + outer patch, same id (one scoring zone).
        for (const auto& patch :
             {projectSectorPatch(rel, board::BULL_RADIUS + 7.f,
                                      board::TRIPLE_INNER - 7.f, 3.5f),
              projectSectorPatch(rel, board::TRIPLE_OUTER + 7.f,
                                      board::DOUBLE_INNER - 7.f, 3.5f)}) {
            cv::fillPoly(markers, std::vector<std::vector<cv::Point>>{patch},
                         cv::Scalar(ZoneMap::SINGLE_BASE + val));
        }
        // Ring zones the colour pass missed: synthesise a thin seed so the
        // zone still exists instead of being swallowed by its neighbours.
        if (!band_seeded[0][rel]) {
            const auto patch = projectSectorPatch(
                rel, board::TRIPLE_INNER + 2.f, board::TRIPLE_OUTER - 2.f, 4.f);
            cv::fillPoly(markers, std::vector<std::vector<cv::Point>>{patch},
                         cv::Scalar(ZoneMap::TRIPLE_BASE + val));
        }
        if (!band_seeded[1][rel]) {
            const auto patch = projectSectorPatch(
                rel, board::DOUBLE_INNER + 2.f, board::DOUBLE_OUTER - 2.f, 4.f);
            cv::fillPoly(markers, std::vector<std::vector<cv::Point>>{patch},
                         cv::Scalar(ZoneMap::DOUBLE_BASE + val));
        }
    }

    // MISS region: everything beyond the convex hull of the detected ring
    // blobs (≈ the double-outer wire), with a small safety dilation.  Pixels
    // past the double wire are misses anyway, and staying in image space
    // avoids the degenerate projection of a board circle seen at a grazing
    // angle (far rim near the horizon line).
    {
        std::vector<cv::Point> pts, hull;
        for (const Blob* b : rings) {
            cv::Mat m = blobMask(*b, red_cc, green_cc);
            std::vector<cv::Point> nz;
            cv::findNonZero(m, nz);
            pts.insert(pts.end(), nz.begin(), nz.end());
        }
        cv::convexHull(pts, hull);
        cv::Mat inside(frame_bgr.size(), CV_8U, cv::Scalar(0));
        cv::fillConvexPoly(inside, hull, cv::Scalar(255));
        cv::dilate(inside, inside,
                   cv::getStructuringElement(cv::MORPH_ELLIPSE, {9, 9}));
        markers.setTo(OUTSIDE_ID, (inside == 0) & (markers == 0));
    }

    cv::watershed(blurred, markers);

    // ── 9. Markers → CV_16U label map; resolve watershed boundary pixels ─
    cv::Mat labels(frame_bgr.size(), CV_16U, cv::Scalar(ZoneMap::MISS));
    for (int y = 0; y < markers.rows; ++y) {
        const int* mr = markers.ptr<int>(y);
        auto*      lr = labels.ptr<uint16_t>(y);
        for (int x = 0; x < markers.cols; ++x) {
            const int v = mr[x];
            if (v > 0 && v != OUTSIDE_ID) lr[x] = static_cast<uint16_t>(v);
        }
    }
    // Boundary (-1) pixels: take any labelled 8-neighbour, a few passes.
    for (int pass = 0; pass < 3; ++pass) {
        cv::Mat next = labels.clone();
        bool changed = false;
        for (int y = 0; y < markers.rows; ++y) {
            for (int x = 0; x < markers.cols; ++x) {
                if (markers.at<int>(y, x) != -1) continue;
                if (labels.at<uint16_t>(y, x) != ZoneMap::MISS) continue;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int ny = y + dy, nx = x + dx;
                        if (ny < 0 || nx < 0 || ny >= labels.rows ||
                            nx >= labels.cols) continue;
                        const uint16_t l = labels.at<uint16_t>(ny, nx);
                        if (l != ZoneMap::MISS) {
                            next.at<uint16_t>(y, x) = l;
                            changed = true;
                            dy = dx = 2;
                        }
                    }
                }
            }
        }
        labels = next;
        if (!changed) break;
    }

    // Diagnostic masks: restrict to the blobs the calibration actually used
    // (rings + bull), so the tool's mask view shows the cleaned detection —
    // dropped artefacts and rim logos simply disappear from it.
    {
        cv::Mat red_used  (frame_bgr.size(), CV_8U, cv::Scalar(0));
        cv::Mat green_used(frame_bgr.size(), CV_8U, cv::Scalar(0));
        const auto addBlob = [&](const Blob& b) {
            (b.is_red ? red_used : green_used)
                .setTo(255, blobMask(b, red_cc, green_cc));
        };
        for (const Blob* b : rings) addBlob(*b);
        addBlob(*bullseye);
        addBlob(*bull);
        res.red_mask   = red_used;
        res.green_mask = green_used;
    }

    res.zone_map.setLabels(labels);
    res.calibration   = calib;
    res.triples_found = n_triple;
    res.doubles_found = n_double;
    if (n_triple != 20 || n_double != 20)
        res.warning = "found " + std::to_string(n_triple) + "/20 triples, " +
                      std::to_string(n_double) + "/20 doubles — missing ring "
                      "segments were seeded from the homography";
    res.ok = true;
    return res;
}

} // namespace camdetect
