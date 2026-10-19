# Roadmap

The goal is to recompile existing C++ Windows games and tools from the DirectX 9 era for the browser, preserving
their application code wherever the platform layer can supply equivalent behavior. Completing the renderer alone
does not complete that ecosystem: input, audio, files, threads, networking, text and application dependencies matter too.

**Current support:** [supported API](docs/supported-api.md).
**Work still to account for:** [ecosystem coverage plan](docs/ecosystem-coverage.md).
**API contracts and online audit:** [official compatibility references](docs/compatibility-references.md).
The coverage plan includes gaps found in the public implementation, extraction work, dependencies to evaluate and
browser limits. It is an inventory, not a claim that every item is implemented or a commitment to emulate all Windows APIs.

## Priorities and next steps

The default order aims for a small, usable fixed-function game before broad shader-era compatibility. A reference
game has not yet been selected. These are proposed milestones, without release dates or completion claims. Area IDs
refer to the [coverage plan](docs/ecosystem-coverage.md); a program that requires a lower-priority item promotes it
to a blocker, rather than silently accepting missing behavior.

| Priority | Work | Why this comes next |
|---|---|---|
| **P0 — Start now** | M0: baseline, API inventory and truthful capability/error reporting. M1: scissor test and state blocks. | Establish what works, remove misleading success/capability claims and supply the state handling needed by UI helpers. |
| **P1 — Make a complete small application usable** | M2: worker, synchronization, files/saves/resources/text and required MFC subset. M3: sprites/fonts, input and DirectSound. Include device recovery needed by that application. | Loading, UI, controls, sound and persistence must work together, beyond the rendering tutorials. |
| **P2 — Broaden compatibility** | M4: remaining fixed-function/resource behavior, shaders, effects, advanced rendering and content helpers. M5: networking and transport decisions. | Expand to more games once the foundations work. Move networking into P1 if the first reference game is multiplayer. |
| **P3 — Resolve application-specific extensions** | M6: older DirectX families, alternative audio/video frameworks, SDK helpers and middleware. | Retain every dependency in the inventory, but implement it when a real program establishes the need. Required extensions become P1/P2 blockers. |

Compatibility tests, browser limits, performance and distribution checks (Q1–Q3) apply at **every** priority;
they are not postponed until P3. An unimplemented success-returning stub in a required path is a blocker regardless
of its original priority. Distinguish it from a documented no-op, such as discarded draws while a D3D9 device is lost.

| Order | Work and dependencies | Completion evidence |
|---|---|---|
| **M0 · P0 — Establish the baseline** | Select a small source-available fixed-function reference program; inventory its APIs, dependency versions and assets. Audit stubs/caps/errors (G7, Q1), then expand the per-API matrix across all inventory areas. | Reproducible native/browser results and a matrix distinguishing implemented, partial, ignored, refused and unknown behavior. Existing tutorials remain regression tests, not the whole compatibility claim. |
| **M1 · P0 — Scissor and state blocks** | Implement scissor first, then state-block create/record/capture/apply, including object ownership and restoration (G1). | Native comparisons cover scissored draws and `Clear`, render-target changes, capture at creation, custom recording, resource references and invalid calls. Use the [contract checklist](docs/compatibility-references.md#acceptance-details-for-the-next-milestones); correct affected capabilities and support documentation. |
| **M2 · P1 — Runtime and storage** | Extract worker and synchronization first; then Win32 files/resources/text, persistent saves/OPFS and the MFC subset actually required (W1–W5, A1). Include the reference application's reset/context-loss behavior (G6). Plan separate threaded and non-threaded builds if both are offered. | Loading does not freeze the page, waits/proxied calls do not deadlock, input reaches the worker, saves survive reload and interrupted asset updates recover. Test feature gates and worker-only synchronous OPFS access. |
| **M3 · P1 — UI, controls and sound** | Build D3DX sprite/font helpers on state blocks; add line helpers where needed. Extract DirectSound, then complete required mouse capture, relative mouse and controller behavior (X1, I1, S1). | The reference program has working UI/text, focus handling, input and sound. Test buffer loss/restore, streaming/3D audio, controller identity and reconnection when claimed; audio capture is a separate requirement. |
| **M4 · P2 — Graphics and content coverage** | Close remaining G1/G2/G4/G6 gaps; implement Shader Model bytecode and constants before effects/runtime compiler integration (G3, X3, X4). Add MRT, instancing, MSAA, sRGB/HDR, queries and required mesh/animation/image helpers (G5, X2, X5, X6); retain G8 decisions. | Each feature has a native comparison and correct caps/error behavior. A second, shader-dependent reference program renders correctly; required formats, shaders and assets have fixtures. |
| **M5 · P2, or P1 for multiplayer — Networking** | Extract Winsock/TCP gateway after runtime foundations; decide UDP/discovery/DirectPlay requirements before claiming multiplayer compatibility (N1, N2, L3). | A controlled server validates partial transfers, readiness, timeouts and reconnect. The documented transport preserves the required protocol semantics. |
| **M6 · P3, promoted when required — Optional dependencies** | Resolve legacy graphics, alternative audio, video, frameworks and middleware (L1–L6); confirm decisions for the D3D9 long tail (G8). | Every required dependency is implemented/tested or explicitly blocks that program. Deferred/excluded optional dependencies retain a reason and affected-program list. |

The first implementation tasks should therefore be: **select the reference program and baseline; audit the caps and
stubs it exercises; implement scissor; implement state blocks; extract the worker/synchronization foundation**.
The detailed inventory remains open while those tasks are completed.

Not everything maps exactly to WebGL2 or browser APIs. For each gap, decide whether to implement it, emulate it,
provide a documented adapter or refuse it accurately. Evaluate WebGPU only for concrete backend gaps; it does not
automatically solve Windows compatibility or imply Direct3D 10/11/12 support.

## What can be extracted

Portico originated in a private port of a 2000s Windows MMO client running in the browser against a private local
server instance. The following components were reported as working in that port. Their public extraction still needs
review, standalone packaging and tests on programs other than that game; this does not establish full API coverage.

| Component | Reported implementation in the original port | Public status |
|---|---|---|
| Worker setup | A pthread with an `OffscreenCanvas` and forwarded page input, for programs that block | To extract |
| Game-facing Win32 subset | Threads and synchronization (fair critical sections), files, persistent registry, code pages, IME, timers, `.rc` resources | To extract beyond the current minimal subset |
| MFC subset (`afx`) | `CString`, `CFile`, `CArchive` (byte-compatible with MFC), runtime classes, `CWinApp`, message maps | To extract |
| DirectSound 8 | 2D and 3D sound on miniaudio and Web Audio; the 3D model was measured against DirectSound | To extract; audit streaming, notifications, effects and capture separately |
| Winsock over WebSocket | `select`, `WSAAsyncSelect`, WebSocket↔TCP gateway with an allowlist | To extract; this does not supply native UDP sockets |
| Asset installer | OPFS storage, SHA-256 manifest, resumable downloads, atomic updates | To extract |

The original port's [design and measurements](https://github.com/matteodante/matteodante/blob/main/articles/porting-a-windows-mmo-client-to-the-browser.md)
are published. Private-port behavior must not be marked supported in the public library before it is published and tested.

## Keeping the plan complete

- Add newly discovered APIs and third-party dependencies to the coverage plan, even when they are deferred.
- Use an issue for each implementation-sized item: identify the program, API/version, current behavior, intended
  behavior, dependencies, acceptance test and browser limitations. Link that issue from the relevant area.
- Update [supported-api.md](docs/supported-api.md) only when the public implementation and evidence change.
- Record exclusions alongside supported features. "A particular game's dependency set is covered" and "the full
  DirectX 9 API is covered" are different claims. Neither follows from compiling or rendering a tutorial.

Ideas and help are welcome: open an issue first for anything large.
