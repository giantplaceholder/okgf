#include "okgf.h"
#include "okgf_internal.h"
#include <stddef.h>

#define RECT_FORWARD                                                                               \
    dest, dest_pitch, dest_x, dest_y, source, source_pitch, source_x, source_y, width, height

enum CopyMode { COPY_WORD, COPY_BED, COPY_TRANSPARENT, COPY_HALF };

/* Load the complete group before storing so overlapping copies follow the original instruction
 * order. */
static void copy_group(uint8_t *dest, const uint8_t *source, size_t bytes) {
    uint8_t value[8];
    memcpy(value, source, bytes);
    memcpy(dest, value, bytes);
}

typedef struct MmxGroups {
    int32_t prefix, groups, tail;
} MmxGroups;

static MmxGroups mmx_groups(const uint8_t *dest, int32_t width) {
    int32_t prefix = (int32_t)((8 - ((uintptr_t)dest & 7)) & 7) / 2;
    int32_t tail = (int32_t)(((uintptr_t)dest + (uintptr_t)2 * width) >> 1 & 3);
    return (MmxGroups){prefix, width < prefix + tail ? -1 : (width - prefix - tail) / 4, tail};
}

/* Compute prefix and tail counts from the first destination address. Reuse them on later rows
 * even if the pitch changes alignment. Odd addresses can leave a WORD uncopied and shift the
 * row cursors. */
static void mmx_row(uint8_t **dest, const uint8_t **source, MmxGroups parts, int reverse) {
    ptrdiff_t step = reverse ? -2 : 2;
    int32_t first = reverse ? parts.tail : parts.prefix;
    int32_t last = reverse ? parts.prefix : parts.tail;
    for (int32_t i = 0; i < first; ++i) {
        copy_group(*dest, *source, 2);
        *dest += step;
        *source += step;
    }
    if (reverse && parts.groups) {
        *dest -= 6;
        *source -= 6;
    }
    for (int32_t i = 0; i < parts.groups; ++i) {
        copy_group(*dest, *source, 8);
        *dest += 4 * step;
        *source += 4 * step;
    }
    if (reverse && parts.groups) {
        *dest += 6;
        *source += 6;
    }
    for (int32_t i = 0; i < last; ++i) {
        copy_group(*dest, *source, 2);
        *dest += step;
        *source += step;
    }
}

static void copy_rect_mmx(OKGF_RECT_ARGS) {
    if (width <= 0 || height <= 0)
        return;
    const uint8_t *s =
        (const uint8_t *)source + (ptrdiff_t)source_y * source_pitch + (ptrdiff_t)2 * source_x;
    uint8_t *d = (uint8_t *)dest + (ptrdiff_t)dest_y * dest_pitch + (ptrdiff_t)2 * dest_x;
    MmxGroups parts = mmx_groups(d, width);
    if (parts.groups < 0)
        return;
    for (int32_t row = 0; row < height; ++row) {
        mmx_row(&d, &s, parts, 0);
        s += source_pitch - (ptrdiff_t)2 * width;
        d += dest_pitch - (ptrdiff_t)2 * width;
    }
}

static void copy_rect(OKGF_RECT_ARGS, enum CopyMode mode, uint16_t key) {
    if (width <= 0 || height <= 0)
        return;
    const uint8_t *s = (const uint8_t *)source + (ptrdiff_t)source_pitch * source_y + 2 * source_x;
    uint8_t *d = (uint8_t *)dest + (ptrdiff_t)dest_pitch * dest_y + 2 * dest_x;
    for (int32_t y = 0; y < height; ++y) {
        int32_t x = 0;
        if (mode == COPY_WORD || mode == COPY_BED) {
            for (; x <= width - 4; x += 4) {
                /* Original loads: 0x1005BCF9 (Copy), 0x1005BDA6 (CopyBed).
                 * Copy loads both DWORDs before storing; CopyBed writes the first DWORD twice. */
                uint32_t first = okgf_load32(s + (ptrdiff_t)2 * x);
                uint32_t second = mode == COPY_BED ? first : okgf_load32(s + (ptrdiff_t)2 * x + 4);
                okgf_store32(d + (ptrdiff_t)2 * x, first);
                okgf_store32(d + (ptrdiff_t)2 * x + 4, second);
            }
        }
        for (; x < width; ++x) {
            uint16_t v = okgf_load16(s + 2 * x);
            if (mode == COPY_TRANSPARENT && v == key)
                continue;
            if (mode == COPY_HALF)
                v = (uint16_t)(((v & 0xF7DEu) >> 1) + ((okgf_load16(d + 2 * x) & 0xF7DEu) >> 1));
            okgf_store16(d + 2 * x, v);
        }
        s += source_pitch;
        d += dest_pitch;
    }
}
void OKGF_CALL OKGR_Copy_XY_XY_WORD(OKGF_RECT_ARGS) {
    if (width >= 56 && (OKGF_GetCPUFeatures() & 3) == 1) {
        copy_rect_mmx(RECT_FORWARD);
        return;
    }
    copy_rect(RECT_FORWARD, COPY_WORD, 0);
}
void OKGF_CALL OKGR_CopyBed_XY_XY_WORD(OKGF_RECT_ARGS) {
    copy_rect(RECT_FORWARD, COPY_BED, 0);
}
void OKGF_CALL OKGR_CopyTrans_XY_XY_WORD(OKGF_RECT_ARGS, uint16_t transparent_color) {
    copy_rect(RECT_FORWARD, COPY_TRANSPARENT, transparent_color);
}
void OKGF_CALL OKGR_HACopy_XY_XY_16(OKGF_RECT_ARGS) {
    copy_rect(RECT_FORWARD, COPY_HALF, 0);
}

#define PAL_FORWARD                                                                                \
    dest, dest_pitch, dest_x, dest_y, source, source_pitch, source_x, source_y, palette, width,    \
        height
static void pal_copy(OKGF_PAL_ARGS, int bpp, int swap) {
    const uint8_t *s = (const uint8_t *)source + (ptrdiff_t)source_pitch * source_y + source_x;
    /* Both DWORD exports also use 2 * dest_x in the original DLL. */
    uint8_t *d = (uint8_t *)dest + (ptrdiff_t)dest_pitch * dest_y + 2 * dest_x;
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            const uint8_t *p = (const uint8_t *)palette + bpp * s[x];
            if (bpp == 2)
                okgf_store16(d + 2 * x, okgf_load16(p));
            else if (!swap)
                okgf_store32(d + 4 * x, okgf_load32(p));
            else {
                d[4 * x] = p[2];
                d[4 * x + 1] = p[1];
                d[4 * x + 2] = p[0];
                d[4 * x + 3] = p[3];
            }
        }
        s += source_pitch;
        d += dest_pitch;
    }
}
void OKGF_CALL OKGR_PalCopy_XY_XY_WORD(OKGF_PAL_ARGS) {
    pal_copy(PAL_FORWARD, 2, 0);
}
void OKGF_CALL OKGR_PalCopy_XY_XY_DWORD(OKGF_PAL_ARGS) {
    pal_copy(PAL_FORWARD, 4, 0);
}
void OKGF_CALL OKGR_PalCopySwap_XY_XY_DWORD(OKGF_PAL_ARGS) {
    pal_copy(PAL_FORWARD, 4, 1);
}

void OKGF_CALL OKGR_CopySingleBuf_XY_XY_WORD(void *pixels, int32_t pitch, int32_t dest_x,
                                             int32_t dest_y, int32_t source_x, int32_t source_y,
                                             int32_t width, int32_t height) {
    if (width <= 0 || height <= 0)
        return;
    if ((OKGF_GetCPUFeatures() & 3) == 1) {
        OKGR_CopySingleBuf_XY_XY_WORD_MMX(pixels, pitch, dest_x, dest_y, source_x, source_y, width,
                                          height);
        return;
    }
    uint8_t *source = (uint8_t *)pixels + (ptrdiff_t)source_y * pitch + (ptrdiff_t)2 * source_x;
    uint8_t *dest = (uint8_t *)pixels + (ptrdiff_t)dest_y * pitch + (ptrdiff_t)2 * dest_x;
    if ((uint32_t)source_x >= (uint32_t)dest_x && (uint32_t)source_y >= (uint32_t)dest_y) {
        /* Original call: 0x1005BF0A -> 0x1005BC50.
         * Forward copies use the ordinary rectangle-copy grouping. */
        copy_rect(pixels, pitch, dest_x, dest_y, pixels, pitch, source_x, source_y, width, height,
                  COPY_WORD, 0);
    } else if ((uint32_t)source_x > (uint32_t)dest_x) {
        source += (ptrdiff_t)(height - 1) * pitch;
        dest += (ptrdiff_t)(height - 1) * pitch;
        int tail = (width & 1) ^ ((uintptr_t)dest >> 1 & 1);
        int32_t pairs = (width - tail) / 2;
        ptrdiff_t row_skip = (ptrdiff_t)pitch + 2 * (ptrdiff_t)width + 2 * tail;
        for (int32_t row = 0; row < height; ++row) {
            if ((uintptr_t)dest & 3) {
                copy_group(dest, source, 2);
                source += 2;
                dest += 2;
            }
            for (int32_t i = 0; i < pairs; ++i) {
                copy_group(dest, source, 4);
                source += 4;
                dest += 4;
            }
            if (tail)
                copy_group(dest, source, 2);
            /* The tail store leaves both cursors unchanged. With the row skip below, this advances
             * the next row by an extra four bytes when tail == 1. */
            source -= row_skip;
            dest -= row_skip;
        }
    } else {
        source += (ptrdiff_t)2 * (width - 1);
        dest += (ptrdiff_t)2 * (width - 1);
        ptrdiff_t row_skip = (ptrdiff_t)2 * width + pitch;
        if ((uint32_t)source_y < (uint32_t)dest_y) {
            source += (ptrdiff_t)(height - 1) * pitch;
            dest += (ptrdiff_t)(height - 1) * pitch;
            row_skip = (ptrdiff_t)2 * width - pitch;
        }
        int tail = ((~width) & 1) ^ ((uintptr_t)dest >> 1 & 1);
        int32_t pairs = (width - tail) / 2;
        for (int32_t row = 0; row < height; ++row) {
            if (((uintptr_t)dest & 3) == 0) {
                copy_group(dest, source, 2);
                source -= 2;
                dest -= 2;
            }
            source -= 2;
            dest -= 2;
            for (int32_t i = 0; i < pairs; ++i) {
                copy_group(dest, source, 4);
                source -= 4;
                dest -= 4;
            }
            source += 2;
            dest += 2;
            if (tail) {
                copy_group(dest, source, 2);
                source -= 2;
                dest -= 2;
            }
            source += row_skip;
            dest += row_skip;
        }
    }
}

void OKGF_CALL OKGR_CopySingleBuf_XY_XY_WORD_MMX(void *pixels, int32_t pitch, int32_t dest_x,
                                                 int32_t dest_y, int32_t source_x, int32_t source_y,
                                                 int32_t width, int32_t height) {
    if (width <= 0 || height <= 0)
        return;
    const uint8_t *s =
        (const uint8_t *)pixels + (ptrdiff_t)source_y * pitch + (ptrdiff_t)2 * source_x;
    uint8_t *d = (uint8_t *)pixels + (ptrdiff_t)dest_y * pitch + (ptrdiff_t)2 * dest_x;
    MmxGroups parts = mmx_groups(d, width);
    if (parts.groups < 0)
        return;
    int reverse = (uint32_t)source_x < (uint32_t)dest_x;
    int bottom_up = (uint32_t)source_y < (uint32_t)dest_y;
    ptrdiff_t row_skip = bottom_up ? -(ptrdiff_t)pitch : pitch;
    row_skip += (reverse ? 2 : -2) * (ptrdiff_t)width;
    if (bottom_up) {
        s += (ptrdiff_t)(height - 1) * pitch;
        d += (ptrdiff_t)(height - 1) * pitch;
    }
    if (reverse) {
        s += (ptrdiff_t)2 * (width - 1);
        d += (ptrdiff_t)2 * (width - 1);
    }
    for (int32_t row = 0; row < height; ++row) {
        mmx_row(&d, &s, parts, reverse);
        s += row_skip;
        d += row_skip;
    }
}
