# Supported API

What `portico::d3d9` and `portico::d3dx` implement in v0.1. This is a feature summary, not a complete per-method
conformance matrix. Future work and dependencies are tracked in the [ecosystem coverage plan](ecosystem-coverage.md)
and [roadmap](../ROADMAP.md). "Ignored" means the call succeeds and does nothing;
"refused" means the call returns a failure, allowing a fallback where the application handles it. Exact HRESULTs and
invalid-input behavior still need the per-method audit; this label alone does not establish native equivalence.
Many unsupported paths log one line the first time (`[portico-d3d9] ...` in the browser console,
`PORTICO_D3D9_LOG=<file>` natively), but some inline stubs do not log. A successful return alone does not establish support.

## Direct3D 9

### Devices and presentation

| Feature | Status |
|---|---|
| `Direct3DCreate9`, one adapter, display modes, `GetDeviceCaps` | Yes. Caps are constants (the same before and after `CreateDevice`) |
| `CreateDevice`, `Reset`, windowed back buffer sized from the window's client area | Yes. One device per process: a second `CreateDevice` resets the first and keeps its window |
| `D3DCREATE_MULTITHREADED`: draws from another thread (a loader drawing a loading screen) | Yes: queued and run on the device's thread (`Portico_D3D9_Service`) |
| `Present`, `Clear`, `BeginScene`/`EndScene`, viewports | Yes |
| Additional swap chains, `GetSwapChain` | No |
| `GetFrontBufferData`, `ProcessVertices`, queries (`CreateQuery`) | No (refused) |
| Device loss | A lost WebGL context is reported once to the page (`Module.onWebGLContextLost`); nothing is restored, and `TestCooperativeLevel` always returns `D3D_OK` |
| Multisampling | No. `CheckDeviceMultiSampleType` refuses modes other than `D3DMULTISAMPLE_NONE` |
| Gamma ramps, hardware cursor, raster status | Gamma ramps are stored/read back but not applied. Cursor methods are stubs; raster status is synthetic |
| Capability/format checks | Partial: several `Check*` methods return success without checking the requested combination; capabilities still need a full consistency audit |

### Resources

| Feature | Status |
|---|---|
| 2D textures with mipmaps, `LockRect` (managed, default, system memory pools) | Yes. A texture keeps its CPU copy, so a later lock sees its contents (`PORTICO_D3D9_DROP_TEXTURE_COPY=1` trades that for memory) |
| Formats: `A8R8G8B8`, `X8R8G8B8`, `A8B8G8R8`, `X8B8G8R8`, `R5G6B5`, `A1R5G5B5`, `X1R5G5B5`, `A4R4G4B4`, `X4R4G4B4`, `DXT1`–`DXT5` | Yes. DXT is decompressed in software where the GPU lacks it |
| Cube and volume textures | No |
| Vertex and index buffers (static and dynamic, `D3DLOCK_DISCARD`/`NOOVERWRITE`), 16 and 32-bit indices | Yes |
| Render target textures, depth-stencil surfaces, `SetRenderTarget`, `GetRenderTargetData` | Yes for the supported combinations, with one render target (slot 0); no multiple render targets |
| `StretchRect`, `UpdateSurface`, `UpdateTexture`, `ColorFill` | Common cases; some surface combinations are not supported (logged) |
| FVF and vertex declarations, up to 16 streams | Yes |
| Instancing through `SetStreamSourceFreq` | No. The setter is a no-op and the getter reports 1 |
| Resource private data, auto-generation filter selection | Private-data setters/free are no-ops and reads return not found. Auto-generation filter changes are ignored; the getter reports linear |
| Palettes, N-patches, rectangular/triangular patches, surface `GetDC`/`ReleaseDC` | Not implemented; calls are refused or stubbed |
| Floating-point, alpha/luminance and depth format variants | Some are listed in the format table; complete upload/lock/conversion behavior is not established here. Floating-point entries are not marked renderable; audit per format and usage before relying on them |

### Fixed-function vertex pipeline

| Feature | Status |
|---|---|
| World, view, projection and texture transforms | Yes |
| Pre-transformed vertices (`XYZRHW`) | Yes |
| Lighting: up to 8 lights, directional, point and spot; materials; `COLORVERTEX` and material sources; specular; local viewer | Yes |
| Vertex blending: 1–3 weights, indexed vertex blending (`D3DRS_INDEXEDVERTEXBLENDENABLE`) | Yes |
| Texture coordinate generation (`D3DTSS_TCI_*`: camera-space position, normal, reflection vector, sphere map) | Yes |
| Texture coordinate transforms (`D3DTSS_TEXTURETRANSFORMFLAGS`, projected) | Yes |
| Fog: vertex and table, linear, exp, exp2, range fog | Yes |
| User clip planes | No (`MaxUserClipPlanes` is 0) |
| Point sprites, point size | No |

### Fixed-function pixel pipeline

| Feature | Status |
|---|---|
| 8 texture stages, color and alpha operations and arguments, `D3DTA_TEMP`, `D3DTA_TFACTOR`, `D3DTA_SPECULAR` | Yes |
| Bump-environment mapping (`D3DTOP_BUMPENVMAP*`) | No (computes `ARG1`) |
| Samplers: filters, mipmaps, anisotropy, wrap/mirror/clamp, border (emulated) | Yes |
| `MIRRORONCE` addressing | Partial: mapped to mirrored repeat, not the exact mirror-once behavior |
| Alpha test, alpha blending (all blend factors and operations), color write mask | Yes |
| Depth test and write, depth bias, stencil | Yes; two-sided stencil (`D3DRS_TWOSIDEDSTENCILMODE`) no, and the caps do not report it |
| Culling | Yes |
| Fill mode and shading mode | Solid fill and Gouraud interpolation are the current path. Other requested fill/shading modes are stored but not applied |
| Scissor test | No (the rectangle is stored, the test is not applied) |
| sRGB texture reads/render-target writes | No. sRGB usage queries are refused; the corresponding states do not implement conversion |

### Shaders and state blocks

| Feature | Status |
|---|---|
| Vertex and pixel shaders (`CreateVertexShader`, `CreatePixelShader`) | Refused. The caps report no shader model, so programs with a fixed-function path choose it; programs that need shaders do not run yet |
| State blocks | No |
| Shader constants | Setters are stubs and getters fail; there is no programmable shader execution |

## D3DX 9

| Feature | Status |
|---|---|
| Vectors (2, 3, 4), matrices, quaternions, planes, colors: the types with their operators | Yes |
| Matrix functions: multiply, transpose, inverse, determinant, scaling, translation, rotations (axis, X/Y/Z, quaternion, yaw-pitch-roll), look-at, perspective and orthographic (LH and RH, off-center), reflect, shadow | Yes |
| Vector functions: normalize, transform (coord, normal), project, unproject | Yes |
| Quaternion functions: normalize, inverse, multiply, from axis/matrix/yaw-pitch-roll, slerp, to axis-angle | Yes |
| Plane and color functions | Yes |
| `D3DXCreateTexture`, `D3DXCreateTextureFromFile(Ex)` (A and W), `...FromFileInMemory(Ex)`, `D3DXGetImageInfoFromFile(InMemory)` | Common cases: DDS (DXT1–5 and uncompressed), TGA, BMP; non-interlaced PNG with zlib. Requested `Filter`/`MipFilter` values are ignored; generated mip levels use a box filter |
| `D3DXLoadSurfaceFromFileInMemory`, `D3DXLoadSurfaceFromSurface` | Common cases. Filter arguments are ignored and encoding decoded pixels into DXT destinations is not implemented |
| Meshes, effects, fonts, sprites, shader compilation | No |
| Lines, render-to-surface/environment-map helpers, animation controllers | No |
| `D3DXSaveSurfaceToFile*`, `ID3DXBuffer` | Save functions return `E_NOTIMPL`; the buffer interface is only forward-declared |

## Win32 (browser builds)

| Feature | Status |
|---|---|
| Window classes and windows (A and W): the first window's client area is the page's canvas, with the client size Windows 10/11 would give | Yes |
| `PeekMessage`, `GetMessage` (gives the thread back to the browser while waiting), `DispatchMessage`, `PostMessage`, `SendMessage`, `DefWindowProc`, `WM_PAINT` as on Windows | Yes |
| Mouse, wheel and keyboard messages from the canvas | Yes |
| `timeGetTime`, `GetTickCount`, `QueryPerformanceCounter`, `Sleep`, `MessageBox` (to the console), `GetModuleHandle`, `OutputDebugString` | Yes |
| `wWinMain`/`WinMain` as the entry point (`portico::win32_main`) | Yes |
| Code-page conversions | Partial: the current conversion functions treat input as UTF-8, rather than implementing arbitrary Windows code pages/flags. Emscripten `wchar_t` is 32-bit |
| Additional game-facing Win32 APIs, MFC subset, DirectSound, Winsock | [Extraction planned](../ROADMAP.md#what-can-be-extracted); this is not a commitment to implement all of Win32 |
| DirectInput, XInput, legacy graphics/audio/network/media frameworks | No dedicated public modules. See the [ecosystem inventory](ecosystem-coverage.md) for investigation, dependencies and exclusions |
