# Contributing

Thanks for helping. Portico is small and young, so the rules are few.

## Before you start

- For anything larger than a fix, open an issue first and describe what you want to change and why.
- Portico implements Windows APIs for programs that call them. A change is justified by a real program that needs
  it: say which call, with which arguments, and what the program expected.
- Licensing: contributions are under the [MIT license](LICENSE). Do not copy code from Wine, ReactOS, Microsoft's
  SDKs or any project whose license is not compatible; write it from the public documentation and say so in the
  pull request. Reading other implementations to understand behavior is fine; copying is not.

## Building and testing

See the [README](README.md#quick-start) and [docs/testing.md](docs/testing.md). Before a pull request:

- the browser build and the native build (if you are on Windows) compile without new warnings;
- `ctest` passes;
- the samples still match the Direct3D 9 references (`tools/parity.py`); CI checks the browser renders.

A change to the rendering should come with a way to see it: a sample, or a test with a reference image made with
the system's Direct3D 9 (`tools/capture_window.py`).

## Style

- C++17, the style of the surrounding code (clang-format is not enforced yet).
- Comments explain why, and cite the Direct3D or Windows documentation when behavior follows it.
- Commit messages: a short summary line, then what and why.

## Reporting bugs

Include the browser and its version, the program (or a small one that shows the problem), what you expected and what
you got, and the console output (`[portico-d3d9]` lines).
