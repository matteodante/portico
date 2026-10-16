# Testing

## Unit tests

```sh
ctest --test-dir build/web            # browser build: runs under Node.js
ctest --test-dir build/native -C Release
```

`tests/test_d3dx_math.cpp` checks the D3DX math against mathematical properties: a matrix times its inverse is
the identity, a quaternion and the matrix built from it rotate the same way, the view transform takes the eye to the
origin, the projections take the near plane to depth 0 and the far plane to 1, reflecting twice is the identity,
and so on.

## Screenshot parity

Every sample is rendered from the same source three ways, with `timeGetTime()` frozen so that every frame is the
same (`-DPORTICO_SAMPLES_FIXED_TIME=1234`):

1. with the system's Direct3D 9 (`-DPORTICO_SAMPLES_SYSTEM_D3D9=ON`, Windows): the reference;
2. with Portico natively, over ANGLE (Windows);
3. with Portico in the browser: Chromium with SwiftShader (what CI uses), Chromium on the GPU, Firefox.

`tools/compare_shots.py` decides whether two images are "the same to the eye": both are reduced to a quarter of
their size (sub-pixel rasterization differences do not count), then the mean difference must stay within 8/255
overall and 20/255 in every 32×32 tile, with limits on the share of very different pixels. A missing object or a
wrong texture fails even when the global numbers are small. `tools/parity.py` runs it over all the samples and
ignores the 4×4 pixels in each corner, which Windows 11 rounds in the captures of the reference windows.

The references are committed in `tests/reference/d3d9/`; CI compares the browser renders with them on every push.

### Making the references (Windows)

```sh
cmake -S . -B build/native-d3d9 -A Win32 -DPORTICO_ANGLE_DIR=<angle> -DPORTICO_SAMPLES_SYSTEM_D3D9=ON -DPORTICO_SAMPLES_FIXED_TIME=1234
cmake --build build/native-d3d9 --config Release
python tools/capture_window.py build/native-d3d9/samples/Release/tut05_textures_d3d9.exe tests/reference/d3d9/tut05_textures.png
```

### Comparing Portico

```sh
# native
cmake -S . -B build/native-fixed -A Win32 -DPORTICO_ANGLE_DIR=<angle> -DPORTICO_SAMPLES_FIXED_TIME=1234
cmake --build build/native-fixed --config Release
python tools/capture_window.py build/native-fixed/samples/Release/tut05_textures.exe build/shots/native/tut05_textures.png
python tools/parity.py tests/reference/d3d9 build/shots/native

# browser
emcmake cmake -S . -B build/web-fixed -G Ninja -DCMAKE_BUILD_TYPE=Release -DPORTICO_SAMPLES_FIXED_TIME=1234
cmake --build build/web-fixed
node tools/browser_shots.mjs --dir build/web-fixed/samples --out build/shots/browser
python tools/parity.py tests/reference/d3d9 build/shots/browser
```
