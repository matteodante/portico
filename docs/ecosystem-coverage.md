# DirectX 9 era ecosystem coverage plan

This is the inventory behind the [roadmap](../ROADMAP.md), so dependencies do not disappear simply because they
are outside the renderer. It describes future work, not additional implemented features.

The initial audit was made on 2026-10-19 against public commit
[`bcda7a8`](https://github.com/matteodante/portico/tree/bcda7a88e1b971398f808afbffb0d5db099ab7c4).
It inspected the module interfaces, implementations, existing documentation and test setup. It is a component-level
inventory, not an exhaustive conformance test of every Windows or DirectX function. M0 still needs the per-API matrix
and reference-program dependency lists; unknown behavior remains open work.

## Scope and status

The primary target is C++ source using Direct3D 9/D3DX 9 and the Windows services needed by games and tools of that
era. The application and its dependencies must be buildable for WebAssembly. Win32, MFC and COM support are bounded
subsets driven by those programs, not promises to reproduce the whole desktop operating system.

Status terms used below:

- **Partial:** implementation exists publicly, but the listed semantics or cases remain missing or unverified.
- **Missing:** no working public implementation was found for the listed feature; a declaration or success-returning
  stub does not count as support.
- **Extraction:** reported working in the original private port; publication and independent tests remain required.
- **Audit/decision:** coverage, feasibility or demand needs investigation; no implementation commitment is implied.
- **Browser constraint:** exact desktop behavior cannot be assumed; record the adapter, changed semantics or refusal.

The baseline is WebGL2 and JSPI; worker builds additionally need pthreads, OffscreenCanvas and cross-origin isolation.
Native Windows/ANGLE is the comparison/debugging path. Browser/version and GPU differences must be measured.

## Priority map

The executable sequence is in [the roadmap](../ROADMAP.md#priorities-and-next-steps). Priority is separate from
implementation status: an extraction can still be blocked, and an optional dependency can become essential for a
particular program.

| Priority | Inventory areas | Rule for scheduling |
|---|---|---|
| **P0** | G7 and Q1 baseline; scissor/state blocks from G1 | Start with a reference program, accurate support reporting and these two bounded rendering tasks. |
| **P1** | W1–W5, A1, X1, I1, S1; required recovery/lifecycle work from G6 | Build a usable application: worker, files, saves, UI, controls and sound. Scope each extracted API to an independent test. |
| **P2** | Remaining G1/G6 work; G2–G5, G8, X2–X6, N1–N2 | Broaden graphical/content/network compatibility. Networking moves to P1 for a multiplayer reference program. |
| **P3 / per-program decision** | L1–L6 | Keep visible; implement or adapt when a selected program needs them. Required dependencies move up and block compatibility until resolved. |
| **All priorities** | Q1–Q3 | Apply diagnostics, native comparisons, browser/lifecycle testing, performance and packaging checks to every delivered feature. |

No row is closed by being assigned P3. An explicit decision and evidence are still needed, and exclusions must remain
visible in the supported-program profile.

## Direct3D 9

| ID | Area and work to account for | Current evidence / disposition |
|---|---|---|
| G1 | Fixed-function state: scissor; state block create/record/capture/apply; user clip planes and clip status; point sprites/point size/scaling; two-sided stencil; bump-environment operations. Include state restoration and interaction with shaders. | Missing. Some setters store values or return success without applying the feature. See [device methods](../modules/d3d9/src/device.h), [implementation](../modules/d3d9/src/device.cpp) and [FFP](../modules/d3d9/src/ffp.cpp). |
| G2 | Rasterization/sampling: wireframe and point fill, flat shading, dithering, edge/line rules, `MIRRORONCE`, border behavior, mip/LOD controls, vertex declaration/FVF edge cases, precision, half-pixel/depth conventions and existing lighting/fog/texture-stage combinations. | Partial/audit. Culling is implemented; `FILLMODE`/`SHADEMODE` changes are not consumed by rendering. `MIRRORONCE` maps to mirrored repeat. Broader state combinations need fixtures in [device_draw.cpp](../modules/d3d9/src/device_draw.cpp). |
| G3 | Vertex/pixel Shader Models 1.x, 2.x and 3.0: bytecode validation/translation, declarations, registers/constants, swizzles/modifiers, control flow, texture operations, precision, linking and program caching. Include vertex texture fetch and mixed fixed/programmed stages. | Missing: shader creation is refused and shader versions report zero. Shader constant setters are stubs. HLSL compilation is a separate dependency (X3). |
| G4 | Resource completeness: cube/volume textures; all required formats and usage combinations; compressed/subrect locks, pitch, pools, dirty regions, mip generation/filter selection, readback/copies and `Update*`/`StretchRect` cases. Include `IUnknown`, ownership, private data, priority/eviction and shared-handle behavior. | Partial. Cube/volume textures are refused; private-data methods and auto-generation filter setters are stubs. The [format table](../modules/d3d9/src/formats.cpp) is not proof that every upload, lock, conversion or render-target use works. Audit [resources](../modules/d3d9/src/resources.h) and their [implementation](../modules/d3d9/src/resources.cpp). |
| G5 | Advanced drawing/output: stream-frequency instancing, multiple render targets and per-target masks, floating-point/HDR targets and blending, sRGB reads/writes, gamma handling, MSAA quality/masks/resolve, queries and `ProcessVertices`. | Missing/partial. `SetStreamSourceFreq` is a no-op; only render target 0 is supported; format checks refuse sRGB/vertex-texture requests and multisampling. Gamma ramps are stored but not applied. Queries and `ProcessVertices` are refused. Floating-point formats exist in the table but are not renderable there. |
| G6 | Device/presentation: context loss and restoration, reset/resource-pool behavior, additional swap chains, independent devices/windows, present rectangles and intervals, resizing/DPI, fullscreen transitions, back/front-buffer access, cursors and raster-status behavior. | Partial. One device is reused, swap-chain retrieval is refused, loss is reported without restoration, and `TestCooperativeLevel` always succeeds. Hardware cursor methods are stubs; raster status is synthetic. Fullscreen, display modes and refresh control have browser constraints. |
| G7 | Capability/error contract: audit every advertised cap, adapter/format/depth combination, valid/invalid argument, output pointer, HRESULT, resource lifetime and threading rule. Make unsupported features discoverable and diagnostics consistent. | Partial/audit. Several `Check*` methods unconditionally succeed; some caps expose operations with missing behavior. Not all stubs log. Compare [IDirect3D9](../modules/d3d9/src/d3d9_impl.h), [caps](../modules/d3d9/src/d3d9.cpp), device and resource interfaces with observed runtime behavior. |
| G8 | D3D9 long tail: palettes, N-patches, rectangular/triangular patches, software vertex processing/`RegisterSoftwareDevice`, GDI surface `GetDC`/`ReleaseDC`, and D3D9Ex-specific interfaces/behavior. | Missing or stubs in the public interfaces; D3D9Ex needs a separate scope decision. Keep these visible even if deferred. No full-D3D9 conformance claim is valid while in-scope entries remain open. |

## D3DX 9 and content

| ID | Area and work to account for | Current evidence / disposition |
|---|---|---|
| X1 | `ID3DXSprite`, `ID3DXFont`, `ID3DXLine`: batching, transforms, clipping, blending, font metrics/layout and lost/reset device handling. Font fallback, non-Latin text and font distribution also need decisions. | Missing. Depends on graphics state restoration and the chosen text/font implementation. |
| X2 | Meshes: `.x` reading/writing, mesh buffers, adjacency, normals/tangents, optimization, progressive/patch meshes, skin information, hierarchy loading and animation controllers. Inventory UV atlas, spherical harmonics and precomputed radiance transfer helpers if a program uses them. | Mesh objects/loaders/animation are missing. A few geometric math helpers such as `D3DXIntersectTri` already exist; they do not provide mesh support. See [mesh header](../modules/d3dx/include/d3dx9mesh.h). Specialized tooling remains an audit/decision. |
| X3 | Shader tools: assembly/disassembly, HLSL compilation, includes/macros/flags, diagnostics, buffers and constant-table/reflection APIs. Inventory D3DX and standalone D3DCompiler imports/versions; decide build-time compilation versus runtime compilation for each program. | `D3DXAssembleShader` returns `E_NOTIMPL`; [ID3DXBuffer](../modules/d3dx/include/d3dx9core.h) is only declared. A Windows compiler DLL cannot simply be loaded into Wasm; bytecode execution and source compilation need separate work. |
| X4 | Effects: `.fx` loading/compilation, techniques/passes, parameter handles, state managers, effect pools, save/restore behavior; render-to-surface and render-to-environment-map helpers. | Missing. Depends on shader support and state/resource semantics. |
| X5 | Texture/image completeness: remaining image formats and PNG variants, cube/volume loaders, resource-based loading, requested resize/mipmap filters, palette/color-key handling, compressed conversion/encoding, procedural fills, normal-map helpers and image export. | Partial. DDS/TGA/BMP/PNG loaders exist; interlaced PNG is refused, `Filter`/`MipFilter` parameters are ignored by texture creation, writing decoded pixels to DXT is refused and `D3DXSaveSurfaceToFile*` is unimplemented. See [loaders](../modules/d3dx/src/d3dx9tex.cpp). |
| X6 | Remaining D3DX math/helper functions, overloads and SDK-version expectations, including degenerate inputs and return values. | Partial/audit. Current math tests cover useful properties and selected Windows outputs, not the whole D3DX API. Enumerate needed symbols before calling this module complete. |

## Windows runtime, input and storage

| ID | Area and work to account for | Current evidence / disposition |
|---|---|---|
| W1 | Worker execution and synchronization: thread creation/join, TLS, critical sections, events, mutexes, semaphores, single/multiple waits, timeouts, atomics, message dispatch between threads and servicing queued graphics during waits. | Extraction beyond the minimal public Win32 module. Test blocking/loading loops and deadlocks; retain the JSPI/main-thread path where appropriate. |
| W2 | Files and CRT integration: handles, seek/enumeration, mappings, async/overlapped operations where used, sharing/locking, working directories, path normalization/case, file timestamps, registry/settings and persistent saves. | Extraction plus audit of exact semantics. Current D3DX path normalization and Emscripten preloaded files are not full Win32 file support. |
| W3 | Windows/UI services: resources from `.rc`, timers, window/message lifecycle, focus/activation, sizing/DPI/fullscreen, dialogs/menus/controls, GDI/text calls and clipboard used by reference programs. | Minimal window/message/time support is public; other pieces require extraction or explicit decisions. `MessageBox` currently writes to the console. Desktop dialogs and arbitrary windows do not automatically map to canvas. |
| W4 | Strings and binary compatibility: ANSI code pages, Unicode conversion/error flags, UTF-16/surrogates, IME/composition, locale, structure packing, integer/pointer widths and serialized/network layouts. | Partial/extraction. Public conversions treat input as UTF-8 rather than implementing arbitrary code pages; Emscripten `wchar_t` is 32-bit, Windows `wchar_t` is 16-bit. Audit formats rather than assuming binary identity. |
| W5 | MFC/ATL/COM dependencies: publish the reported `CString`/`CFile`/`CArchive`, runtime classes, application/message-map subset; inventory collections, dialogs, serialization versions, COM activation/interfaces and any ATL/OLE use separately. | MFC extraction; the rest is audit/decision. Existing COM-style D3D objects do not imply a general COM runtime, ActiveX or full MFC/ATL support. |
| I1 | Input: Win32 key state/mouse capture, relative mouse/Pointer Lock, focus-loss releases, keyboard layouts/IME, DirectInput 8 acquisition/data formats/buffered events (and older interfaces if used), XInput controllers, WinMM joystick/Raw Input dependencies, hotplug, dead zones and vibration/force feedback. | Only basic mouse/wheel/key messages from the canvas are public. DirectInput/XInput are additional planned investigation, not previously extracted modules. Browser support for raw device access, exclusive mode and force feedback varies or has no equivalent. |
| A1 | Asset delivery and persistence: OPFS installer, manifest/hash validation, resumable downloads, atomic version changes, cancellation/progress, corruption/quota/eviction handling, save migration, export/import and network/offline behavior. | Installer extraction plus lifecycle audit. Browser storage is origin-scoped and can be cleared; document persistence guarantees. App archive/compression/encryption formats need buildable dependencies and fixture files. |

## Audio, network and media

| ID | Area and work to account for | Current evidence / disposition |
|---|---|---|
| S1 | DirectSound 8: device/cooperative-level behavior, static/streaming buffers, lock/play/write cursors, looping, notifications, mixing/resampling, volume/pan/frequency, 3D listener/sources and deferred changes. Inventory effects, capture/full-duplex and voice paths separately. | 2D/3D audio is reported working privately and awaits extraction. That report does not establish all streaming, effects or capture semantics. Browser autoplay/user activation, suspend/resume, latency and microphone permission need explicit behavior and tests. |
| N1 | Winsock/TCP: socket lifecycle, blocking/nonblocking operations, partial reads/writes, DNS/address handling, errors, `select`/`WSAAsyncSelect` ordering, reconnect/timeouts/backpressure and gateway deployment/configuration. Inventory event/overlapped variants if used. | Extraction of the reported WebSocket↔TCP path. Browsers do not open arbitrary native TCP sockets; document the gateway and its allowlist. Validate against a controlled server and the program's actual protocol. |
| N2 | Datagram and discovery dependencies: UDP message boundaries, ordering/loss, broadcast/multicast, LAN discovery, peer-to-peer/NAT behavior, address families and transport security requirements. | Audit/decision and browser constraints. WebSocket is an ordered reliable stream; it is not a transparent UDP implementation. Any relay/WebRTC adaptation requires a protocol-specific design and tests. |

These neighboring technologies are part of the dependency inventory for games of the era. They are **not** all
core DirectX 9 APIs, and listing them does not commit Portico to implementing each framework in full.

| ID | Dependency family to check | Current disposition |
|---|---|---|
| L1 | Older graphics: DirectDraw, Direct3D 7/8, D3DX 8, and mixed legacy 2D/3D launchers or renderers. | No dedicated public module. Decide whether a source-level adapter to D3D9 is justified by a reference program. |
| L2 | Other audio: WinMM `waveOut`/MIDI/MCI, DirectMusic, XAudio2/X3DAudio/XAPO, XACT banks and optional vendor extensions such as EAX. | No public compatibility modules for these families. Check the exact version, assets, licensing and browser audio path; DirectSound support does not cover them. |
| L3 | DirectPlay/DirectPlay Voice, lobby/session/discovery services and game-specific online SDKs. | No public module. Treat as protocol/service dependencies, distinct from Winsock; decide adaptation, server requirements or explicit exclusion. |
| L4 | Video/cutscenes: DirectShow, Video for Windows/AVI, MCI video, codecs and later Media Foundation dependencies. Include video-to-texture, seeking, subtitles, audio/video sync and bundled middleware. | No public media compatibility module. Choose browser decoding or a portable decoder and document unsupported codecs/APIs and distribution rights. |
| L5 | SDK/application frameworks: DXUT, D3D9Ex consumers, later Direct2D/DirectWrite overlays, WIC/GDI+ image loading, SDK helper libraries, engine plugins, physics/audio/video middleware, launchers/patchers and DirectSetup/redistributable or versioned-DLL assumptions. | Audit each program's actual headers, symbols, versions and resources. Portico's Windows entry point does not implement these frameworks or run a Windows installer. Prefer compatible portable components where appropriate. |
| L6 | Dependencies that require a different execution model: binary-only DLLs/plugins, x86 inline assembly, MSVC-specific SEH/intrinsics, managed/XNA code, OS processes/services, kernel drivers, desktop DRM and anti-cheat. | Require source changes/rebuilds or explicit exclusion. CPU/PE emulation, general .NET and kernel/desktop integration are outside the current source-recompilation scope. Direct3D 10/11/12 are also outside this roadmap. |

## Browser contract and qualification

| ID | Work to account for | Acceptance requirement |
|---|---|---|
| Q1 | API and application compatibility matrix, diagnostics and negative tests. | Track exact API/version/arguments, public behavior, expected native behavior, fallback/refusal, a reference fixture and unresolved gaps. Classify no-op success separately from implemented behavior. Track program dependencies, not just linked Portico modules. |
| Q2 | Browser/GPU/lifecycle coverage: Chromium/Edge, Firefox and Safari where supported; actual hardware plus software rendering; JSPI, WebGL2/extensions, OffscreenCanvas/pthreads; resize/DPI, context loss, background tabs, focus, audio interruption and memory pressure. | Publish tested versions/devices and feature gates, including untested platforms. Exercise COOP/COEP/CORP/CORS for worker builds and affected assets. Do not claim parity based only on the existing tutorial screenshots. |
| Q3 | Performance, packaging and maintainability: startup/download size, frame time, CPU/GPU work, Wasm memory/stack, resource leaks, loading stalls, caching, SDK/toolchain pinning, source maps/logs, server headers/MIME types, licenses and source availability. | Record per-program budgets and measurements, test clean builds and packaged assets, and document release/reproduction instructions. Distinguish live-site health from verification of a particular source build. |

Fullscreen, Pointer Lock, clipboard and audio may require user activation or permission. Browsers can throttle hidden
tabs, constrain memory, lose GPU contexts and evict storage. Network access and device access follow browser rules.
For each affected Windows call, document the actual browser behavior and recovery path. These are compatibility
requirements, not details that can be left to a game's installer.

## How an item is closed

1. Identify a reference program or a focused fixture and the exact API/version and inputs. Audit all its required
   symbols and middleware; add newly discovered families to this inventory.
2. Record the implementation location, dependencies and intended semantics. If unavailable, record an explicit
   limitation, adapter design or deferral with a reason and affected programs.
3. Add relevant comparisons with the Windows implementation: rendering, return values, state, file/packet bytes,
   audio/input behavior or lifecycle. Use licensed fixtures; private-port results alone are insufficient.
4. Verify integration, invalid inputs, failure/recovery cases and the claimed browser targets. Record evidence and
   any performance limits; update [supported-api.md](supported-api.md) and [testing.md](testing.md).

An inventory can be fully accounted for while implementation is incomplete. A declared compatibility profile is
complete only when every required item in that profile works and is tested. Deferred optional items remain visible;
they must not be used to describe the full DirectX/Windows ecosystem as implemented.
