# Copyright (c) 2026 Matteo Dante
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Package the browser samples and their license notices for static hosting."""
import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
SAMPLES = (
    "tut01_createdevice", "tut02_vertices", "tut03_matrices",
    "tut04_lights", "tut05_textures",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=Path, default=ROOT / "build/web/samples")
    parser.add_argument("--out", type=Path, default=ROOT / "build/demo")
    args = parser.parse_args()
    artifacts = [args.samples / f"{sample}.{ext}" for sample in SAMPLES for ext in ("html", "js", "wasm")]
    artifacts.append(args.samples / "tut05_textures.data")
    notices = sorted((ROOT / "licenses").glob("*.txt"))
    required_notices = {"EMSCRIPTEN.txt", "musl.txt", "musl-math.txt", "zlib.txt"}
    if not required_notices <= {notice.name for notice in notices}:
        parser.error("The browser runtime license notices are incomplete")
    for artifact in artifacts:
        if not artifact.is_file():
            parser.error(f"Missing sample artifact: {artifact}")
    args.out.mkdir(parents=True, exist_ok=True)
    for artifact in artifacts:
        shutil.copy2(artifact, args.out / artifact.name)
    for source, name in (
        (ROOT / "web/index.html", "index.html"),
        (ROOT / "LICENSE", "LICENSE.txt"),
        (ROOT / "samples/LICENSE-Microsoft-DirectX-SDK-Samples.txt", "LICENSE-samples.txt"),
    ):
        shutil.copy2(source, args.out / name)
    parts = [(ROOT / "licenses/README.md").read_text(encoding="utf-8")]
    for notice in notices:
        parts.append(f"\n{'=' * 72}\n{notice.name}\n{'=' * 72}\n")
        parts.append(notice.read_text(encoding="utf-8"))
    (args.out / "THIRD-PARTY-LICENSES.txt").write_text(
        "\n".join(parts), encoding="utf-8", newline="\n",
    )
    print(f"Packaged {len(SAMPLES)} samples and {len(notices)} third-party license texts in {args.out}")


if __name__ == "__main__":
    main()
