# Berkeley SoftFloat subset

Upstream: https://www.jhauser.us/arithmetic/SoftFloat.html
Archive: https://www.jhauser.us/arithmetic/SoftFloat-3e.zip
Release: 3e (2018-01-20)
Archive SHA-256: `21130ce885d35c1fe73fc1e1bf2244178167e05c6747cad5f450cc991714c746`
License: BSD 3-Clause; see [COPYING.txt](COPYING.txt).

The selected upstream C sources and headers are unmodified. The subset supplies
extended arithmetic and conversions, plus binary128 arithmetic for the bounded
resampling/planet trigonometry. Generic integer primitives are retained for 32-bit
compilers without a 128-bit integer type. Unused archive members are not linked.

`platform.h` and `CMakeLists.txt` are project integration files. Rounding state
is thread-local. Library arithmetic selects nearest/even and the configured
x87 precision, then restores that state on return. SoftFloat precision values
32/64/80 correspond to 24/53/64 significand bits, with the extended exponent
range retained. The software trigonometric result retains 64 significand bits.
Symbols are hidden from the shared-library interface on ELF/Mach-O platforms.
No download, pkg-config, MPFR or GMP is needed to build this vendored subset.
