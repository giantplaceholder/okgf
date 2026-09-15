#include "okgf.h"
#include "math/rescale_accumulator.h"
#include <stddef.h>
#include <stdlib.h>
#include <fenv.h>
#include <math.h>

typedef struct RescaleMath {
    OkgfFloat a, b;
} RescaleMath;
typedef struct RescaleTap {
    int32_t source_index;
    union {
        RescaleWeight exact;
        double host;
    } weight;
} RescaleTap;
typedef struct RescaleContributions {
    int32_t count, first;
    unsigned arithmetic;
    double center;
    RescaleTap *taps;
} RescaleContributions;

static void sinc(RescaleMath *m, double x) {
    fp_set(m->a, x);
    fp_mul_d(m->a, m->a, 3.141592);
    if (fp_compare(m->a, 0) == 0) {
        fp_set(m->a, 1);
    } else {
        fp_sin(m->b, m->a);
        fp_div(m->a, m->b, m->a);
    }
}

/* Store the kernel result in m->a. Constants use the original binary64 values, including the
 * shortened pi and 1/6 constants. */
static void kernel(RescaleMath *m, int32_t filter, double x) {
    if (filter < 1 || filter > 6) {
        fp_set(m->a, x > -0.5 && x <= 0.5);
        return;
    }
    if (x < 0)
        x = -x;
    fp_set(m->a, x);
    if ((filter == 1 || filter == 4) && x < 1) {
        if (filter == 1) {
            fp_d_sub(m->a, 1, m->a);
        } else {
            fp_mul_d(m->a, m->a, 2);
            fp_sub_d(m->a, m->a, 3);
            fp_mul_d(m->a, m->a, x);
            fp_mul_d(m->a, m->a, x);
            fp_add_d(m->a, m->a, 1);
        }
        return;
    }
    if (filter == 2 && x < 1.5) {
        if (x < 0.5) {
            fp_mul(m->a, m->a, m->a);
            fp_d_sub(m->a, 0.75, m->a);
        } else {
            fp_sub_d(m->a, m->a, 1.5);
            fp_mul(m->a, m->a, m->a);
            fp_mul_d(m->a, m->a, 0.5);
        }
        return;
    }
    if (filter == 3 && x < 2) {
        if (x < 1) {
            fp_mul(m->b, m->a, m->a);
            fp_mul(m->a, m->a, m->b);
            fp_mul_d(m->a, m->a, 0.5);
            fp_sub(m->a, m->a, m->b);
            fp_add_d(m->a, m->a, 0.6666666666666666);
        } else {
            fp_d_sub(m->b, 2, m->a);
            fp_mul(m->a, m->b, m->b);
            fp_mul(m->a, m->a, m->b);
            fp_mul_d(m->a, m->a, 0.16666666666666666);
        }
        return;
    }
    if (filter == 5 && x < 3) {
        fp_mul_d(m->a, m->a, 0.3333333333333333);
        sinc(m, fp_double(m->a));
        double first = fp_double(m->a);
        sinc(m, x);
        fp_mul_d(m->a, m->a, first);
        return;
    }
    if (filter == 6 && x < 2) {
        fp_mul(m->b, m->a, m->a);
        fp_mul(m->a, m->a, m->b);
        fp_mul_d(m->a, m->a, x < 1 ? 7.0 : -2.3333333333333335);
        fp_mul_d(m->b, m->b, x < 1 ? -12.0 : 12.0);
        fp_add(m->a, m->a, m->b);
        if (x >= 1) {
            fp_set(m->b, x);
            fp_mul_d(m->b, m->b, -20.0);
            fp_add(m->a, m->a, m->b);
        }
        fp_add_d(m->a, m->a, x < 1 ? 5.333333333333333 : 10.666666666666666);
        fp_mul_d(m->a, m->a, 0.16666666666666666);
        return;
    }
    fp_set(m->a, 0);
}

static void free_contributions(RescaleContributions *rows) {
    if (!rows)
        return;
    free(rows[0].taps);
    free(rows);
}

static RescaleContributions *contributions(RescaleMath *m, int32_t dest_size, int32_t source_size,
                                           int32_t filter) {
    const double supports[] = {0.5, 1, 1.5, 2, 1, 3, 2};
    double support = supports[filter >= 1 && filter <= 6 ? filter : 0];
    fp_set(m->a, dest_size);
    fp_div_d(m->a, m->a, source_size);
    double scale = fp_double(m->a), radius = support, inverse = 1;
    if (scale < 1) {
        fp_set(m->a, support);
        fp_div_d(m->a, m->a, scale);
        radius = fp_double(m->a);
        fp_set(m->a, 1);
        fp_div_d(m->a, m->a, scale);
        inverse = fp_double(m->a);
    }
    RescaleContributions *rows = calloc((size_t)dest_size, sizeof(*rows));
    if (!rows)
        return NULL;
    size_t total = 0;
    for (int32_t i = 0; i < dest_size; ++i) {
        fp_set(m->a, i);
        fp_div_d(m->a, m->a, scale);
        double center = fp_double(m->a);
        /* FST saves center but leaves its extended value for the left bound. */
        fp_sub_d(m->a, m->a, radius);
        int64_t first = fp_integer(m->a, 1);
        fp_set(m->a, center);
        fp_add_d(m->a, m->a, radius);
        int64_t last = fp_integer(m->a, 0);
        if (first < -INT32_MAX || last > INT32_MAX || last - first > INT32_MAX - 1)
            goto failure;
        rows[i].count = last >= first ? (int32_t)(last - first + 1) : 0;
        rows[i].center = center;
        rows[i].first = (int32_t)first;
        if ((size_t)rows[i].count > SIZE_MAX / sizeof(RescaleTap) - total)
            goto failure;
        total += (size_t)rows[i].count;
    }
    RescaleTap *pool = calloc(total ? total : 1, sizeof(*pool));
    if (!pool)
        goto failure;
    rows[0].taps = pool;
    unsigned precision = okgf_get_math_precision();
#ifdef OKGF_PORTABLE_MATH
    int host_rounding = 0;
#else
    int host_rounding = FLT_EVAL_METHOD == 0 && fegetround() == FE_TONEAREST;
#endif
    for (int32_t i = 0; i < dest_size; ++i) {
        double center = rows[i].center;
        int64_t first = rows[i].first, last = first + rows[i].count - 1;
        rows[i].taps = pool;
        pool += rows[i].count;
#ifdef OKGF_NATIVE_MATH
        rows[i].arithmetic = precision == 24 ? 24 : 53;
        (void)host_rounding;
#else
        rows[i].arithmetic = host_rounding && (precision == 24 || precision == 53) ? precision : 64;
#endif
        for (int64_t j = first; j <= last; ++j) {
            int64_t index = j < 0 ? -j : j >= source_size ? (int64_t)2 * source_size - j - 1 : j;
            /* Reflect once and check bounds, including for taps with zero weight. */
            if (index < 0 || index >= source_size)
                goto failure;
            fp_set(m->a, center);
            fp_sub_d(m->a, m->a, j);
            if (scale < 1)
                fp_div_d(m->a, m->a, inverse);
            kernel(m, filter, fp_double(m->a));
            if (scale < 1)
                fp_div_d(m->a, m->a, inverse);
            double weight = fp_double(m->a);
            RescaleTap *tap = &rows[i].taps[j - first];
            tap->source_index = (int32_t)index;
            tap->weight.host = weight;
#ifndef OKGF_NATIVE_MATH
            /* Host float/double operations reproduce 24-/53-bit x87 rounding when all inputs,
             * products, and sums stay normal. The magnitude bounds below and a tap count no greater
             * than 2^31 keep sums in that range. At 24 bits, weights must also be exactly
             * representable as float. Use SoftFloat for other cases. */
            double magnitude = fabs(weight);
            if (weight != 0 && (!(magnitude >= 0x1p-80 && magnitude <= 0x1p16) ||
                                (precision == 24 && (double)(float)weight != weight)))
                rows[i].arithmetic = 64;
#endif
        }
        if (rows[i].arithmetic == 64) {
            for (int32_t j = 0; j < rows[i].count; ++j) {
                RescaleTap *tap = &rows[i].taps[j];
                tap->weight.exact = rescale_weight(tap->weight.host);
            }
        }
    }
    return rows;
failure:
    free_contributions(rows);
    return NULL;
}

static uint8_t sample(RescaleMath *m, const RescaleContributions *row, const uint8_t *source,
                      ptrdiff_t stride) {
    if (row->arithmetic == 24) {
        float sum = 0;
        for (int32_t i = 0; i < row->count; ++i) {
            const RescaleTap *tap = &row->taps[i];
            float product = (float)tap->weight.host * source[(ptrdiff_t)tap->source_index * stride];
            sum += product;
        }
        return sum <= 0 ? 0 : sum >= 255 ? 255 : (uint8_t)sum;
    }
    if (row->arithmetic == 53) {
        double sum = 0;
        for (int32_t i = 0; i < row->count; ++i) {
            const RescaleTap *tap = &row->taps[i];
            double product = tap->weight.host * source[(ptrdiff_t)tap->source_index * stride];
            sum += product;
        }
        return sum <= 0 ? 0 : sum >= 255 ? 255 : (uint8_t)sum;
    }
    fp_set(m->a, 0);
    for (int32_t i = 0; i < row->count; ++i) {
        const RescaleTap *tap = &row->taps[i];
        RescaleValue term =
            rescale_product(tap->weight.exact, source[(ptrdiff_t)tap->source_index * stride]);
        *m->a = rescale_add(*m->a, term);
    }
    if (fp_compare(m->a, 0) < 0)
        return 0;
    if (fp_compare(m->a, 255) > 0)
        return 255;
    return (uint8_t)fp_integer(m->a, 0);
}

void OKGF_CALL OKGF_Rescale(void *dest, int32_t width, int32_t height, int32_t dest_pitch,
                            const void *source, int32_t source_width, int32_t source_height,
                            int32_t source_pitch, int32_t bytes_per_pixel, int32_t filter) {
    if (width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0 ||
        bytes_per_pixel <= 0 || dest_pitch <= 0 || (int64_t)width * bytes_per_pixel > dest_pitch ||
        (uint64_t)source_height * (uint32_t)dest_pitch > SIZE_MAX)
        return;
    RescaleMath m = {0};
    OkgfMathState saved = fp_enter();
    RescaleContributions *horizontal = contributions(&m, width, source_width, filter);
    RescaleContributions *vertical = contributions(&m, height, source_height, filter);
    uint8_t *temp = malloc((size_t)source_height * dest_pitch);
    if (!horizontal || !vertical || !temp)
        goto done;
    for (int32_t channel = 0; channel < bytes_per_pixel; ++channel) {
        for (int32_t y = 0; y < source_height; ++y) {
            const uint8_t *row = (const uint8_t *)source + (ptrdiff_t)y * source_pitch + channel;
            for (int32_t x = 0; x < width; ++x)
                temp[(ptrdiff_t)y * dest_pitch + (ptrdiff_t)x * bytes_per_pixel + channel] =
                    sample(&m, &horizontal[x], row, bytes_per_pixel);
        }
    }
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            for (int32_t channel = 0; channel < bytes_per_pixel; ++channel) {
                ptrdiff_t offset = (ptrdiff_t)x * bytes_per_pixel + channel;
                ((uint8_t *)dest)[(ptrdiff_t)y * dest_pitch + offset] =
                    sample(&m, &vertical[y], temp + offset, dest_pitch);
            }
        }
    }
done:
    free(temp);
    free_contributions(horizontal);
    free_contributions(vertical);
    fp_leave(saved);
}
