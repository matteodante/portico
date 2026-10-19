# Compatibility reference audit

Reviewed on **2026-10-19** against the roadmap, ecosystem inventory and support summary at
[`bb7ec03`](https://github.com/matteodante/portico/tree/bb7ec03cb8ddf0b1472fe377167af476f3862f1e), with an independent
second review of the accessory ecosystem. The corrections from this review are incorporated into the current
[roadmap](../ROADMAP.md) and [inventory](ecosystem-coverage.md).

These sources describe API contracts and platform constraints. They do not prove that Portico implements them.
The review checks component coverage and selected semantics; M0 still needs a per-API matrix and reference-program
fixtures. Archived Microsoft documentation is intentional for legacy APIs, and framework/API versions must be
recorded rather than replaced silently with the latest Windows behavior.

## Acceptance details for the next milestones

- **M0 — Capability and error audit (G7, Q1):** compare advertised caps and accepted format/usage combinations with
  working paths. Record exact return values and output/state changes. Separate an unimplemented success-returning
  stub from a documented no-op: D3D9 can discard draws successfully while a device is lost. A refused call also needs
  its exact HRESULT checked; failure by itself does not establish native equivalence.
- **M1 — Scissor (G1):** test enable/disable, coordinate conversion, viewport interaction, draws and `Clear`.
  `SetRenderTarget` resets both viewport and scissor rectangle to the full target. Test state-block recording and
  restoration of the scissor rectangle, including `D3DSBT_ALL`, and invalid rectangles/calls against native D3D9.
- **M1 — State blocks (G1):** `CreateStateBlock` captures the selected state immediately. Test the actual membership
  of `D3DSBT_ALL`, `D3DSBT_PIXELSTATE` and `D3DSBT_VERTEXSTATE`, custom `BeginStateBlock`/`EndStateBlock` recording,
  later `Capture`, and `Apply` leaving unrecorded states unchanged. Cover resource references and invalid sequences.
  Do not interpret "all" as every device property without checking the documented state list.
- **M2 — Worker and storage (W1, A1, Q2):** JSPI suspension does not create threads. Emscripten requires separate
  pthread and non-threaded builds if both profiles are offered. Test thread-pool startup and main-thread proxy
  deadlocks. Validate a secure, cross-origin-isolated context with COOP `same-origin`, COEP `require-corp` or
  `credentialless`, and a permitting `cross-origin-isolated` Permissions-Policy, including asset/embed behavior.
  Check `crossOriginIsolated`, SharedArrayBuffer and worker WebGL2 at runtime. Synchronous OPFS access is restricted
  to dedicated workers; `createSyncAccessHandle()` itself returns a promise. Test quota, interruption and reload.
- **M2/M4 — Loss and reset (G4, G6):** define the mapping from WebGL context loss to D3D9
  `Present`/`TestCooperativeLevel`/`Reset`. D3D9 requires default-pool resources and additional swap chains to be
  released before reset; managed/system-memory resources do not require that release, and shaders survive reset.
  Test the resource-specific behavior rather than recreating or discarding everything indiscriminately.
  D3D9Ex uses a different loss/status contract and remains a separate scope decision.
- **M3 — Audio and input (S1, I1):** test DirectSound primary/secondary buffers, duplication, capabilities, cursor
  and status queries, `DSERR_BUFFERLOST`, `Restore` and refilling lost data. Browser audio resumption is a separate
  event. Select the XInput version explicitly, test trigger mapping and duplicate DirectInput/XInput enumeration,
  and account for WMI/COM dependencies in application-side controller detection.
- **M4 — Graphics dependencies (G3, G5):** native indexed stream-frequency instancing requires a device supporting
  vertex shader model 3.0 and a programmable vertex path; it is not a fixed-function feature. Keep shader work ahead
  of its compatibility claim. Test MRT caps, slot-zero restrictions, viewport/scissor resets, and render-target/depth
  format, dimensions and multisample compatibility.
- **M5 — Transport (N1, N2):** WebSocket exposes reliable, ordered messages. A TCP adapter must bridge message
  framing, buffering and partial reads/writes, with explicit receive flow control because the standard WebSocket
  API has no automatic backpressure. UDP loss/ordering, discovery and DirectPlay services need their own protocol
  decisions; naming a WebSocket or WebRTC transport does not establish equivalence.

## Official sources by inventory area

All links below were opened during the review. The source index complements the code links in the inventory;
when closing an item, attach a method-specific reference and test evidence as well.

| Areas | Reference | What it constrains |
|---|---|---|
| G1–G8 | [D3D9 reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9-graphics-reference), [device interface](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nn-d3d9-idirect3ddevice9), [resource interface](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nn-d3d9-idirect3dresource9) | Starting inventories for methods, caps, resource ownership and error contracts. |
| G1 | [Scissor test](https://learn.microsoft.com/en-us/windows/win32/direct3d9/scissor-test), [state blocks](https://learn.microsoft.com/en-us/windows/win32/direct3d9/state-blocks-save-and-restore-state), [CreateStateBlock](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-createstateblock) | Clear/clipping, target changes, capture at creation, predefined and custom state sets. |
| G3, G5 | [Geometry instancing](https://learn.microsoft.com/en-us/windows/win32/direct3d9/efficiently-drawing-multiple-instances-of-geometry), [SetRenderTarget](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setrendertarget) | Shader prerequisites, target restrictions, caps and depth/multisample compatibility. |
| G4, G6, G8 | [Lost devices](https://learn.microsoft.com/en-us/windows/win32/direct3d9/lost-devices) | D3D9 loss, successful discarded work, reset/resource lifetimes and D3D9Ex differences. |
| X1–X6 | [D3DX reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9-graphics-reference-d3dx) | Deprecated utility library covering effects, geometry/meshes, math, shaders and textures; the current public subset is smaller. |
| W1, Q2 | [Emscripten pthreads](https://emscripten.org/docs/porting/pthreads.html), [Asyncify and JSPI](https://emscripten.org/docs/porting/asyncify.html) | Separate builds, thread startup, proxy deadlocks and asynchronous suspension. |
| W1, Q2 | [crossOriginIsolated](https://developer.mozilla.org/en-US/docs/Web/API/Window/crossOriginIsolated), [OffscreenCanvas](https://developer.mozilla.org/en-US/docs/Web/API/OffscreenCanvas) | Isolation/security/embedding requirements; individual worker rendering features still need testing. |
| Q2 | [WebAssembly.promising](https://developer.mozilla.org/en-US/docs/WebAssembly/Reference/JavaScript_interface/promising_static), [MDN JSPI compatibility data](https://github.com/mdn/browser-compat-data/blob/main/webassembly/jspi.json) | At review time: Chrome/Edge 137, Firefox 153, Safari 27. These are JSPI availability thresholds, not Portico qualification results; Safari remains untested. |
| W5 | [COM initialization](https://learn.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library), [threads and apartments](https://learn.microsoft.com/en-us/windows/win32/com/processes--threads--and-apartments) | Per-thread initialization, apartment model, message pumping and marshaling; COM-style interfaces alone are insufficient. |
| I1 | [XInput versions](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-versions), [XInput and DirectInput](https://learn.microsoft.com/en-us/windows/win32/xinput/xinput-and-directinput) | Versioned features, controller enumeration, trigger differences and WMI-based duplicate detection. |
| S1 | [DirectSound buffer basics](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416967(v=vs.85)), [buffer management](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416241(v=vs.85)) | Primary/secondary buffers, capabilities/status/format, duplication and loss/restore/refill behavior. |
| A1 | [Origin private file system](https://developer.mozilla.org/en-US/docs/Web/API/File_System_API/Origin_private_file_system), [createSyncAccessHandle](https://developer.mozilla.org/en-US/docs/Web/API/FileSystemFileHandle/createSyncAccessHandle) | Origin storage, quotas, dedicated-worker-only synchronous access and asynchronous handle acquisition. |
| N1, N2 | [WHATWG WebSockets](https://websockets.spec.whatwg.org/), [MDN WebSocket](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket) | Message-oriented API over a connection, send buffering and lack of automatic backpressure. |
| N2, L3 | [DirectPlay 8 Core and Service Providers](https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-dpl8cs/61dd01ff-8cf8-423d-8c4a-31047ab6362b) | Session services over datagram protocols such as UDP; a TCP bridge alone does not implement them. |
| S1, L2, L4 | [DirectX Media Objects](https://learn.microsoft.com/en-us/windows/win32/directshow/directx-media-objects) | Media components used directly or by DirectShow and DirectSound effects. |
| L2 | [Audio Compression Manager](https://learn.microsoft.com/en-us/windows/win32/multimedia/about-the-audio-compression-manager), [multimedia file I/O](https://learn.microsoft.com/en-us/windows/win32/multimedia/multimedia-file-i-o), [mmioOpen](https://learn.microsoft.com/en-us/windows/win32/api/mmiscapi/nf-mmiscapi-mmioopena) | Audio conversion/codecs and RIFF/mmio operations with their own handles; ordinary Win32 file support does not supply them. |
| L2 | [XAudio2 versions](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-versions), [WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi) | DirectX SDK XAudio2 2.0–2.7 versus later 2.8/2.9, activation/enumeration/codec differences, optional later audio APIs. |

## Keeping the review current

Use the version required by the reference program, and verify the linked page again when implementing an item.
Add newly discovered dependencies to the [inventory](ecosystem-coverage.md) even when deferred. A new source or a
successful build cannot close an API gap without behavioral evidence. Browser compatibility data can change;
publish actual tested browser/GPU versions separately in [testing.md](testing.md).
