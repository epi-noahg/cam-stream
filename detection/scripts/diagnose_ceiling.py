#!/usr/bin/env python3
"""Is the detector at its GEOMETRIC CEILING, or is there a software lever left?

Two measurements, both overfit-proof, that decide whether further accuracy gains
must come from algorithm or from hardware (resolution / lighting / camera count).
RE-RUN THIS AFTER ANY HARDWARE, LIGHTING, OR CALIBRATION CHANGE — the verdict is
conditioned on the current silhouette quality, which is hardware-dependent.

  (1) CALIBRATION diagnostic.  Segment ring blobs on a clean frame per camera,
      map them through the EXISTING homography to board mm, and decompose the
      residual vs ISO into a SHARED (cross-camera) component and per-camera
      SCATTER.  SHARED >> SCATTER (and residual vectors agree in direction) =>
      the real board deviates from ISO => joint bundle-adjusted calibration
      would help.  Otherwise it is per-camera noise => BA only overfits, STOP.

  (2) ALONG/ACROSS decomposition.  From a traced session replay, project each
      camera's per-dart residual (per_cam.board_xy - fused X) onto that camera's
      dart axis.  ALONG-dominated => the multi-camera crossing already
      neutralises it (and recall covers single-camera darts) => at the geometric
      ceiling, no software position lever.  ACROSS-dominated => the axis-angle /
      foreshortening term in DartDetector is still a software lever worth tuning
      before reaching for hardware.

Usage:
  python3 scripts/diagnose_ceiling.py [tests/session1.yml] [--clean-frame N]
"""
import math, os, re, subprocess, sys
import numpy as np

try:
    import cv2
except ImportError:
    sys.exit("needs opencv-python: pip install opencv-python-headless numpy")

TI, TO, DI, DO = 99.0, 107.0, 162.0, 170.0
TMID, DMID = (TI + TO) / 2, (DI + DO) / 2
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def parse_spec(path):
    txt = open(path).read()
    offs = [0, 0, 0]
    m = re.search(r"offsets:\s*((?:\s*-\s*-?\d+)+)", txt)
    if m:
        offs = [int(v) for v in re.findall(r"-\s*(-?\d+)", m.group(1))][:3]
    vids = re.findall(r'videos:\s*((?:\s*-\s*"[^"]+"\s*)+)', txt)
    videos = re.findall(r'"([^"]+)"', vids[0]) if vids else []
    cal = re.findall(r'calibs:\s*((?:\s*-\s*"[^"]+"\s*)+)', txt)
    calibs = re.findall(r'"([^"]+)"', cal[0]) if cal else []
    return offs, videos[:3], calibs[:3]


def load_H(calib_path):
    fs = cv2.FileStorage(calib_path, cv2.FILE_STORAGE_READ)
    H = fs.getNode("homography_img_to_board").mat()
    fs.release()
    return H


def segment_blobs(bgr, red_a=16, green_a=12, min_chroma=16, min_area=15):
    blur = cv2.GaussianBlur(bgr, (3, 3), 0)
    lab = cv2.cvtColor(blur, cv2.COLOR_BGR2Lab)
    a = lab[:, :, 1].astype(np.float32) - 128
    b = lab[:, :, 2].astype(np.float32) - 128
    chroma = np.sqrt(a * a + b * b)
    red = ((a > red_a) & (chroma > min_chroma) & (b > -5)).astype(np.uint8) * 255
    grn = ((a < -green_a) & (chroma > min_chroma) & (b > -5)).astype(np.uint8) * 255
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    out = []
    for m in (red, grn):
        m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, k)
        n, _, stats, cc = cv2.connectedComponentsWithStats(m, 8)
        for j in range(1, n):
            if stats[j, cv2.CC_STAT_AREA] >= min_area:
                out.append((cc[j][0], cc[j][1], float(stats[j, cv2.CC_STAT_AREA])))
    return out


def calibration_diag(videos, calibs, clean_frame):
    Hs = [load_H(c) for c in calibs]
    data = {}
    for cam in range(3):
        cap = cv2.VideoCapture(videos[cam])
        cap.set(cv2.CAP_PROP_POS_FRAMES, clean_frame)
        ok, f = cap.read()
        cap.release()
        if not ok:
            continue
        for px, py, area in segment_blobs(f):
            q = cv2.perspectiveTransform(np.array([[[px, py]]], np.float64), Hs[cam])
            bx, by = q[0, 0]
            r = math.hypot(bx, by)
            if TI - 6 <= r <= TO + 6:
                ring, rmid = 1, TMID
            elif DI - 6 <= r <= DO + 6:
                ring, rmid = 2, DMID
            else:
                continue
            slot = int(round((math.degrees(math.atan2(bx, by)) % 360) / 18)) % 20
            deg = math.radians(slot * 18)
            iso = np.array([rmid * math.sin(deg), rmid * math.cos(deg)])
            data.setdefault((slot, ring), []).append((bx - iso[0], by - iso[1], area))

    shared, scatter, w, cos = [], [], [], []
    for vs in data.values():
        if len(vs) < 2:
            continue
        V = np.array([[dx, dy] for dx, dy, a in vs])
        A = np.array([a for dx, dy, a in vs])
        mean = V.mean(0)
        shared.append(np.linalg.norm(mean))
        scatter.append(math.sqrt(((V - mean) ** 2).sum(1).mean()))
        w.append(A.sum())
        for i in range(len(V)):
            for j in range(i + 1, len(V)):
                ni, nj = np.linalg.norm(V[i]), np.linalg.norm(V[j])
                if ni > 1 and nj > 1:
                    cos.append(float(V[i] @ V[j] / (ni * nj)))
    if not shared:
        print("  (no ring blobs found — check clean frame / thresholds)")
        return
    w = np.array(w)
    sh, sc = np.average(shared, weights=w), np.average(scatter, weights=w)
    cm = float(np.mean(cos)) if cos else 0.0
    print(f"  locations (>=2 cams): {len(shared)}")
    print(f"  SHARED  (cross-cam mean residual vs ISO): {sh:.2f} mm")
    print(f"  SCATTER (per-cam noise around mean):      {sc:.2f} mm")
    print(f"  residual-vector direction agreement (cos): {cm:.2f}")
    verdict = ("SHARED board-vs-ISO bias -> JOINT BUNDLE ADJUSTMENT would help"
               if sh > 1.3 * sc and cm > 0.3
               else "per-camera noise -> bundle adjustment would OVERFIT, STOP")
    print(f"  ratio SHARED/SCATTER = {sh / max(sc, 0.01):.2f}  =>  {verdict}")


def along_across_diag(spec):
    binp = os.path.join(REPO, "build", "camdetect_runtest")
    env = dict(os.environ, CAMDETECT_TRACE="1")
    out = subprocess.run([binp, spec], env=env, capture_output=True, text=True,
                         cwd=REPO).stderr
    percam, fused = [], []
    for l in out.splitlines():
        m = re.search(r"f=(\d+) cam(\d) HIT zone=\S+ xy=\(([-\d.]+),([-\d.]+)\)"
                      r".*axis=\(([-\d.]+),([-\d.]+)\)", l)
        if m:
            percam.append(tuple(float(m.group(i)) for i in (1, 3, 4, 5, 6)))
        m = re.search(r"f=(\d+) FUSED zone=\S+ xy=\(([-\d.]+),([-\d.]+)\)", l)
        if m:
            fused.append((int(m.group(1)), float(m.group(2)), float(m.group(3))))
    al, ac = [], []
    for ff, x, y, ax, ay in percam:
        cand = [g for g in fused if abs(g[0] - ff) <= 12]
        if not cand:
            continue
        g = min(cand, key=lambda g: abs(g[0] - ff))
        dx, dy = x - g[1], y - g[2]
        n = math.hypot(ax, ay) or 1
        ux, uy = ax / n, ay / n
        al.append(abs(dx * ux + dy * uy))
        ac.append(abs(dx * -uy + dy * ux))
    if not al:
        print("  (no traced hits — build camdetect_runtest first)")
        return
    al, ac = np.array(al), np.array(ac)
    print(f"  hits: {len(al)}")
    print(f"  ALONG-axis residual : mean={al.mean():.1f}mm  p90={np.percentile(al,90):.1f}mm")
    print(f"  ACROSS-axis residual: mean={ac.mean():.1f}mm  p90={np.percentile(ac,90):.1f}mm")
    r = al.mean() / max(ac.mean(), 0.01)
    verdict = ("ALONG-dominated -> crossing neutralises it; AT GEOMETRIC CEILING "
               "(remaining gains are hardware: recall needs peer-visible views)"
               if r > 1.8 else
               "ACROSS has real share -> tune the axis-angle term in DartDetector "
               "(software lever) before reaching for hardware")
    print(f"  along/across ratio = {r:.1f}  =>  {verdict}")


def main():
    args = sys.argv[1:]
    spec = "tests/session1.yml"
    clean_frame = 150
    i = 0
    while i < len(args):
        if args[i] == "--clean-frame":
            clean_frame = int(args[i + 1]); i += 2
        else:
            spec = args[i]; i += 1
    spec_path = spec if os.path.isabs(spec) else os.path.join(REPO, spec)
    offs, videos, calibs = parse_spec(spec_path)

    print("=" * 70)
    print("(1) CALIBRATION diagnostic  (board-vs-ISO bias vs per-cam noise)")
    print("=" * 70)
    if videos and calibs:
        calibration_diag(videos, calibs, clean_frame)
    else:
        print("  (spec has no video/calib paths)")
    print("\n" + "=" * 70)
    print("(2) ALONG/ACROSS decomposition  (geometric ceiling vs software lever)")
    print("=" * 70)
    along_across_diag(spec)


if __name__ == "__main__":
    main()
