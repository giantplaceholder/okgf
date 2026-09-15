# okgf.dll

A C reimplementation of `okgf.dll` for Space Rangers HD. Only the functions used
by the latest game build are implemented.

Original DLL SHA-256:
`0ca91f482e1490bcf71da0672d1a72604a591f1b415dbc4c4f329f176eea5d55`

Source comments use virtual addresses at image base `0x10000000`.

## Build

Requires CMake 3.20+, a C11 compiler, libpng, and libjpeg.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `okgf.dll` on Windows, `libokgf.dylib` on macOS, or `libokgf.so` on Linux.
Headers are in `include/`. The CMake target is `okgf`.

Set `-DOKGF_MATH_BACKEND=...` to choose the math implementation:

- `COMPATIBLE` (default): compatibility arithmetic.
- `EXACT`: x87 arithmetic and trigonometry; requires x86.
- `NATIVE`: faster host arithmetic, with possible numerical differences.

## License

[MIT](LICENSE). Bundled SoftFloat uses the [BSD 3-Clause license](vendor/softfloat/COPYING.txt).
