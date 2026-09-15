#ifndef OKGF_NATIVE_MATH_H
#define OKGF_NATIVE_MATH_H

#include "okgf.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Host arithmetic with per-operation rounding and binary64 stores. NATIVE uses float precision
 * for 24-bit requests and double precision otherwise. Reassociation and fused multiply-add are
 * disabled. */
_Static_assert(FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_RADIX == 2,
               "Native math requires IEEE binary32 and binary64");
typedef double OkgfFloat[1];
typedef unsigned OkgfMathState;
#ifdef _MSC_VER
static __declspec(thread) unsigned native_precision;
#else
static _Thread_local unsigned native_precision;
#endif

static inline OkgfMathState fp_enter(void) {
    unsigned saved = native_precision;
    native_precision = okgf_get_math_precision();
    return saved;
}
static inline void fp_leave(OkgfMathState saved) {
    native_precision = saved;
}
static inline double native_round(double value) {
    return native_precision == 24 ? (double)(float)value : value;
}
static inline void fp_set(OkgfFloat out, double value) {
    *out = value;
}
static inline double fp_double(const OkgfFloat value) {
    return *value;
}
static inline int fp_compare(const OkgfFloat a, double b) {
    return (*a > b) - (*a < b);
}
static inline int64_t fp_integer(const OkgfFloat value, int away) {
    double v = away ? (*value < 0 ? floor(*value) : ceil(*value)) : trunc(*value);
    /* Return x87's integer-indefinite value when the result is outside int64_t range. */
    if (!(v >= -0x1p63 && v < 0x1p63))
        return INT64_MIN;
    return (int64_t)v;
}
#define FP_OP(name, op)                                                                            \
    static inline void fp_##name(OkgfFloat out, const OkgfFloat a, const OkgfFloat b) {            \
        *out = native_round(*a op * b);                                                            \
    }                                                                                              \
    static inline void fp_##name##_d(OkgfFloat out, const OkgfFloat a, double b) {                 \
        *out = native_round(*a op b);                                                              \
    }
FP_OP(add, +)
FP_OP(sub, -)
FP_OP(mul, *)
FP_OP(div, /)
#undef FP_OP
static inline void fp_d_sub(OkgfFloat out, double a, const OkgfFloat b) {
    *out = native_round(a - *b);
}
static inline void fp_sin(OkgfFloat out, const OkgfFloat value) {
    *out = sin(*value);
}
static inline void fp_cos(OkgfFloat out, const OkgfFloat value) {
    *out = cos(*value);
}
static inline void fp_abs(OkgfFloat value) {
    *value = fabs(*value);
}

typedef struct OkgfCoefficient {
    uint64_t signif;
    uint16_t signExp;
} OkgfCoefficient;
static inline void fp_coefficient(OkgfFloat out, OkgfCoefficient value) {
    double magnitude = ldexp((double)value.signif, (value.signExp & 0x7fff) - 16383 - 63);
    *out = value.signExp & 0x8000 ? -magnitude : magnitude;
}

#endif
