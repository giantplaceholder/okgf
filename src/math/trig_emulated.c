#include "math/backend.h"
#include <assert.h>

#if !defined(OKGF_NATIVE_X87_TRIG) && !defined(OKGF_NATIVE_MATH)

#ifdef LITTLEENDIAN
#define QUAD(low, high)                                                                            \
    {                                                                                              \
        {                                                                                          \
            UINT64_C(low), UINT64_C(high)                                                          \
        }                                                                                          \
    }
#else
#define QUAD(low, high)                                                                            \
    {                                                                                              \
        {                                                                                          \
            UINT64_C(high), UINT64_C(low)                                                          \
        }                                                                                          \
    }
#endif
#include "tables/rescale_sine_table.inc"
#undef QUAD

/* Original instructions: FSIN at 0x10061359, FCOS at 0x1003462B.
 * Sine and cosine for abs(x) < 3 * binary64(3.141592).
 * The x87 sine model approximates sin(x * pi / P), where P is Intel's
 * truncated 68-bit approximation to pi.
 *
 * Reduce by multiples of P / 2 and scale the remainder by pi / P. Binary128 holds the
 * reduction exactly and provides guard bits for the polynomial evaluation.
 * For abs(remainder) <= 0.786, the Taylor remainder is below 2^-140,
 * so binary128 rounding dominates the error.
 * The result can differ in its last extended-precision bit from a processor's FSIN/FCOS
 * result. */
static void software_trig(OkgfFloat out, const OkgfFloat value, unsigned is_cosine) {
    extFloat80_t a = *value;
    unsigned negative = a.signExp >> 15;
    a.signExp &= 0x7fff;
    assert(extF80_lt(a, from_double(3 * 3.141592)));
    /* Compare encoded values against boundaries rounded down to avoid host floating-point
     * rounding. */
    unsigned quadrant = 0;
    while (quadrant < 6 && (a.signExp > quadrant_boundary[quadrant].signExp ||
                            (a.signExp == quadrant_boundary[quadrant].signExp &&
                             a.signif > quadrant_boundary[quadrant].signif)))
        ++quadrant;
    float128_t reduced = f128_sub(extF80_to_f128(a), pi_multiple[quadrant]);
    reduced = f128_mul(reduced, pi_ratio);
    float128_t square = f128_mul(reduced, reduced);
    unsigned function_quadrant = quadrant + is_cosine;
    const float128_t *coefficients = function_quadrant & 1 ? cosine : sine;
    float128_t result = coefficients[17];
    for (int i = 16; i >= 0; --i)
        result = f128_mulAdd(result, square, coefficients[i]);
    if (!(function_quadrant & 1))
        result = f128_mul(result, reduced);
    *out = f128_to_extF80(result);
    if ((!is_cosine && negative) ^ ((function_quadrant & 3) >= 2))
        out->signExp ^= 0x8000;
}

void okgf_x87_sin(OkgfFloat out, const OkgfFloat value) {
    software_trig(out, value, 0);
}

void okgf_x87_cos(OkgfFloat out, const OkgfFloat value) {
    software_trig(out, value, 1);
}

#endif
