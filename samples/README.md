# Samples

Microsoft's Direct3D 9 tutorials 1–5 from the [DirectX SDK samples](https://github.com/microsoft/DirectX-SDK-Samples)
(`C++/Direct3D/Tutorials`), under the MIT license ([LICENSE-Microsoft-DirectX-SDK-Samples.txt](LICENSE-Microsoft-DirectX-SDK-Samples.txt)).
The sources and `banana.bmp` are byte for byte Microsoft's; Portico builds them unchanged.

| Sample | What it shows |
|---|---|
| `Tut01_CreateDevice` | A device, `Clear` and `Present` in `WM_PAINT`, with `GetMessage` |
| `Tut02_Vertices` | A vertex buffer of pre-transformed (`XYZRHW`) colored vertices |
| `Tut03_Matrices` | World, view and projection transforms (D3DX math), animated with `timeGetTime` |
| `Tut04_Lights` | A material, a directional light, a cylinder |
| `Tut05_Textures` | `D3DXCreateTextureFromFile`, texture stages |

Build options (see `CMakeLists.txt`): `PORTICO_SAMPLES_FIXED_TIME=<ms>` freezes `timeGetTime()` through a forced
include (`fixed_time.h`) so the screenshot tests see the same frame; `PORTICO_SAMPLES_SYSTEM_D3D9=ON` builds them
against the system's Direct3D 9 on Windows, for the reference renders. `shell.html` is the page of the browser builds.
