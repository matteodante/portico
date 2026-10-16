# Copyright (c) 2026 Matteo Dante
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
# not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Screenshot parity of the samples against the Direct3D 9 reference shots.

    python tools/parity.py <reference dir> <capture dir> [sample ...]

Each <sample>.png of the capture dir is compared with the reference of the same name by tools/compare_shots.py
(the same to the eye: global and per-tile limits). The 4x4 pixels at each corner are ignored: Windows 11 rounds
window corners, and the captures of the reference windows (tools/capture_window.py) show the rounding there.
Prints one line per sample and exits with 1 when any sample fails or is missing.
"""
import json
import os
import subprocess
import sys


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ref_dir, cap_dir, only = sys.argv[1], sys.argv[2], sys.argv[3:]
    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "compare_shots.py")
    names = sorted(f[:-4] for f in os.listdir(ref_dir) if f.endswith(".png"))
    if only:
        names = [n for n in names if n in only]
    failed = 0
    for name in names:
        ref, cap = os.path.join(ref_dir, name + ".png"), os.path.join(cap_dir, name + ".png")
        if not os.path.exists(cap):
            print(f"FAIL {name}: no capture")
            failed += 1
            continue
        from PIL import Image
        w, h = Image.open(ref).size
        corners = [f"0,0,4,4", f"{w - 4},0,{w},4", f"0,{h - 4},4,{h}", f"{w - 4},{h - 4},{w},{h}"]
        args = [sys.executable, tool, ref, cap]
        for c in corners:
            args += ["--ignore", c]
        out = subprocess.run(args, capture_output=True, text=True)
        try:
            r = json.loads(out.stdout.strip().splitlines()[-1])
        except (ValueError, IndexError):
            print(f"FAIL {name}: {out.stdout.strip()} {out.stderr.strip()}")
            failed += 1
            continue
        ok = r.get("pass")
        failed += 0 if ok else 1
        worst = r.get("worst_tile", {})
        print(f"{'ok  ' if ok else 'FAIL'} {name}: mean {r['mean_abs_diff']}, bad pixels {r['bad_pixel_fraction']}, "
              f"worst tile {worst.get('tile_screen')} mean {worst.get('mean')}")
    print(f"{len(names) - failed}/{len(names)} samples the same to the eye as Direct3D 9")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
