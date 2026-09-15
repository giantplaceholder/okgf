#include "okgf.h"
#include <stddef.h>
#include <string.h>

static void store(uint8_t *p, uint32_t color, int bytes) {
    for (int i = 0; i < bytes; ++i)
        p[i] = (uint8_t)(color >> (8 * i));
}

static void span(OKGF_SPAN_ARGS, uint16_t color, const OkgfRect *clip, int bytes) {
    if (!length)
        return;
    OkgfRect bounds;
    memcpy(&bounds, clip, sizeof(bounds));
    int64_t first = x, count = length;
    if (count < 0) {
        first += count + 1;
        count = -count;
    }
    int64_t last = first + count - 1;
    if (y < bounds.top || y > bounds.bottom || first > bounds.right || last < bounds.left)
        return;
    if (first < bounds.left) {
        count -= bounds.left - first;
        first = bounds.left;
        /* For WORD spans, trimming the right edge is an else branch of the left-edge check. */
        if (bytes == 1 && first + count - 1 > bounds.right)
            count = (int64_t)bounds.right - first + 1;
    } else if (last > bounds.right) {
        count = (int64_t)bounds.right - first + 1;
    }
    if (count <= 0)
        return;
    uint8_t *p = (uint8_t *)pixels + (ptrdiff_t)y * pitch + (ptrdiff_t)first * bytes;
    for (int64_t i = 0; i < count; ++i, p += bytes)
        store(p, color, bytes);
}

void OKGF_CALL OKGR_GLine_DrawClip_BYTE(OKGF_SPAN_ARGS, uint8_t color, const OkgfRect *clip) {
    span(pixels, pitch, x, y, length, color, clip, 1);
}
void OKGF_CALL OKGR_GLine_DrawClip_WORD(OKGF_SPAN_ARGS, uint16_t color, const OkgfRect *clip) {
    span(pixels, pitch, x, y, length, color, clip, 2);
}

static void plot(void *pixels, int32_t pitch, int64_t x, int64_t y, uint16_t color,
                 const OkgfRect *clip, int bytes) {
    /* Outline clipping compares coordinates as unsigned values. */
    if ((uint32_t)x >= (uint32_t)clip->left && (uint32_t)x <= (uint32_t)clip->right &&
        (uint32_t)y >= (uint32_t)clip->top && (uint32_t)y <= (uint32_t)clip->bottom)
        store((uint8_t *)pixels + (ptrdiff_t)y * pitch + (ptrdiff_t)x * bytes, color, bytes);
}

static void outline(OKGF_CIRCLE_ARGS, uint16_t color, const OkgfRect *clip, int bytes) {
    OkgfRect bounds;
    memcpy(&bounds, clip, sizeof(bounds));
    uint32_t y_fixed = ((uint32_t)radius << 16) - 1, x_fixed = 0;
    if (radius <= 0)
        return;
    do {
        /* The original CDQ/SHLD/DIV sequence preserves the low numerator word. Include those bits
         * in addition to x_fixed << 16; they affect pixel coverage. */
        uint32_t high = (x_fixed >> 16) | (x_fixed & 0x80000000u ? 0xffff0000u : 0);
        uint64_t numerator = ((uint64_t)high << 32) | x_fixed;
        y_fixed -= (uint32_t)(numerator / y_fixed);
        int64_t x = x_fixed >> 16, y = y_fixed >> 16;
        plot(pixels, pitch, center_x + x, center_y + y, color, &bounds, bytes);
        plot(pixels, pitch, center_x - x, center_y + y, color, &bounds, bytes);
        plot(pixels, pitch, center_x + x, center_y - y, color, &bounds, bytes);
        plot(pixels, pitch, center_x - x, center_y - y, color, &bounds, bytes);
        plot(pixels, pitch, center_x + y, center_y + x, color, &bounds, bytes);
        plot(pixels, pitch, center_x - y, center_y + x, color, &bounds, bytes);
        plot(pixels, pitch, center_x + y, center_y - x, color, &bounds, bytes);
        plot(pixels, pitch, center_x - y, center_y - x, color, &bounds, bytes);
        x_fixed += 65536;
    } while (x_fixed < y_fixed);
}

static void filled(OKGF_CIRCLE_ARGS, uint16_t color, const OkgfRect *clip, int bytes) {
    int32_t x = 0, y = radius - 1;
    int64_t error = 3 - (int64_t)2 * radius;
    do {
        span(pixels, pitch, center_x - x, center_y + y, 2 * x + 1, color, clip, bytes);
        span(pixels, pitch, center_x - x, center_y - y, 2 * x + 1, color, clip, bytes);
        span(pixels, pitch, center_x - y, center_y + x, 2 * y + 1, color, clip, bytes);
        span(pixels, pitch, center_x - y, center_y - x, 2 * y + 1, color, clip, bytes);
        if (error >= 0) {
            error += (int64_t)4 * (x - y) + 10;
            --y;
        } else {
            error += (int64_t)4 * x + 6;
        }
        ++x;
    } while (x <= y);
}

#define CIRCLE_VARIANT(suffix, type, bytes)                                                        \
    void OKGF_CALL OKGR_Circle_DrawClip_##suffix(OKGF_CIRCLE_ARGS, type color,                     \
                                                 const OkgfRect *clip) {                           \
        outline(pixels, pitch, center_x, center_y, radius, color, clip, bytes);                    \
    }                                                                                              \
    void OKGF_CALL OKGR_Circle_DrawFillClip_##suffix(OKGF_CIRCLE_ARGS, type color,                 \
                                                     const OkgfRect *clip) {                       \
        filled(pixels, pitch, center_x, center_y, radius, color, clip, bytes);                     \
    }
CIRCLE_VARIANT(BYTE, uint8_t, 1)
CIRCLE_VARIANT(WORD, uint16_t, 2)

static void trapezium(OKGF_TRAPEZIUM_ARGS, uint32_t color, const OkgfRect *clip, int mode) {
    OkgfRect bounds;
    memcpy(&bounds, clip, sizeof(bounds));
    if (top_y >= bounds.bottom || bottom_y < bounds.top || top_y > bottom_y)
        return;
    int64_t height = (int64_t)bottom_y - top_y;
    int64_t left_delta = (int64_t)bottom_left - top_left;
    int64_t right_delta = (int64_t)bottom_right - top_right;
    int64_t left_step = height ? left_delta / height : 1;
    int64_t right_step = height ? right_delta / height : 1;
    int64_t left_remainder = height ? left_delta % height : left_delta;
    int64_t right_remainder = height ? right_delta % height : right_delta;
    if (left_remainder < 0)
        left_remainder = -left_remainder;
    if (right_remainder < 0)
        right_remainder = -right_remainder;
    int64_t left_error = 0, right_error = 0, left = top_left, right = top_right;
    int bytes = mode == 0 ? 4 : 2;
    uint16_t contribution =
        (uint16_t)((color >> (mode == 64 ? 2 : 1)) & (mode == 64 ? 0x39e7 : 0x7bef));
    for (int64_t y = top_y; y <= bottom_y && y < bounds.bottom; ++y) {
        if (y >= bounds.top && left < bounds.right && right >= bounds.left) {
            int64_t first = left < bounds.left ? bounds.left : left;
            int64_t last = right >= bounds.right ? (int64_t)bounds.right - 1 : right;
            uint8_t *p = (uint8_t *)pixels + (ptrdiff_t)y * pitch + (ptrdiff_t)first * bytes;
            for (int64_t x = first; x <= last; ++x, p += bytes) {
                uint32_t value = color;
                if (mode) {
                    uint16_t old = (uint16_t)(p[0] | (uint16_t)p[1] << 8);
                    value = mode == 64 ? contribution + old - ((old >> 2) & 0x39e7)
                                       : contribution + ((old >> 1) & 0x7bef);
                }
                store(p, value, bytes);
            }
        }
        left += left_step;
        left_error += left_remainder;
        if (left_error >= height) {
            left_error -= height;
            left += left_delta >= 0 ? 1 : -1;
        }
        right += right_step;
        right_error += right_remainder;
        if (right_error >= height) {
            right_error -= height;
            right += right_delta >= 0 ? 1 : -1;
        }
    }
}

#define TRAPEZIUM_VALUES                                                                           \
    pixels, pitch, top_left, top_right, top_y, bottom_left, bottom_right, bottom_y
void OKGF_CALL OKGR_Alpha64Trapezium_16(OKGF_TRAPEZIUM_ARGS, uint16_t color, const OkgfRect *clip) {
    trapezium(TRAPEZIUM_VALUES, color, clip, 64);
}
void OKGF_CALL OKGR_Alpha128Trapezium_16(OKGF_TRAPEZIUM_ARGS, uint16_t color,
                                         const OkgfRect *clip) {
    trapezium(TRAPEZIUM_VALUES, color, clip, 128);
}
void OKGF_CALL OKGR_FillTrapezium_DWORD(OKGF_TRAPEZIUM_ARGS, uint32_t color, const OkgfRect *clip) {
    trapezium(TRAPEZIUM_VALUES, color, clip, 0);
}
