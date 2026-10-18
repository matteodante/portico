# Changelog

All notable changes are listed here. The project follows [Semantic Versioning](https://semver.org); until 1.0 a
minor version may change the API.

## [Unreleased]

### Fixed
- Package browser samples with the Emscripten, musl and zlib license notices; preserve the Emscripten notice in
  generated JavaScript. The optional GitHub Pages deployment is manual while the demo is hosted on matteodante.it.
- Describe the D3DX black-box expected values and the MPL's distribution condition accurately.

### Added
- `portico::d3dx`: `D3DXMatrixTransformation`, `D3DXMatrixTransformation2D`, `D3DXMatrixDecompose`,
  `D3DXQuaternionLn`, and the ray tests `D3DXIntersectTri` and `D3DXSphereBoundProbe` (new `d3dx9mesh.h`), with
  unit tests.
- `d3dx9core.h` and `d3dx9shader.h`, so that programs with a fixed-function fallback link: `D3DXAssembleShader`,
  `D3DXSaveSurfaceToFileA` and `D3DXSaveSurfaceToFileInMemory` fail with `E_NOTIMPL` and give back no buffer.

## [0.1.0] - 2026-10-16

First release: the Direct3D 9 module.

### Added
- `portico::d3d9`: Direct3D 9 fixed-function pipeline on OpenGL ES 3 (WebGL2 in the browser, ANGLE natively);
  multithreaded devices; the program on the page's main thread, with the frame presented through JSPI.
- `portico::d3dx`: D3DX 9 math and texture loading (DDS, TGA, BMP, PNG).
- `portico::win32`: the Win32 subset for browser builds (windows on the canvas, messages, input, timers, entry
  point), and the Direct3D 9 headers there (MinGW-w64, submodule).
- Microsoft's Direct3D 9 tutorials 1–5, built unchanged, as samples and live demo.
- Tests: D3DX math unit tests; screenshot parity of the samples against the system's Direct3D 9 (native and
  browser), checked in CI with Chromium.

[0.1.0]: https://github.com/matteodante/portico/releases/tag/v0.1.0
