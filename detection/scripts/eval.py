#!/usr/bin/env python3
"""Detection accuracy dashboard.

Runs camdetect_runtest with CAMDETECT_TRACE=1 over a test spec, parses the
per-camera and fused hits, and reports a full quality breakdown against the
spec's ground-truth darts:

  * per-camera zone accuracy (correct / wrong / missed)
  * fused zone accuracy (tight: nearest fused hit per GT dart, same round)
  * geometric zone of the fused crossing position (sanity on triangulation)
  * confidence / votes distribution
  * crossing residual = per-camera board_xy vs fused board_xy (mm)

Usage:
  python3 scripts/eval.py [tests/session1.yml] [--build-dir build] [--bin camdetect_runtest]
"""
import math, os, re, subprocess, sys

SECTORS = [20, 1, 18, 4, 13, 6, 10, 15, 2, 17, 3, 19, 7, 16, 8, 11, 14, 9, 12, 5]
BULLSEYE, BULL, TI, TO, DI, DO = 6.35, 16.0, 99.0, 107.0, 162.0, 170.0


def zlookup(x, y):
    r = math.hypot(x, y)
    if r <= BULLSEYE: return "Bull"
    if r <= BULL: return "25"
    if r > DO: return "MISS"
    deg = (90 - math.degrees(math.atan2(y, x)) + 360) % 360
    v = SECTORS[int(math.floor((deg + 9) / 18)) % 20]
    if TI <= r <= TO: return "T" + str(v)
    if DI <= r <= DO: return "D" + str(v)
    return str(v)


def canon(z):
    z = z.strip().upper()
    if z in ("B", "BULL", "DBULL", "50"): return "BULL"
    if z in ("25", "SBULL", "OUTER"): return "25"
    return z


def parse_spec(path):
    """Return (darts[(frame,zone)], clears[frame], offsets[3])."""
    darts, clears, offsets = [], [], [0, 0, 0]
    txt = open(path).read()
    moff = re.search(r"offsets:\s*((?:\s*-\s*-?\d+)+)", txt)
    if moff:
        offsets = [int(v) for v in re.findall(r"-\s*(-?\d+)", moff.group(1))][:3]
    cur = {}
    for line in txt.splitlines():
        s = line.strip()
        m = re.match(r"frame:\s*(\d+)", s)
        if m:
            if cur: _flush(cur, darts, clears); cur = {}
            cur["frame"] = int(m.group(1))
        elif s.startswith("type:"):
            cur["type"] = s.split(":", 1)[1].strip()
        elif s.startswith("zone:"):
            cur["zone"] = s.split(":", 1)[1].strip().strip('"')
    if cur: _flush(cur, darts, clears)
    return darts, sorted(clears), offsets


def _flush(cur, darts, clears):
    t = cur.get("type")
    if t == "dart" and "zone" in cur:
        darts.append((cur["frame"], cur["zone"]))
    elif t == "clear":
        clears.append(cur["frame"])


def round_of(frame, clears):
    return sum(1 for c in clears if c < frame)


def run_trace(spec, build_dir, binname):
    binpath = os.path.join(build_dir, binname)
    env = dict(os.environ, CAMDETECT_TRACE="1")
    p = subprocess.run([binpath, spec], env=env, capture_output=True, text=True,
                       cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    return p.stdout + p.stderr


PERCAM = re.compile(
    r"f=(\d+) cam(\d) HIT zone=(\S+) xy=\(([-\d.]+),([-\d.]+)\) conf=([\d.]+) "
    r"sigma=([\d.]+) shape_q=([\d.]+) margin=([\d.]+) axis=\(([-\d.]+),([-\d.]+)\) "
    r"s_along=([\d.]+) s_across=([\d.]+) sup=(\d+)/(\d+)")
FUSED = re.compile(
    r"f=(\d+) FUSED zone=(\S+) xy=\(([-\d.]+),([-\d.]+)\) conf=([\d.]+) votes=(\d)")


def main():
    args = [a for a in sys.argv[1:]]
    spec = "tests/session1.yml"
    build_dir, binname = "build", "camdetect_runtest"
    i = 0
    while i < len(args):
        if args[i] == "--build-dir": build_dir = args[i + 1]; i += 2
        elif args[i] == "--bin": binname = args[i + 1]; i += 2
        else: spec = args[i]; i += 1

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    spec_path = spec if os.path.isabs(spec) else os.path.join(repo, spec)
    darts, clears, offsets = parse_spec(spec_path)

    out = run_trace(spec, build_dir, binname)
    percam = {0: [], 1: [], 2: []}   # (master_frame, zone, conf, xy, shape_q, sup)
    fused = []                        # (frame, zone, x, y, conf, votes)
    for line in out.splitlines():
        m = PERCAM.search(line)
        if m:
            cam = int(m.group(2))
            master = int(m.group(1)) - offsets[cam]
            percam[cam].append(dict(f=master, zone=m.group(3),
                x=float(m.group(4)), y=float(m.group(5)), conf=float(m.group(6)),
                shape_q=float(m.group(8)), sup=(int(m.group(14)), int(m.group(15)))))
            continue
        m = FUSED.search(line)
        if m:
            fused.append(dict(f=int(m.group(1)), zone=m.group(2),
                x=float(m.group(3)), y=float(m.group(4)),
                conf=float(m.group(5)), votes=int(m.group(6))))

    TOL = 60
    # ---- per-camera ----
    print("=" * 78)
    print(f"SPEC {spec}   darts={len(darts)}  offsets={offsets}")
    print("=" * 78)
    pc_ok = {0: 0, 1: 0, 2: 0}; pc_seen = {0: 0, 1: 0, 2: 0}
    hdr = f"{'GT':>10} |" + "".join(f"{'cam'+str(c):>12}" for c in range(3)) + \
          f"{'FUSED':>10}{'geom':>7}{'v':>3}{'conf':>7}"
    print(hdr)
    f_ok = f_geom_ok = 0
    for gf, gz in darts:
        rnd = round_of(gf, clears)
        row = []
        for c in range(3):
            cand = sorted((h for h in percam[c]
                           if abs(h["f"] - gf) <= TOL and round_of(h["f"], clears) == rnd),
                          key=lambda h: abs(h["f"] - gf))
            if cand:
                z = cand[0]["zone"]; ok = canon(z) == canon(gz)
                pc_seen[c] += 1; pc_ok[c] += ok
                row.append(f"{z}{'OK' if ok else 'XX'}")
            else:
                row.append("--miss--")
        fc = sorted((h for h in fused
                     if abs(h["f"] - gf) <= TOL and round_of(h["f"], clears) == rnd),
                    key=lambda h: abs(h["f"] - gf))
        if fc:
            h = fc[0]; fz = h["zone"]; geom = zlookup(h["x"], h["y"])
            ok = canon(fz) == canon(gz); g_ok = canon(geom) == canon(gz)
            f_ok += ok; f_geom_ok += g_ok
            tail = f"{fz+('OK'if ok else'XX'):>10}{geom+('o'if g_ok else'x'):>7}{h['votes']:>3}{h['conf']:>7.2f}"
        else:
            tail = f"{'--MISS--':>10}{'':>7}{0:>3}{0:>7.2f}"
        print(f"{gf:>5} {gz:>4} |" + "".join(f"{r:>12}" for r in row) + tail)

    print("-" * 78)
    n = len(darts)
    for c in range(3):
        miss = n - pc_seen[c]
        acc = pc_ok[c] / n * 100
        print(f"  cam{c}: {pc_ok[c]}/{n} correct ({acc:4.0f}%)  "
              f"[{pc_ok[c]} ok / {pc_seen[c]-pc_ok[c]} wrong / {miss} missed]")
    print(f"  FUSED: {f_ok}/{n} ({f_ok/n*100:.0f}%)   "
          f"geom(crossing): {f_geom_ok}/{n} ({f_geom_ok/n*100:.0f}%)")

    # ---- confidence + residuals ----
    confs = [h["conf"] for h in fused]
    if confs:
        confs.sort()
        mean = sum(confs) / len(confs)
        med = confs[len(confs) // 2]
        lo = sum(1 for c in confs if c < 0.3)
        print(f"  fused conf: mean={mean:.2f} median={med:.2f}  <0.3: {lo}/{len(confs)}")
    # crossing residual: per-cam board_xy vs nearest fused
    res = []
    for c in range(3):
        for h in percam[c]:
            fc = [g for g in fused if abs(g["f"] - h["f"]) <= 5]
            if fc:
                g = fc[0]
                res.append(math.hypot(h["x"] - g["x"], h["y"] - g["y"]))
    if res:
        res.sort()
        print(f"  per-cam vs fused residual (mm): median={res[len(res)//2]:.1f} "
              f"p90={res[int(len(res)*0.9)]:.1f} max={res[-1]:.1f}")
    # shape_q distribution
    sq = [h["shape_q"] for c in range(3) for h in percam[c]]
    if sq:
        floored = sum(1 for s in sq if s <= 0.21)
        print(f"  shape_q: mean={sum(sq)/len(sq):.2f}  at-floor(<=0.21): {floored}/{len(sq)}")


if __name__ == "__main__":
    main()
