#ifndef OKGF_EDGE_MATH_H
#define OKGF_EDGE_MATH_H

#include "okgf_internal.h"
#include <float.h>

_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53,
               "Edge arithmetic requires binary64 doubles");

/* Finite arithmetic for rotation and polygon clipping, using 24-, 53-, or 64-bit significands
 * and binary64 stores. Supports integer ratios and products. Denominators must be nonzero. */
typedef struct OkgfEdgeFloat {
    uint64_t significand;
    int exponent;
    int negative;
    unsigned precision;
} OkgfEdgeFloat;

static inline OkgfEdgeFloat okgf_ratio(uint32_t numerator, uint32_t denominator,
                                       unsigned precision) {
    OkgfEdgeFloat result = {.precision = precision};
    if (!numerator)
        return result;
    uint64_t remainder = numerator, divisor = denominator;
    while (remainder >= 2 * divisor) {
        divisor <<= 1;
        ++result.exponent;
    }
    while (remainder < divisor) {
        remainder <<= 1;
        --result.exponent;
    }
    remainder -= divisor;
    result.significand = UINT64_C(1) << 63;
    unsigned discarded = 64 - precision;
    /* The normalized remainder and divisor fit in 32 bits. Compute up to 32 quotient bits per
     * division, keeping the exact remainder for final rounding to nearest, ties to even. */
    unsigned bits = precision - 1;
    while (bits) {
        unsigned chunk = bits > 32 ? 32 : bits;
        uint64_t numerator_bits = remainder << chunk;
        bits -= chunk;
        result.significand |= (numerator_bits / divisor) << (bits + discarded);
        remainder = numerator_bits % divisor;
    }
    uint64_t unit = UINT64_C(1) << discarded;
    if (2 * remainder > divisor || (2 * remainder == divisor && (result.significand & unit))) {
        result.significand += unit;
        if (!result.significand) {
            result.significand = UINT64_C(1) << 63;
            ++result.exponent;
        }
    }
    return result;
}

static inline OkgfEdgeFloat okgf_signed_ratio(int32_t numerator, int32_t denominator,
                                              unsigned precision) {
    uint32_t n = numerator < 0 ? 0u - (uint32_t)numerator : (uint32_t)numerator;
    uint32_t d = denominator < 0 ? 0u - (uint32_t)denominator : (uint32_t)denominator;
    OkgfEdgeFloat result = okgf_ratio(n, d, precision);
    result.negative = (numerator < 0) != (denominator < 0);
    return result;
}

static inline OkgfEdgeFloat okgf_multiply_integer(OkgfEdgeFloat value, int32_t integer) {
    uint32_t magnitude = integer < 0 ? 0u - (uint32_t)integer : (uint32_t)integer;
    value.negative ^= integer < 0;
    uint64_t low_product = (value.significand & UINT32_MAX) * magnitude;
    uint64_t high_product = (value.significand >> 32) * magnitude;
    uint64_t low = low_product + (high_product << 32);
    uint64_t high = (high_product >> 32) + (low < low_product);
    unsigned shift = 0;
    for (uint64_t bits = high; bits; bits >>= 1)
        ++shift;
    value.significand = low;
    uint64_t remainder = 0;
    if (shift) {
        value.significand = (high << (64 - shift)) | (low >> shift);
        remainder = low & ((UINT64_C(1) << shift) - 1);
    }
    if (value.precision < 64) {
        unsigned discarded = 64 - value.precision;
        uint64_t unit = UINT64_C(1) << discarded, half = unit >> 1;
        uint64_t tail = value.significand & (unit - 1);
        value.significand &= ~(unit - 1);
        if (tail > half || (tail == half && (remainder || (value.significand & unit)))) {
            value.significand += unit;
            if (!value.significand) {
                value.significand = UINT64_C(1) << 63;
                ++shift;
            }
        }
    } else if (shift) {
        uint64_t half = UINT64_C(1) << (shift - 1);
        if (remainder > half || (remainder == half && (value.significand & 1))) {
            if (!++value.significand) {
                value.significand = UINT64_C(1) << 63;
                ++shift;
            }
        }
    }
    value.exponent += (int)shift;
    return value;
}

static inline int32_t okgf_truncate64(OkgfEdgeFloat value, int binary_scale) {
    int exponent = value.exponent + binary_scale;
    if (!value.significand || exponent < 0)
        return 0;
    /* Only the low 32 bits of the int64_t conversion are used. An out-of-range x87 conversion
     * returns integer indefinite, whose low 32 bits are zero. */
    if (exponent >= 63)
        return 0;
    uint32_t integer = (uint32_t)(value.significand >> (63 - exponent));
    return okgf_signed32(value.negative ? 0u - integer : integer);
}

static inline double okgf_store_double(OkgfEdgeFloat value) {
    if (!value.significand)
        return 0;
    uint64_t mantissa = value.significand >> 11;
    unsigned remainder = (unsigned)(value.significand & 2047);
    if (remainder > 1024 || (remainder == 1024 && (mantissa & 1))) {
        if (++mantissa == (UINT64_C(1) << 53)) {
            mantissa >>= 1;
            ++value.exponent;
        }
    }
    uint64_t bits = ((uint64_t)value.negative << 63) | ((uint64_t)(value.exponent + 1023) << 52) |
                    (mantissa & UINT64_C(0xFFFFFFFFFFFFF));
    double result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

#endif
