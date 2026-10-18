# Browser sample license notices

These notices accompany the browser sample binaries. Portico's own license is
`LICENSE.txt` in the packaged demo, and Microsoft's sample license is
`LICENSE-samples.txt`. The demo's `THIRD-PARTY-LICENSES.txt` includes this index
and every `.txt` file in this directory.

| Text | Component and source |
|---|---|
| `EMSCRIPTEN.txt` | Emscripten runtime, including its Node.js-derived code; [Emscripten 6.0.12 LICENSE](https://github.com/emscripten-core/emscripten/blob/6.0.12/LICENSE), MIT / Illinois-NCSA |
| `musl.txt` | C runtime; [Emscripten 6.0.12 musl COPYRIGHT](https://github.com/emscripten-core/emscripten/blob/6.0.12/system/lib/libc/musl/COPYRIGHT), MIT and notices of incorporated code |
| `musl-math.txt` | Additional SunPro/SunSoft notices from the math functions linked into the samples; file names are listed with each notice |
| `zlib.txt` | PNG decoding; [zlib 1.3.2 LICENSE](https://github.com/madler/zlib/blob/v1.3.2/LICENSE), zlib license |

The upstream copyright and permission notices are retained. These components keep
their own licenses; Portico's MPL-2.0 does not replace them. The SDK's LLVM runtime
libraries use Apache-2.0 with the LLVM exception, which permits their automatic
inclusion in compiled programs without attribution; see
[LLVM's license policy](https://llvm.org/docs/DeveloperPolicy.html#license).

Build the samples and run `python tools/package_demo.py` to create `build/demo/`.
Deploy the whole output directory, including the three license files, together.
If the SDK or linked components change, recheck these texts against that version.
