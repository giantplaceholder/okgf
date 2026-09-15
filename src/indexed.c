#include "okgf.h"
#include "okgf_internal.h"
#include <stddef.h>

_Static_assert(sizeof(OkgfIndexedAlphaEntry) == 4, "Indexed alpha palette entries must be 4 bytes");

/* Add the rounded six-bit contributions before reducing red and blue to five bits. PixelAlpha
 * uses a different truncation rule. */
static uint16_t blend565(uint16_t source, uint16_t dest, unsigned front, unsigned back) {
    unsigned red = okgf_scale63_rounded((source >> 10) & 62, front) +
                   okgf_scale63_rounded((dest >> 10) & 62, back);
    unsigned green = okgf_scale63_rounded((source >> 5) & 63, front) +
                     okgf_scale63_rounded((dest >> 5) & 63, back);
    unsigned blue = okgf_scale63_rounded((source & 31) * 2, front) +
                    okgf_scale63_rounded((dest & 31) * 2, back);
    return (uint16_t)(((red << 10) & 0xF800) | (green << 5) | (blue >> 1));
}

static uint32_t expand_bgra(uint16_t color, unsigned alpha, int unpremultiply) {
    unsigned red = (color >> 8) & 248;
    unsigned green = (color >> 3) & 252;
    unsigned blue = (color & 31) << 3;
    if (unpremultiply && alpha) {
        red = red * 255 / alpha;
        green = green * 255 / alpha;
        blue = blue * 255 / alpha;
    }
    return (uint32_t)(uint8_t)blue | (uint32_t)(uint8_t)green << 8 | (uint32_t)(uint8_t)red << 16 |
           (uint32_t)(uint8_t)alpha << 24;
}

enum IndexedMode { COPY_WORD, COPY_ALPHA, COPY_BGRA, PREMULT_WORD, PREMULT_ALPHA, PREMULT_BGRA };

typedef struct IndexedPalette {
    const uint8_t *entries;
    enum IndexedMode mode;
    unsigned opacity;
    uint32_t bgra[256];
} IndexedPalette;

static void literal_run(uint8_t *dest, const uint8_t *indices, int count,
                        const IndexedPalette *palette) {
    for (int i = 0; i < count; ++i) {
        unsigned index = indices[i];
        if (palette->mode == COPY_BGRA || palette->mode == PREMULT_BGRA) {
            okgf_store32(dest, palette->bgra[index]);
            dest += 4;
            continue;
        }
        if (palette->mode == COPY_WORD || palette->mode == COPY_ALPHA) {
            uint16_t color = okgf_load16(palette->entries + 2 * index);
            if (palette->mode == COPY_ALPHA)
                color = blend565(color, okgf_load16(dest), palette->opacity, 63 - palette->opacity);
            okgf_store16(dest, color);
        } else {
            uint32_t entry = okgf_load32(palette->entries + 4 * index);
            unsigned inverse_alpha = entry >> 16;
            uint16_t old = okgf_load16(dest), color;
            if (palette->mode == PREMULT_ALPHA) {
                unsigned back = 63 - okgf_scale63_rounded(63 - inverse_alpha, palette->opacity);
                color = blend565((uint16_t)entry, old, palette->opacity, back);
            } else {
                unsigned red = okgf_scale63_rounded((old >> 10) & 62, inverse_alpha) & 62;
                unsigned green = okgf_scale63_rounded((old >> 5) & 63, inverse_alpha);
                unsigned blue = okgf_scale63_rounded((old & 31) * 2, inverse_alpha) >> 1;
                /* Native WORD addition can carry across color components. */
                color = (uint16_t)(entry + ((red << 10) | (green << 5) | blue));
            }
            okgf_store16(dest, color);
        }
        dest += 2;
    }
}

static void draw(void *dest, int32_t pitch, const OkgfRleHeader *source, int32_t x, int32_t y,
                 const OkgfRect *clip, enum IndexedMode mode, uint8_t alpha) {
    const uint8_t *header = (const uint8_t *)source;
    int32_t width = (int32_t)okgf_load32(header + 4), height = (int32_t)okgf_load32(header + 8);
    unsigned palette_count = header[12] ? header[12] : 256;
    int premultiplied = mode >= PREMULT_WORD;
    int output_bpp = mode == COPY_BGRA || mode == PREMULT_BGRA ? 4 : 2;
    IndexedPalette palette = {.entries = header + 16, .mode = mode, .opacity = alpha >> 2};
    const uint8_t *commands = palette.entries + (premultiplied ? 4 : 2) * palette_count;
    if (output_bpp == 4) {
        /* The converted palette has a fixed maximum size and fits on the stack. */
        for (unsigned i = 0; i < palette_count; ++i) {
            uint32_t entry = premultiplied ? okgf_load32(palette.entries + 4 * i)
                                           : okgf_load16(palette.entries + 2 * i);
            unsigned opacity = premultiplied ? 252 - 4 * (entry >> 16) : 255;
            palette.bgra[i] = expand_bgra((uint16_t)entry, opacity, premultiplied);
        }
    }
    if (!clip || (x >= clip->left && (int64_t)x + width <= clip->right && y >= clip->top &&
                  (int64_t)y + height <= clip->bottom)) {
        int32_t remaining = (int32_t)okgf_load32(header);
        ptrdiff_t offset = (ptrdiff_t)y * pitch + (ptrdiff_t)x * output_bpp;
        while (remaining > 0) {
            unsigned command = *commands++;
            --remaining;
            if (!command)
                offset += pitch - (ptrdiff_t)width * output_bpp;
            else if (command == 128)
                offset += pitch;
            else if (command < 128)
                offset += (ptrdiff_t)command * output_bpp;
            else {
                int count = command & 127;
                literal_run((uint8_t *)dest + offset, commands, count, &palette);
                commands += count;
                remaining -= count;
                offset += (ptrdiff_t)count * output_bpp;
            }
        }
        return;
    }
    if (x > clip->right || (int64_t)x + width <= clip->left || y > clip->bottom ||
        (int64_t)y + height <= clip->top)
        return;
    /* Inclusive right/bottom edges, as in the game's other sprite decoders. */
    for (int32_t row = 0; row < height; ++row) {
        int64_t py = (int64_t)y + row, px = x;
        if (py > clip->bottom)
            break;
        for (;;) {
            unsigned command = *commands++;
            if (!command || command == 128)
                break;
            int count = command & 127;
            if (command & 128) {
                if (py >= clip->top) {
                    int lo = px < clip->left ? (int)((int64_t)clip->left - px) : 0;
                    int hi =
                        px + count - 1 > clip->right ? (int)((int64_t)clip->right - px + 1) : count;
                    if (lo < hi)
                        literal_run((uint8_t *)dest + (ptrdiff_t)py * pitch +
                                        output_bpp * (ptrdiff_t)(px + lo),
                                    commands + lo, hi - lo, &palette);
                }
                commands += count;
            }
            px += count;
        }
    }
}

void OKGF_CALL OKGR_AlphaIndexed_CopyDraw_WORD(OKGF_RLE_ARGS) {
    draw(dest, pitch, source, 0, 0, NULL, COPY_WORD, 255);
}
void OKGF_CALL OKGR_AlphaIndexed_CopyDraw_Alpha_16(OKGF_RLE_ARGS, uint8_t alpha) {
    draw(dest, pitch, source, 0, 0, NULL, COPY_ALPHA, alpha);
}
void OKGF_CALL OKGR_AlphaIndexed_Draw_RGBA(OKGF_RLE_ARGS) {
    draw(dest, pitch, source, 0, 0, NULL, COPY_BGRA, 255);
}
void OKGF_CALL OKGR_AlphaIndexed_AlphaDraw_16(OKGF_RLE_ARGS) {
    draw(dest, pitch, source, 0, 0, NULL, PREMULT_WORD, 255);
}
void OKGF_CALL OKGR_AlphaIndexed_AlphaDraw_Alpha_16(OKGF_RLE_ARGS, uint8_t alpha) {
    draw(dest, pitch, source, 0, 0, NULL, PREMULT_ALPHA, alpha);
}
void OKGF_CALL OKGR_AlphaIndexed_AlphaDraw_RGBA(OKGF_RLE_ARGS) {
    draw(dest, pitch, source, 0, 0, NULL, PREMULT_BGRA, 255);
}
void OKGF_CALL OKGR_AlphaIndexed_CopyDrawClip_WORD(OKGF_RLE_CLIP_ARGS) {
    draw(dest, pitch, source, x, y, clip, COPY_WORD, 255);
}
void OKGF_CALL OKGR_AlphaIndexed_CopyDrawClip_Alpha_16(OKGF_RLE_CLIP_ARGS, uint8_t alpha) {
    draw(dest, pitch, source, x, y, clip, COPY_ALPHA, alpha);
}
void OKGF_CALL OKGR_AlphaIndexed_AlphaDrawClip_16(OKGF_RLE_CLIP_ARGS) {
    draw(dest, pitch, source, x, y, clip, PREMULT_WORD, 255);
}
void OKGF_CALL OKGR_AlphaIndexed_AlphaDrawClip_Alpha_16(OKGF_RLE_CLIP_ARGS, uint8_t alpha) {
    draw(dest, pitch, source, x, y, clip, PREMULT_ALPHA, alpha);
}
