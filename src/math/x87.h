#ifndef OKGF_X87_MATH_H
#define OKGF_X87_MATH_H
#include <stdint.h>
#include <float.h>
#include <string.h>
#include "platform.h"
#include "softfloat.h"
#include "okgf.h"

#if !defined(OKGF_PORTABLE_MATH) &&                                                                \
    (((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))) ||  \
     (defined(_MSC_VER) && defined(_M_IX86)))
#define OKGF_NATIVE_X87_TRIG 1
#endif
/* SoftFloat arithmetic with per-thread rounding state. SoftFloat precision 80 selects 64
 * significand bits. */
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && FLT_RADIX == 2,
               "OKGF arithmetic requires binary64 doubles");
typedef extFloat80_t OkgfFloat[1];

typedef struct OkgfMathState {
    uint_fast8_t precision, rounding, flags;
} OkgfMathState;

static inline OkgfMathState fp_enter(void) {
    OkgfMathState saved = {extF80_roundingPrecision, softfloat_roundingMode,
                           softfloat_exceptionFlags};
    unsigned precision = okgf_get_math_precision();
    extF80_roundingPrecision = precision == 24 ? 32 : precision == 53 ? 64 : 80;
    softfloat_roundingMode = softfloat_round_near_even;
    softfloat_exceptionFlags = 0;
    return saved;
}

static inline void fp_leave(OkgfMathState saved) {
    extF80_roundingPrecision = saved.precision;
    softfloat_roundingMode = saved.rounding;
    softfloat_exceptionFlags = saved.flags;
}
static inline extFloat80_t from_double(double x) {
    float64_t value;
    memcpy(&value.v, &x, 8);
    return f64_to_extF80(value);
}
static inline void fp_set(OkgfFloat a, double v) {
    *a = from_double(v);
}
static inline double fp_double(const OkgfFloat a) {
    float64_t value = extF80_to_f64(*a);
    double result;
    memcpy(&result, &value.v, 8);
    return result;
}
static inline int fp_compare(const OkgfFloat a, double b) {
    extFloat80_t bb = from_double(b);
    return extF80_lt(bb, *a) - extF80_lt(*a, bb);
}
static inline int64_t fp_integer(const OkgfFloat a, int away) {
    uint_fast8_t mode = away ? (a->signExp & 0x8000 ? softfloat_round_min : softfloat_round_max)
                             : softfloat_round_minMag;
    return extF80_to_i64(*a, mode, false);
}
#define FP_OP(op)                                                                                  \
    static inline void fp_##op(OkgfFloat out, const OkgfFloat a, const OkgfFloat b) {              \
        *out = extF80_##op(*a, *b);                                                                \
    }                                                                                              \
    static inline void fp_##op##_d(OkgfFloat out, const OkgfFloat a, double b) {                   \
        *out = extF80_##op(*a, from_double(b));                                                    \
    }
FP_OP(add)
FP_OP(sub)
FP_OP(mul)
FP_OP(div)
#undef FP_OP
static inline void fp_d_sub(OkgfFloat out, double a, const OkgfFloat b) {
    *out = extF80_sub(from_double(a), *b);
}
#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((visibility("hidden")))
#endif
void okgf_x87_sin(OkgfFloat out, const OkgfFloat a);
#define fp_sin okgf_x87_sin

#if defined(__GNUC__) && !defined(_WIN32)
__attribute__((visibility("hidden")))
#endif
void okgf_x87_cos(OkgfFloat out, const OkgfFloat a);
#define fp_cos okgf_x87_cos

static inline void fp_abs(OkgfFloat value) {
    value->signExp &= 0x7fff;
}

typedef extFloat80_t OkgfCoefficient;
static inline void fp_coefficient(OkgfFloat out, OkgfCoefficient value) {
    *out = value;
}
#endif
