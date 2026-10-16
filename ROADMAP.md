# Roadmap

Portico grows by extracting the rest of the platform layer of a complete port (a 2000s Windows MMO client running
in the browser against its original servers), one module at a time, each with its own tests on programs that are not
that game.

| Next | What | Status in the original port |
|---|---|---|
| Win32, complete | Threads and synchronization (fair critical sections), files, registry (kept in browser storage), code pages, IME, timers, resources from `.rc` files | Done, to be extracted |
| MFC subset (`afx`) | `CString`, `CFile`, `CArchive` (byte-compatible with MFC), runtime classes, `CWinApp`, message maps | Done, to be extracted |
| DirectSound 8 | 2D and 3D sound on miniaudio and Web Audio, with the 3D model measured on the real DirectSound | Done, to be extracted |
| Winsock over WebSocket | `select` and `WSAAsyncSelect` semantics, plus a WebSocket↔TCP gateway with an allowlist | Done, to be extracted |
| Asset installer | Game files in the browser's Origin Private File System: manifest with SHA-256, resumable downloads, atomic updates | Done, to be extracted |
| Programmable shaders | Shader model 1–3 bytecode to GLSL | Not started |
| Missing fixed-function pieces | Scissor test, user clip planes, point sprites, cube and volume textures, bump-environment mapping, state blocks | Not started |
| Write-up | The design and the measurements of the original port | Published separately |

Ideas and help are welcome: open an issue first for anything large.
