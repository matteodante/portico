# Third-party notices

Portico's own code is under the [Mozilla Public License 2.0](LICENSE). It uses or ships with the following components.

| Component | Where | License | How it is used |
|---|---|---|---|
| [Microsoft DirectX SDK samples](https://github.com/microsoft/DirectX-SDK-Samples): Direct3D 9 tutorials 1–5 and `banana.bmp` | `samples/Tut0*` | MIT, © Microsoft Corporation ([`samples/LICENSE-Microsoft-DirectX-SDK-Samples.txt`](samples/LICENSE-Microsoft-DirectX-SDK-Samples.txt)) | Demos and test programs, unchanged |
| [MinGW-w64 DirectX headers](https://github.com/Joshua-Ashton/mingw-directx-headers) (from MinGW-w64, originally Wine's) | `external/mingw-directx-headers` (git submodule, not part of Portico's tree) | LGPL-2.1-or-later | `d3d9.h`, `d3d9types.h` and `d3d9caps.h` for browser builds only: declarations, constants and structure layouts. Windows builds use the Windows SDK's headers instead. The same arrangement as [DXVK](https://github.com/doitsujin/dxvk) |
| [ANGLE](https://chromium.googlesource.com/angle/angle) | native Windows builds only, not shipped | BSD-3-Clause | OpenGL ES 3 for the native build |
| [Emscripten](https://emscripten.org) runtime, including Node.js-derived code | browser builds | MIT / University of Illinois NCSA | Linked into browser builds; full text in [`licenses/EMSCRIPTEN.txt`](licenses/EMSCRIPTEN.txt) |
| [musl](https://musl.libc.org) C runtime and math | browser builds | MIT and SunPro/SunSoft notices | Full texts in [`licenses/musl.txt`](licenses/musl.txt) and [`licenses/musl-math.txt`](licenses/musl-math.txt) |
| [zlib](https://zlib.net) | browser builds (Emscripten port); native when found | zlib license | PNG decoding in `portico::d3dx`; [`license text`](licenses/zlib.txt) |
| [Playwright](https://playwright.dev) | `tools/`, development only | Apache-2.0 | Browser screenshots in the tests |
| [Pillow](https://python-pillow.org) | `tools/`, development only | MIT-CMU (HPND) | Screenshot comparison |

## Clean-room notes

- `modules/d3dx` (math, headers) was written from the public D3DX documentation and standard linear algebra; it is
  tested against mathematical properties. Where the documentation leaves behavior open, tests also use
  outputs obtained by calling the installed Microsoft D3DX DLL as a black box; see [Testing](docs/testing.md).
- `modules/win32/include` was written from the public Windows documentation (type layouts, constant values).
- The fixed-function shader generator of `modules/d3d9` follows the Direct3D 9 documentation of the fixed-function
  pipeline; its structure was compared with Wine's wined3d and with LostMyCode/d3d9-webgl, and no code was copied
  from either.
