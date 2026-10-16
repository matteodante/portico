"""Compares a captured frame with a Direct3D 9 reference shot: "the same to the eye".

    python tools/compare_shots.py <reference.png> <capture.png> [--diff out.png] [--json out.json]
                                  [--baseline <other D3D9 frames of the same scene> ...]
                                  [--ignore x0,y0,x1,y1 ...] [--animated x0,y0,x1,y1 ...]

Both images are reduced to a quarter of their size with a box filter, so sub-pixel rasterization and
filtering differences do not count. Then two criteria must both hold:

  Global:  mean absolute difference <= 8 (of 255, averaged over RGB) and at most 5% of the reduced
           pixels with a channel difference > 40.
  Local:   the reduced image is cut into tiles of 8x8 reduced pixels (32x32 on screen); in every
           tile the mean difference must stay <= 20 and at most 25% of its pixels may differ by > 40.
           A missing panel, a missing object or a wrong texture on one object fails here even when the
           global numbers are small.

Animated scenes (clouds, idle animations, water) differ from themselves over time. --baseline
takes other D3D9 frames of the same scene; for each tile (and for the global numbers) the allowed
value becomes the larger of the fixed limit and 1.25 x the largest difference measured between the
reference and those frames. Tiles that do not vary between D3D9 frames keep the fixed limit.

Nearest frame: with --baseline the capture is also compared with the fixed limits only (nothing
widened) against each of the reference and the baseline frames. It passes when one single frame
satisfies the global and the local criteria together: an animated scene caught at another phase
than the reference matches the D3D9 frame of its own phase, and an object of the scene must be
where that frame has it. Only inside the rectangles given with --animated (independent animations,
such as clouds and particles, rarely all in one frame's phase) a tile may match any frame. The
capture passes when either rule passes; a defect fails both, since it differs from every D3D9 frame
in the same place (or from the one frame that matches the rest).

--ignore excludes declared rectangles (screen pixels) whose content is expected to differ, such as
a clock or a user name. Every ignored rectangle is listed in
the JSON result.
"""
import argparse
import json
import sys

from PIL import Image, ImageChops, ImageDraw, ImageStat

MEAN_LIMIT = 8.0
BAD_DELTA = 40
BAD_LIMIT = 0.05
TILE = 8                # reduced pixels per tile side (32 screen pixels)
TILE_MEAN_LIMIT = 20.0
TILE_BAD_LIMIT = 0.25
BASELINE_MARGIN = 1.25


def load(path, size=None):
    img = Image.open(path).convert("RGB")
    if size and img.size != size:
        img = img.resize(size, Image.BILINEAR)
    return img


def reduce(img):
    return img.resize((max(1, img.width // 4), max(1, img.height // 4)), Image.BOX)


def blank(img, rects):
    """Paints the ignored rectangles black, so both images agree there."""
    if not rects:
        return img
    img = img.copy()
    d = ImageDraw.Draw(img)
    for r in rects:
        d.rectangle(r, fill=(0, 0, 0))
    return img


def measure(ref_small, cap_small):
    """Global (mean, bad) and per-tile [(mean, bad)] of two reduced images."""
    diff = ImageChops.difference(ref_small, cap_small)
    ch = diff.split()
    worst = ImageChops.lighter(ImageChops.lighter(ch[0], ch[1]), ch[2])
    w, h = diff.size
    mean = sum(ImageStat.Stat(diff).mean) / 3.0
    bad = sum(worst.histogram()[BAD_DELTA + 1:]) / float(w * h)
    tiles = []
    for ty in range(0, h, TILE):
        for tx in range(0, w, TILE):
            box = (tx, ty, min(w, tx + TILE), min(h, ty + TILE))
            n = (box[2] - box[0]) * (box[3] - box[1])
            tmean = sum(ImageStat.Stat(diff.crop(box)).mean) / 3.0
            tbad = sum(worst.crop(box).histogram()[BAD_DELTA + 1:]) / float(n)
            tiles.append((box, tmean, tbad))
    return mean, bad, tiles


def overlaps(box, rects):
    """Whether a tile (reduced pixels) touches one of the rectangles (screen pixels)."""
    x0, y0, x1, y1 = (v * 4 for v in box)
    return any(x0 < r[2] and r[0] < x1 and y0 < r[3] and r[1] < y1 for r in rects)


def compare(reference, capture, baseline=(), ignore=(), animated=()):
    ref = load(reference)
    cap = load(capture, ref.size)
    rects = [tuple(r) for r in ignore]
    ref_s = reduce(blank(ref, rects))
    cap_s = reduce(blank(cap, rects))
    mean, bad, tiles = measure(ref_s, cap_s)

    base_s = [reduce(blank(load(b, ref.size), rects)) for b in baseline]
    base = [measure(ref_s, b) for b in base_s]
    mean_limit = max([MEAN_LIMIT] + [BASELINE_MARGIN * m for m, _, _ in base])
    bad_limit = max([BAD_LIMIT] + [BASELINE_MARGIN * b for _, b, _ in base])

    failed = []
    widened = 0
    for i, (box, tmean, tbad) in enumerate(tiles):
        lm = max([TILE_MEAN_LIMIT] + [BASELINE_MARGIN * t[i][1] for _, _, t in base])
        lb = max([TILE_BAD_LIMIT] + [BASELINE_MARGIN * t[i][2] for _, _, t in base])
        if lm > TILE_MEAN_LIMIT or lb > TILE_BAD_LIMIT:
            widened += 1
        if tmean > lm or tbad > lb:
            failed.append({"tile_screen": [v * 4 for v in box], "mean": round(tmean, 2), "bad": round(tbad, 3),
                           "limit_mean": round(lm, 2), "limit_bad": round(lb, 3)})
    worst_tile = max(tiles, key=lambda t: t[1])
    ref_ok = mean <= mean_limit and bad <= bad_limit and not failed

    # Nearest D3D9 frame (reference included), fixed limits: one frame must satisfy the global and the
    # local criteria together. Tiles in the declared animated rectangles may match any frame instead.
    nearest = None
    if base_s:
        anim = [tuple(r) for r in animated]
        cands = [(reference, ref_s)] + list(zip(baseline, base_s))
        meas = [(name, measure(frame, cap_s)) for name, frame in cands]
        tile_ok = lambda m, i: m[2][i][1] <= TILE_MEAN_LIMIT and m[2][i][2] <= TILE_BAD_LIMIT
        free = [overlaps(tiles[i][0], anim) for i in range(len(tiles))]
        anim_failed = sum(1 for i in range(len(tiles)) if free[i] and not any(tile_ok(m, i) for _, m in meas))
        best = None
        for name, m in meas:
            nfail = anim_failed + sum(1 for i in range(len(tiles)) if not free[i] and not tile_ok(m, i))
            ok = m[0] <= MEAN_LIMIT and m[1] <= BAD_LIMIT and nfail == 0
            rank = (not ok, nfail, m[0])
            if best is None or rank < best[0]:
                best = (rank, name, m, nfail, ok)
        _, gname, (gmean, gbad, _), tile_failed, npass = best
        nearest = {"frame": gname, "pass": npass, "mean_abs_diff": round(gmean, 3),
                   "bad_pixel_fraction": round(gbad, 4), "failed_tile_count": tile_failed,
                   "animated": [list(r) for r in anim], "animated_tiles": sum(free)}
    ok = ref_ok or bool(nearest and nearest["pass"])
    return {
        "reference": reference,
        "capture": capture,
        "size": list(ref.size),
        "mean_abs_diff": round(mean, 3),
        "bad_pixel_fraction": round(bad, 4),
        "limits": {"mean_abs_diff": round(mean_limit, 3), "bad_pixel_fraction": round(bad_limit, 4),
                   "tile": TILE * 4, "tile_mean": TILE_MEAN_LIMIT, "tile_bad": TILE_BAD_LIMIT},
        "baseline_frames": len(base),
        "tiles": len(tiles),
        "tiles_widened_by_baseline": widened,
        "worst_tile": {"tile_screen": [v * 4 for v in worst_tile[0]], "mean": round(worst_tile[1], 2),
                       "bad": round(worst_tile[2], 3)},
        "failed_tiles": failed[:20],
        "failed_tile_count": len(failed),
        "ignored": [list(r) for r in rects],
        "reference_rule_pass": ref_ok,
        "nearest_frame": nearest,
        "pass": ok,
    }, ref, cap


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference")
    ap.add_argument("capture")
    ap.add_argument("--diff")
    ap.add_argument("--json")
    ap.add_argument("--baseline", nargs="*", default=[], help="other D3D9 frames of the same scene")
    ap.add_argument("--ignore", nargs="*", default=[], help="x0,y0,x1,y1 rectangles expected to differ")
    ap.add_argument("--animated", nargs="*", default=[],
                    help="x0,y0,x1,y1 rectangles of independent animations (nearest-frame rule only)")
    a = ap.parse_args()
    ignore = [[int(v) for v in r.split(",")] for r in a.ignore]
    animated = [[int(v) for v in r.split(",")] for r in a.animated]
    result, ref, cap = compare(a.reference, a.capture, a.baseline, ignore, animated)
    if a.diff:
        img = ImageChops.difference(ref, cap).point(lambda v: min(255, v * 4))
        d = ImageDraw.Draw(img)
        for t in result["failed_tiles"]:
            d.rectangle(t["tile_screen"], outline=(255, 0, 0))
        img.save(a.diff)
    if a.json:
        with open(a.json, "w") as f:
            json.dump(result, f, indent=2)
    print(json.dumps({k: result[k] for k in ("mean_abs_diff", "bad_pixel_fraction", "limits", "baseline_frames",
                                             "tiles_widened_by_baseline", "worst_tile", "failed_tile_count",
                                             "reference_rule_pass", "nearest_frame", "pass")}))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
