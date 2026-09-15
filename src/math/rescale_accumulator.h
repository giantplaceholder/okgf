#ifndef OKGF_RESCALE_ACCUMULATOR_H
#define OKGF_RESCALE_ACCUMULATOR_H

#include "math/backend.h"

#ifdef OKGF_NATIVE_MATH

typedef double RescaleWeight;
typedef double RescaleValue;
static inline RescaleWeight rescale_weight(double value) {
    return value;
}
static inline RescaleValue rescale_product(RescaleWeight weight, uint8_t pixel) {
    return native_round(weight * pixel);
}
static inline RescaleValue rescale_add(RescaleValue a, RescaleValue b) {
    return native_round(a + b);
}

#else

typedef extFloat80_t RescaleValue;

static inline unsigned rescale_clz64(uint64_t value) {
    if (!value)
        return 64;
#if (defined(__GNUC__) || defined(__clang__)) && !defined(OKGF_PORTABLE_MATH)
    return (unsigned)__builtin_clzll(value);
#else
    unsigned count = 0;
    if (!(value >> 32)) {
        count += 32;
        value <<= 32;
    }
    if (!(value >> 48)) {
        count += 16;
        value <<= 16;
    }
    if (!(value >> 56)) {
        count += 8;
        value <<= 8;
    }
    if (!(value >> 60)) {
        count += 4;
        value <<= 4;
    }
    if (!(value >> 62)) {
        count += 2;
        value <<= 2;
    }
    return count + !(value >> 63);
#endif
}

/* Add finite pixel sums using two uint64_t limbs. Inputs must be normal extended-precision
 * values or zero. The product of a binary64 weight and an 8-bit pixel stays within the
 * extended exponent range, including after cancellation. */
static inline extFloat80_t rescale_add(extFloat80_t a, extFloat80_t b) {
    if (extF80_roundingPrecision != 80)
        return extF80_add(a, b);
    if (!a.signif)
        return b;
    if (!b.signif)
        return a;
    unsigned ae = a.signExp & 0x7fff, be = b.signExp & 0x7fff;
    if (ae < be || (ae == be && a.signif < b.signif)) {
        extFloat80_t swap = a;
        a = b;
        b = swap;
        unsigned exponent = ae;
        ae = be;
        be = exponent;
    }
    unsigned shift = ae - be;
    uint64_t bh = 0, bl = 0;
    int lost = 0;
    if (!shift) {
        bh = b.signif;
    } else if (shift < 64) {
        bh = b.signif >> shift;
        bl = b.signif << (64 - shift);
    } else if (shift == 64) {
        bl = b.signif;
    } else if (shift < 128) {
        bl = b.signif >> (shift - 64);
        lost = (b.signif << (128 - shift)) != 0;
    } else {
        lost = 1;
    }
    uint64_t high, low;
    if ((a.signExp ^ b.signExp) & 0x8000) {
        low = 0 - bl;
        high = a.signif - bh - (bl != 0);
        if (lost) {
            if (!low--)
                --high;
            low |= 1;
        }
        if (!high && !low)
            return (extFloat80_t){0};
        unsigned leading;
        if (high) {
            leading = rescale_clz64(high);
            if (leading) {
                high = (high << leading) | (low >> (64 - leading));
                low <<= leading;
            }
        } else {
            leading = rescale_clz64(low);
            high = low << leading;
            low = 0;
            leading += 64;
        }
        ae -= leading;
    } else {
        low = bl;
        high = a.signif + bh;
        if (high < a.signif) {
            low = (high << 63) | (low >> 1) | (low & 1);
            high = (UINT64_C(1) << 63) | (high >> 1);
            ++ae;
        }
        if (lost)
            low |= 1;
    }
    const uint64_t half = UINT64_C(1) << 63;
    if (low > half || (low == half && (high & 1))) {
        if (!++high) {
            high = half;
            ++ae;
        }
    }
    return (extFloat80_t){.signif = high, .signExp = (uint16_t)((a.signExp & 0x8000) | ae)};
}

/* Weights are stored as binary64. Multiplying a 53-bit significand by an 8-bit pixel needs at
 * most 61 bits. Round this product to the selected precision before adding it to the sum. */
typedef struct RescaleWeight {
    uint64_t significand;
    int exponent;
    uint16_t sign;
} RescaleWeight;

static inline RescaleWeight rescale_weight(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    unsigned exponent = (unsigned)(bits >> 52) & 2047;
    return (RescaleWeight){.significand = (bits & UINT64_C(0xfffffffffffff)) |
                                          (exponent ? UINT64_C(1) << 52 : 0),
                           .exponent = (exponent ? (int)exponent - 1023 : -1022) - 52,
                           .sign = (uint16_t)((bits >> 48) & 0x8000)};
}

static inline extFloat80_t rescale_product(RescaleWeight weight, uint8_t pixel) {
    uint64_t product = weight.significand * pixel;
    if (!product)
        return (extFloat80_t){0};
    unsigned shift = rescale_clz64(product);
    extFloat80_t result = {
        .signif = product << shift,
        .signExp = (uint16_t)(weight.sign | (weight.exponent + 63 - (int)shift + 16383))};
    if (extF80_roundingPrecision != 80) {
        unsigned discarded = extF80_roundingPrecision == 32 ? 40 : 11;
        uint64_t unit = UINT64_C(1) << discarded, half = unit >> 1;
        uint64_t tail = result.signif & (unit - 1);
        result.signif &= ~(unit - 1);
        if (tail > half || (tail == half && (result.signif & unit))) {
            result.signif += unit;
            if (!result.signif) {
                result.signif = UINT64_C(1) << 63;
                ++result.signExp;
            }
        }
    }
    return result;
}

#endif /* OKGF_NATIVE_MATH */
#endif
