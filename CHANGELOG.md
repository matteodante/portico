# Changelog

All notable changes are listed here. The project follows [Semantic Versioning](https://semver.org); until 1.0 a
minor version may change the API.

## [0.1.0] - 2026-10-16

First release: the Direct3D 9 module.

### Added
- `portico::d3d9`: Direct3D 9 fixed-function pipeline on OpenGL ES 3 (WebGL2 in the browser, ANGLE natively);
  multithreaded devices; main-thread and worker setups in the browser, with the frame presented through JSPI.
- `portico::d3dx`: D3DX 9 math and texture loading (DDS, TGA, BMP, PNG).
- `portico::win32`: the Win32 subset for browser builds (windows on the canvas, messages, input, timers, entry
  point), and the Direct3D 9 headers there (MinGW-w64, submodule).
- Microsoft's Direct3D 9 tutorials 1–5, built unchanged, as samples and live demo.
- Tests: D3DX math unit tests; screenshot parity of the samples against the system's Direct3D 9 (native and
  browser), checked in CI with Chromium.

[0.1.0]: https://github.com/matteodante/portico/releases/tag/v0.1.0
