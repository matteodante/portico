# Porting guide

How to bring a Direct3D 9 C++ program to the browser with Portico. The samples in `samples/` are complete examples:
Microsoft's tutorials, built unchanged.

## 1. Build it natively against Portico first

Before the browser, link the program against Portico on Windows: `portico::d3d9` (over ANGLE) instead of
`d3d9.lib`, and `portico::d3dx` instead of `d3dx9.lib`. Everything that differs from the system's Direct3D 9 shows
up here, with a debugger, RenderDoc and fast rebuilds.

```cmake
add_subdirectory(portico)               # or FetchContent
target_link_libraries(my_program PRIVATE portico::d3d9 portico::d3dx portico::win32 portico::win32_main)
```

Compare with the real thing: build the program once more against `d3d9.lib`, capture the same frame from both
(`tools/capture_window.py`) and compare them (`tools/compare_shots.py`). Make the frame deterministic first: the
samples freeze `timeGetTime()` with a forced include (`samples/fixed_time.h`).

Set `PORTICO_D3D9_LOG=<file>` to get the layer's log: every unsupported call is logged once.

## 2. Build it with Emscripten

```sh
emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/web
```

The same targets work: `portico::win32` provides `windows.h` and the Windows calls the program makes;
`portico::win32_main` provides `main()` and calls the program's `wWinMain` or `WinMain`.

Things to check:

- **Headers.** `<windows.h>` is Portico's subset. A missing declaration means the program uses an API Portico does
  not implement yet: add it to `modules/win32` (and open an issue or a pull request).
- **Files.** The program reads files through the C library. Small sets of files can be packaged with
  `--preload-file`; large ones need a file system backed by the network or by the browser's storage (OPFS).
  Paths with backslashes are accepted by Portico's D3DX loaders; for your own `fopen` calls, convert them or use
  forward slashes.
- **Blocking.** If the program blocks anywhere other than `Present` and `GetMessage` (`Sleep` in a loop, waiting for
  its own threads), link `portico::d3d9_worker` and serve the page cross-origin isolated (see
  [architecture.md](architecture.md#two-thread-setups)). `Sleep` works on the main thread too, but a long one
  freezes the page.
- **Stack.** Windows threads start with 1 MB of stack; `portico::win32` sets the same (`-sSTACK_SIZE=1MB`).
  Programs with large local arrays may need more.
- **Text.** `wchar_t` is 32 bits in Emscripten and 16 on Windows. Source that uses `L"..."` and the W functions
  works; code that assumes `sizeof(wchar_t) == 2` (binary file formats, hand-written UTF-16) does not.
- **Shaders.** v0.1 runs the fixed-function pipeline only. Programs that need vertex or pixel shaders do not render
  yet ([supported-api.md](supported-api.md)).

## 3. The page

The samples use `samples/shell.html`: a `#canvas`, and a message when the browser lacks JSPI. Your page needs the
same two things. The canvas size is set by the program's first window.

## 4. Test

The browser renders can be captured with `tools/browser_shots.mjs` (Chromium, with SwiftShader or the real GPU,
or Firefox) and compared with the Direct3D 9 references with `tools/parity.py`. See [testing.md](testing.md).
