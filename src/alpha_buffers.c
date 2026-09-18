#include "okgf.h"
#include "okgf_internal.h"
#include <stddef.h>

static void alpha_rect(OKGF_RECT_ARGS, const void *palette, int is555) {
    int bpp = palette ? 1 : 4;
    const uint8_t *src =
        (const uint8_t *)source + (ptrdiff_t)source_y * source_pitch + (ptrdiff_t)source_x * bpp;
    uint8_t *dst = (uint8_t *)dest + (ptrdiff_t)dest_y * dest_pitch + 2 * (ptrdiff_t)dest_x;
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const uint8_t *p = palette ? (const uint8_t *)palette + 4 * src[x] : src + 4 * x;
            unsigned a = p[3] >> 2;
            if (!a)
                continue;
            unsigned r = p[palette ? 0 : 2] >> 3, b = p[palette ? 2 : 0] >> 3,
                     g = p[1] >> (is555 ? 3 : 2);
            uint16_t color = (uint16_t)(r << (is555 ? 10 : 11) | g << 5 | b);
            if (a >= 62)
                okgf_store16(dst + 2 * x, color);
            else if (is555)
                OKGR_PixelAlpha_15(dst + 2 * x, color, p[3]);
            else
                OKGR_PixelAlpha_16(dst + 2 * x, color, p[3]);
        }
        dst += dest_pitch;
        src += source_pitch;
    }
}
#define RECT_PASS                                                                                  \
    dest, dest_pitch, dest_x, dest_y, source, source_pitch, source_x, source_y, width, height
void OKGF_CALL OKGR_AlphaSimpleBuf_Draw_16(OKGF_RECT_ARGS) {
    alpha_rect(RECT_PASS, NULL, 0);
}
void OKGF_CALL OKGR_AlphaSimpleBufPalAlpha_Draw_16(OKGF_RECT_ARGS, const void *palette) {
    alpha_rect(RECT_PASS, palette, 0);
}

/* Use alpha tables generated at 53-bit startup precision. Their truncation differs between the
 * lower and upper halves of the alpha range. Only bucket 63 bypasses the tables here. */
void OKGF_CALL OKGR_PixelAlpha_16(void *pixel, uint16_t color, uint8_t alpha) {
    uint8_t *p = pixel;
    unsigned a = alpha >> 2;
    if (a == 63) {
        okgf_store16(p, color);
        return;
    }
    okgf_store16(p, okgf_blend565_truncated(color, okgf_load16(p), a));
}

void OKGF_CALL OKGR_AlphaSimpleBuf_Draw_15(OKGF_RECT_ARGS) {
    alpha_rect(RECT_PASS, NULL, 1);
}
void OKGF_CALL OKGR_AlphaSimpleBufPalAlpha_Draw_15(OKGF_RECT_ARGS, const void *palette) {
    alpha_rect(RECT_PASS, palette, 1);
}
void OKGF_CALL OKGR_PixelAlpha_15(void *pixel, uint16_t color, uint8_t alpha) {
    unsigned a = alpha >> 2;
    okgf_store16(pixel, a == 63 ? color : okgf_blend555_truncated(color, okgf_load16(pixel), a));
}
