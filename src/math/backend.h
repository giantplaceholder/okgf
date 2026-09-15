#ifndef OKGF_MATH_BACKEND_H
#define OKGF_MATH_BACKEND_H

/* Select the arithmetic operations used by the rescaler and geometry builders. */
#ifdef OKGF_NATIVE_MATH
#include "native.h"
#else
#include "x87.h"
#endif

/* Truncate to int64_t and interpret the low 32 bits as signed. Increment when the fractional
 * part of the saved binary64 value is at least 0.5. */
static inline int32_t fp_round_coordinate(const OkgfFloat value, int store_first) {
    OkgfFloat stored;
    fp_set(stored, fp_double(value));
    uint32_t low = (uint32_t)fp_integer(store_first ? stored : value, 0);
    int32_t integer;
    memcpy(&integer, &low, sizeof(integer));
    fp_sub_d(stored, stored, integer);
    low += fp_compare(stored, 0.5) >= 0;
    memcpy(&integer, &low, sizeof(integer));
    return integer;
}
#endif
