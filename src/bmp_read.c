#include "okgf.h"
#include "okgf_internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void output32(int32_t *dest, uint32_t value) {
    memcpy(dest, &value, 4);
}

static OkgfBmpReadContext *begin(const uint8_t *source, int32_t source_size, int32_t *width,
                                 int32_t *height, int32_t *palette_count) {
    if (source_size < 54 || !source || okgf_load16(source) != 0x4d42 ||
        okgf_load32(source + 14) != 40)
        return NULL;
    unsigned depth = okgf_load16(source + 28);
    int32_t count = 0;
    if (palette_count) {
        if (depth > 8)
            return NULL;
        uint32_t stored = okgf_load32(source + 46);
        memcpy(&count, &stored, 4);
        if (!count) {
            if (depth != 1 && depth != 4 && depth != 8)
                return NULL;
            count = 1 << depth;
        }
        if (count < 0)
            return NULL;
    } else if (depth < 24) {
        return NULL;
    }
    uint32_t offset = okgf_load32(source + 10);
    if (offset > (uint32_t)source_size)
        return NULL;
    OkgfBmpReadContext *context = calloc(1, sizeof(*context));
    if (!context)
        return NULL;
    context->header = source;
    context->pixels = source + offset;
    context->palette_count = count;
    context->source_data = source;
    context->source_size = source_size;
    output32(width, okgf_load32(source + 18));
    output32(height, okgf_load32(source + 22));
    if (palette_count)
        output32(palette_count, (uint32_t)count);
    return context;
}

OkgfBmpReadContext *OKGF_CALL OKGR_ReadStart_BMP_Buf(const uint8_t *source, int32_t source_size,
                                                     int32_t *width, int32_t *height) {
    return begin(source, source_size, width, height, NULL);
}
OkgfBmpReadContext *OKGF_CALL OKGR_ReadStart_BMPPAL_Buf(const uint8_t *source, int32_t source_size,
                                                        int32_t *width, int32_t *height,
                                                        int32_t *palette_count) {
    return begin(source, source_size, width, height, palette_count);
}

void OKGF_CALL okgf_cancel_read_bmp(OkgfBmpReadContext *context) {
    if (!context)
        return;
    if (context->owns_source)
        free((void *)context->source_data);
    free(context);
}

static int32_t decode(OkgfBmpReadContext *context, void *pixels, int32_t pitch, uint8_t *palette) {
    const uint8_t *header = context->header;
    int64_t width = (int32_t)okgf_load32(header + 18), height = (int32_t)okgf_load32(header + 22);
    if (width <= 0 || height <= 0)
        return 0;
    if (palette && (okgf_load16(header + 28) != 8 || okgf_load32(header + 30)))
        return 0;
    int64_t row_bytes = width * (palette ? 1 : 3);
    int64_t stride = row_bytes & 2 ? (row_bytes & ~INT64_C(3)) + 4 : row_bytes;
    int64_t available = context->source_size - (int64_t)okgf_load32(header + 10);
    if (available < 0 || stride > available / height)
        return 0;
    if (palette) {
        /* Begin also accepts headers without pixel data. Validate the palette and rows before
         * decoding, including counts above 256 that the original DLL accepts. */
        if (context->palette_count <= 0 ||
            (int64_t)context->palette_count * 4 > (int64_t)context->source_size - 54)
            return 0;
        const uint8_t *entry = context->source_data + 54;
        for (int32_t i = 0; i < context->palette_count; ++i, entry += 4, palette += 3) {
            palette[0] = entry[2];
            palette[1] = entry[1];
            palette[2] = entry[0];
        }
    }
    const uint8_t *source = context->pixels + (ptrdiff_t)(stride * (height - 1));
    uint8_t *dest = pixels;
    for (int64_t y = 0; y < height; ++y) {
        if (palette) {
            /* Copy each DWORD before advancing, then copy any trailing bytes. */
            int64_t x = 0;
            for (; x + 4 <= width; x += 4) {
                uint8_t group[4];
                memcpy(group, source + x, 4);
                memcpy(dest + x, group, 4);
            }
            for (; x < width; ++x)
                dest[x] = source[x];
        } else {
            for (int64_t x = 0; x < width; ++x) {
                dest[3 * x] = source[3 * x + 2];
                dest[3 * x + 1] = source[3 * x + 1];
                dest[3 * x + 2] = source[3 * x];
            }
        }
        source -= stride;
        dest += pitch;
    }
    okgf_cancel_read_bmp(context);
    return 1;
}
int32_t OKGF_CALL OKGF_Read_BMP(OkgfBmpReadContext *context, void *pixels, int32_t pitch_bytes) {
    return decode(context, pixels, pitch_bytes, NULL);
}
int32_t OKGF_CALL OKGF_Read_BMPPAL(OkgfBmpReadContext *context, void *pixels, int32_t pitch_bytes,
                                   void *palette_rgb) {
    return decode(context, pixels, pitch_bytes, palette_rgb);
}
