#include "okgf.h"
#include "math/edge.h"
#include "okgf_internal.h"
#include "math/backend.h"
#include <stdlib.h>
#include <string.h>

#ifndef OKGF_NATIVE_X87_TRIG
static const OkgfCoefficient rotation_trig[3][256][2] = {
#include "tables/rotation_trig.inc"
};
#endif

static int32_t round_source(double value) {
    /* A 24-bit multiply can round INT32_MAX to 2^31. Convert to int64_t, then interpret the low 32
     * bits as signed. */
    int32_t integer = okgf_signed32((uint32_t)(int64_t)value);
    return okgf_add32(integer, value - integer >= 0.5);
}

void OKGF_CALL okgf_rotation_edge(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t *dest_x,
                                  int32_t sx0, int32_t sy0, int32_t sx1, int32_t sy1,
                                  int32_t *source_coordinates) {
    if (y0 >= y1)
        return;
    unsigned precision = okgf_get_math_precision();
    uint8_t *coordinates = (uint8_t *)source_coordinates;
    for (int64_t y = y0; y <= y1; ++y) {
        OkgfEdgeFloat fraction =
            okgf_signed_ratio(okgf_sub32((int32_t)y, y0), okgf_sub32(y1, y0), precision);
        int32_t dx =
            okgf_add32(x0, okgf_truncate64(okgf_multiply_integer(fraction, okgf_sub32(x1, x0)), 0));
        memcpy((void *)dest_x++, &dx, sizeof(dx));
        OkgfEdgeFloat source_x = okgf_multiply_integer(fraction, okgf_sub32(sx1, sx0));
        int32_t integer = okgf_truncate64(source_x, 0);
        int32_t pair[2] = {
            okgf_add32(okgf_add32(sx0, integer), okgf_store_double(source_x) - integer >= 0.5),
            okgf_add32(sy0, round_source(okgf_store_double(
                                okgf_multiply_integer(fraction, okgf_sub32(sy1, sy0)))))};
        memcpy(coordinates, pair, sizeof(pair));
        coordinates += sizeof(OkgfRotationScanline);
    }
}

typedef struct Corner {
    int32_t x, y, sx, sy;
} Corner;

/* Original corner calculations: 0x10062078..0x1006226B.
 * FST stores a binary64 copy but leaves the extended value on the stack. Corner calculations
 * reload that copy at different points; preserve those rounding boundaries. */
static void rotate_corners(Corner corners[4], unsigned angle, unsigned precision) {
    unsigned table = precision == 24 ? 0 : precision == 53 ? 1 : 2;
    OkgfFloat cosine, sine;
#ifdef OKGF_NATIVE_X87_TRIG
    /* Evaluate coefficients with this CPU's FSIN/FCOS. Other builds use precomputed coefficients.
     */
    OkgfFloat argument;
    fp_set(argument, angle);
    fp_mul_d(argument, argument, 0.00390625);
    fp_mul_d(argument, argument, 360);
    fp_mul_d(argument, argument, 0.01745329222222222);
    double stored = fp_double(argument);
    fp_sin(sine, argument);
    fp_set(argument, stored);
    fp_cos(cosine, argument);
    (void)table;
#else
    fp_coefficient(cosine, rotation_trig[table][angle][0]);
    fp_coefficient(sine, rotation_trig[table][angle][1]);
#endif
    OkgfFloat xc, ys, xs, yc, rc, rs, bs, bc, value;
    fp_mul_d(xc, cosine, corners[0].x);
    fp_mul_d(ys, sine, corners[0].y);
    fp_mul_d(xs, sine, corners[0].x);
    fp_mul_d(yc, cosine, corners[0].y);
    double xc_saved = fp_double(xc), ys_saved = fp_double(ys), xs_saved = fp_double(xs);
    fp_set(cosine, fp_double(cosine));
    fp_mul_d(rc, cosine, corners[1].x);
    fp_mul_d(rs, sine, corners[1].x);
    fp_mul_d(bs, sine, corners[2].y);
    fp_mul_d(bc, cosine, corners[2].y);
    double rc_saved = fp_double(rc), rs_saved = fp_double(rs), bc_saved = fp_double(bc);

    fp_set(value, xc_saved);
    fp_sub(value, value, ys);
    corners[0].x = fp_round_coordinate(value, 1);
    fp_add(value, xs, yc);
    corners[0].y = fp_round_coordinate(value, 0);
    fp_sub_d(value, rc, ys_saved);
    corners[1].x = fp_round_coordinate(value, 0);
    fp_add(value, rs, yc);
    corners[1].y = fp_round_coordinate(value, 1);
    fp_set(value, rc_saved);
    fp_sub(value, value, bs);
    corners[2].x = fp_round_coordinate(value, 0);
    fp_add_d(value, bc, rs_saved);
    corners[2].y = fp_round_coordinate(value, 0);
    fp_set(value, xc_saved);
    fp_sub(value, value, bs);
    corners[3].x = fp_round_coordinate(value, 1);
    fp_set(value, bc_saved);
    fp_add_d(value, value, xs_saved);
    corners[3].y = fp_round_coordinate(value, 0);
}

static void edge(const Corner *first, const Corner *last, OkgfRotationFrame *frame,
                 int32_t *positions, int right) {
    int32_t row = first->y - frame->dest_y;
    int32_t *coordinates =
        right ? &frame->scanlines[row].source_x_end : &frame->scanlines[row].source_x_start;
    okgf_rotation_edge(first->x, first->y, last->x, last->y, positions + row, first->sx, first->sy,
                       last->sx, last->sy, coordinates);
}

OkgfRotationBuffer *OKGF_CALL OKGR_RotateBuf_Build(int32_t width, int32_t height,
                                                   int32_t source_width, int32_t source_height,
                                                   int32_t center_x, int32_t center_y) {
    /* Smaller shapes can collapse to one row: the original then reads stale
     * edge scratch data and uninitialized source coordinates. */
    if (width < 3 || height < 3 || source_width < 1 || source_height < 1)
        return NULL;
    OkgfRotationBuffer *rotation = calloc(1, sizeof(*rotation));
    if (!rotation)
        return NULL;
    unsigned precision = okgf_get_math_precision();
    OkgfMathState saved = fp_enter();
    for (unsigned angle = 0; angle < 256; ++angle) {
        Corner corners[4] = {
            {okgf_sub32(0, center_x), okgf_sub32(0, center_y), 0, 0},
            {okgf_sub32(width - 1, center_x), okgf_sub32(0, center_y), source_width - 1, 0},
            {okgf_sub32(width - 1, center_x), okgf_sub32(height - 1, center_y), source_width - 1,
             source_height - 1},
            {okgf_sub32(0, center_x), okgf_sub32(height - 1, center_y), 0, source_height - 1}};
        rotate_corners(corners, angle, precision);
        unsigned quadrant = angle <= 64 ? 0 : angle <= 128 ? 1 : angle <= 192 ? 2 : 3;
        unsigned top = (4 - quadrant) & 3, bottom = (top + 2) & 3;
        unsigned right = (top + 1) & 3, left = (top + 3) & 3;
        OkgfRotationFrame *frame = &rotation->frames[angle];
        /* A distant center can collapse edges even when width and height are at least 3. Reject
         * collapsed or unordered edges before they leave rows unwritten or address samples outside
         * the frame. */
        int64_t rows = (int64_t)corners[bottom].y - corners[top].y + 1;
        if (rows < 2 || rows > INT32_MAX || corners[left].y < corners[top].y ||
            corners[left].y > corners[bottom].y || corners[right].y < corners[top].y ||
            corners[right].y > corners[bottom].y ||
            (uint64_t)rows > SIZE_MAX / sizeof(*frame->scanlines))
            goto failure;
        frame->dest_y = corners[top].y;
        frame->scanline_count = (int32_t)rows;
        size_t count = (size_t)frame->scanline_count;
        frame->scanlines = calloc(count, sizeof(*frame->scanlines));
        int32_t *positions = calloc(count, 2 * sizeof(*positions));
        if (!frame->scanlines || !positions) {
            free(positions);
            goto failure;
        }
        edge(&corners[top], &corners[right], frame, positions + count, 1);
        edge(&corners[right], &corners[bottom], frame, positions + count, 1);
        edge(&corners[top], &corners[left], frame, positions, 0);
        edge(&corners[left], &corners[bottom], frame, positions, 0);
        frame->min_x = 99999999;
        frame->max_x = -99999999;
        for (size_t row = 0; row < count; ++row) {
            OkgfRotationScanline *line = &frame->scanlines[row];
            line->dest_x = positions[row];
            int64_t pixels = (int64_t)positions[count + row] - positions[row] + 1;
            if (pixels < 1 || pixels > INT32_MAX) {
                free(positions);
                goto failure;
            }
            line->pixel_count = (int32_t)pixels;
            if (positions[row] < frame->min_x)
                frame->min_x = positions[row];
            if (positions[count + row] > frame->max_x)
                frame->max_x = positions[count + row];
            OkgfEdgeFloat reciprocal = okgf_ratio(1, (uint32_t)line->pixel_count, precision);
            line->source_dx_16_16 =
                okgf_truncate64(okgf_multiply_integer(reciprocal, okgf_sub32(line->source_x_end,
                                                                             line->source_x_start)),
                                16);
            line->source_dy_16_16 =
                okgf_truncate64(okgf_multiply_integer(reciprocal, okgf_sub32(line->source_y_end,
                                                                             line->source_y_start)),
                                16);
        }
        free(positions);
    }
    fp_leave(saved);
    return rotation;
failure:
    OKGR_RotateBuf_Free(rotation);
    fp_leave(saved);
    return NULL;
}
