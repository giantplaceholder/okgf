# okgf.dll

A C reimplementation of `okgf.dll` for Space Rangers HD and Space Rangers 1.
Implements the functions used by the latest HD build and all 119 SR1 graphics
and image imports. AVI playback and zlib exports are outside the scope.

Original DLL SHA-256:
`0ca91f482e1490bcf71da0672d1a72604a591f1b415dbc4c4f329f176eea5d55`

Source comments use virtual addresses at image base `0x10000000`; unqualified
original addresses refer to the HD DLL.

## Build

Requires CMake 3.20+, a C11 compiler, libpng, and libjpeg.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `okgf.dll` on Windows, `libokgf.dylib` on macOS, or `libokgf.so` on Linux.
Headers are in `include/`. The CMake target is `okgf`.

The default game release is HD. Build SR1 with:

```sh
cmake -S . -B build-sr1 -DCMAKE_BUILD_TYPE=Release -DOKGF_GAME_RELEASE=SR1
cmake --build build-sr1
```

`OKGF_GAME_RELEASE` accepts `SRHD` or `SR1`. For builds without CMake, define
`OKGF_GAME_RELEASE=OKGF_GAME_SR1` when compiling the library. The release selects
RGBA font blending, rescale arithmetic order, and rotation-edge rounding.
RGB555 functions retain SR1's original alpha, mask, and line-position quirks.
One library instance targets one release.

Set `-DOKGF_MATH_BACKEND=...` to choose the math implementation:

- `COMPATIBLE` (default): compatibility arithmetic.
- `EXACT`: x87 arithmetic and trigonometry; requires x86.
- `NATIVE`: faster host arithmetic, with possible numerical differences.

## License

[MIT](LICENSE). Bundled SoftFloat uses the [BSD 3-Clause license](vendor/softfloat/COPYING.txt).
