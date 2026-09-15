#ifndef OKGF_INTERNAL_H
#define OKGF_INTERNAL_H

#include <stdint.h>
#include <string.h>

/* Serialized pixels/streams are little endian and may be byte aligned. */
static inline uint16_t okgf_load16(const uint8_t *p) {
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}

static inline uint32_t okgf_load32(const uint8_t *p) {
    return (uint32_t)okgf_load16(p) | (uint32_t)okgf_load16(p + 2) << 16;
}

static inline void okgf_store16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static inline void okgf_store32(uint8_t *p, uint32_t value) {
    okgf_store16(p, (uint16_t)value);
    okgf_store16(p + 2, (uint16_t)(value >> 16));
}

static inline int32_t okgf_signed32(uint32_t value) {
    int32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static inline int32_t okgf_add32(int32_t a, int32_t b) {
    return okgf_signed32((uint32_t)a + (uint32_t)b);
}

static inline int32_t okgf_sub32(int32_t a, int32_t b) {
    return okgf_signed32((uint32_t)a - (uint32_t)b);
}

/* Original table initialization: 0x1005E8D0..0x1005E9B5.
 * The DLL builds alpha tables by repeatedly adding binary64(1 / 63) at 53-bit precision,
 * multiplying by the channel, and truncating. For channels and factors in 0..63, this equals
 * floor(product / 63) for factors 0..32 and floor((product - 1) / 63) for nonzero products at
 * factors 33..63. The integer formula preserves these startup values regardless of the current
 * math precision. */
static inline unsigned okgf_scale63_truncated(unsigned channel, unsigned factor) {
    unsigned product = channel * factor;
    return (product - (unsigned)(factor > 32 && channel != 0)) / 63;
}

/* The rounded table family has a separate rule. */
static inline unsigned okgf_scale63_rounded(unsigned channel, unsigned factor) {
    return (channel * factor + 31) / 63;
}

/* Table contributions can darken pixels even at alpha 0 or 63. Callers apply any
 * transparent/opaque shortcuts; planet edges always use the tables. */
static inline uint16_t okgf_blend565_truncated(uint16_t source, uint16_t dest, unsigned alpha) {
    unsigned r = okgf_scale63_truncated(source >> 11, alpha) +
                 okgf_scale63_truncated(dest >> 11, 63 - alpha);
    unsigned g = okgf_scale63_truncated((source >> 5) & 63, alpha) +
                 okgf_scale63_truncated((dest >> 5) & 63, 63 - alpha);
    unsigned b =
        okgf_scale63_truncated(source & 31, alpha) + okgf_scale63_truncated(dest & 31, 63 - alpha);
    return (uint16_t)(r << 11 | g << 5 | b);
}

static inline uint16_t okgf_shift_mask565(unsigned shift) {
    static const uint16_t masks[] = {0xFFFF, 0x7BEF, 0x39E7, 0x18E3, 0x0861, 0x0020};
    return shift < 6 ? masks[shift] : 0;
}

/* Use the first contiguous bit run and x86 shift counts modulo 32. Leave the result unmasked. */
static inline void okgf_mask_shifts(uint32_t mask, unsigned *left, unsigned *right) {
    unsigned start = 0, bits = 0;
    if (mask) {
        while (!(mask & 1)) {
            ++start;
            mask >>= 1;
        }
        while (mask & 1) {
            ++bits;
            mask >>= 1;
        }
    }
    *left = start;
    *right = (8u - bits) & 31;
}

#endif
