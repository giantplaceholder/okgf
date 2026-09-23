#include "okgf.h"
#include "okgf_internal.h"
#include <png.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void read_source(png_structp png, png_bytep dest, png_size_t count) {
    OkgfPngReadContext *context = png_get_io_ptr(png);
    if (context->source_cursor < 0 || context->source_cursor > context->source_size ||
        count > (size_t)(context->source_size - context->source_cursor))
        png_error(png, "Read Error");
    memcpy(dest, context->source_data + context->source_cursor, count);
    context->source_cursor += (int32_t)count;
}

static void ignore_warning(png_structp png, png_const_charp message) {
    (void)png;
    (void)message;
}

void OKGF_CALL okgf_cancel_read_png(OkgfPngReadContext *context) {
    if (!context)
        return;
    png_structp png = context->png;
    png_infop info = context->info;
    png_destroy_read_struct(&png, &info, NULL);
    if (context->owns_source)
        free((void *)context->source_data);
    free(context);
}

/* Keep allocation and cleanup outside libpng's longjmp recovery frame. */
static int start_decoder(OkgfPngReadContext *context, int32_t *width, int32_t *height,
                         int32_t *palette_count) {
    png_structp png = context->png;
    png_infop info = context->info;
    if (setjmp(png_jmpbuf(png)))
        return 0;
    png_set_read_fn(png, context, read_source);
    png_set_sig_bytes(png, 0);
    png_read_info(png, info);
    png_set_strip_16(png);
    png_set_packing(png);
    if (!palette_count)
        png_set_expand(png);
    png_read_update_info(png, info);
    if (palette_count) {
        int color_type = png_get_color_type(png, info);
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_colorp palette;
            int count;
            png_get_PLTE(png, info, &palette, &count);
            context->palette_rgb = (const uint8_t *)palette;
            context->palette_count = count;
        } else if (color_type == PNG_COLOR_TYPE_GRAY) {
            context->palette_count = 256;
        } else {
            return 0;
        }
        if (png_get_channels(png, info) != 1) {
            return 0;
        }
    }
    context->width = (int32_t)png_get_image_width(png, info);
    context->height = (int32_t)png_get_image_height(png, info);
    memcpy(width, &context->width, 4);
    memcpy(height, &context->height, 4);
    if (palette_count)
        memcpy(palette_count, &context->palette_count, 4);
    return 1;
}

static OkgfPngReadContext *begin(const uint8_t *source, int32_t source_size, int32_t *width,
                                 int32_t *height, int32_t *palette_count) {
    if (!source || source_size < 0)
        return NULL;
    OkgfPngReadContext *context = calloc(1, sizeof(*context));
    if (!context)
        return NULL;
    context->source_data = source;
    context->source_size = source_size;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, ignore_warning);
    context->png = png;
    png_infop info = png ? png_create_info_struct(png) : NULL;
    context->info = info;
    if (!info) {
        okgf_cancel_read_png(context);
        return NULL;
    }
    if (!start_decoder(context, width, height, palette_count)) {
        okgf_cancel_read_png(context);
        return NULL;
    }
    return context;
}

OkgfPngReadContext *OKGF_CALL OKGR_ReadStart_PNG_Buf(const uint8_t *source, int32_t source_size,
                                                     int32_t *width, int32_t *height) {
    return begin(source, source_size, width, height, NULL);
}
OkgfPngReadContext *OKGF_CALL OKGR_ReadStart_PNGPAL_Buf(const uint8_t *source, int32_t source_size,
                                                        int32_t *width, int32_t *height,
                                                        int32_t *palette_count) {
    return begin(source, source_size, width, height, palette_count);
}

/* Use the first contiguous mask run. Shift counts wrap modulo 32, including for runs wider
 * than eight bits. Leave the result unmasked. */

static int32_t decode(OkgfPngReadContext *context, void *pixels, int32_t pitch,
                      const uint32_t masks[4], int32_t bytes_per_pixel, uint8_t *palette) {
    if (!palette && !masks)
        return 0;
    png_structp png = context->png;
    png_infop info = context->info;
    if (palette) {
        for (int32_t i = 0; i < context->palette_count; ++i) {
            for (int channel = 0; channel < 3; ++channel)
                palette[4 * i + channel] =
                    context->palette_rgb ? context->palette_rgb[3 * i + channel] : (uint8_t)i;
            palette[4 * i + 3] = 0;
        }
    }
    size_t stride = png_get_rowbytes(png, info);
    if (context->height <= 0 || stride > SIZE_MAX / (size_t)context->height ||
        (size_t)context->height > SIZE_MAX / sizeof(png_bytep))
        return 0;
    png_bytep *rows = malloc((size_t)context->height * sizeof(*rows));
    uint8_t *data = malloc(stride * (size_t)context->height);
    if (!rows || !data) {
        free(rows);
        free(data);
        return 0;
    }
    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        free(data);
        return 0;
    }
    for (int32_t y = 0; y < context->height; ++y)
        rows[y] = data + (size_t)y * stride;
    png_read_image(png, rows);
    png_read_end(png, info);
    int color_type = png_get_color_type(png, info);
    if (png_get_bit_depth(png, info) != 8 ||
        (!palette && color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGBA)) {
        free(rows);
        free(data);
        return 0;
    }
    unsigned left[4], right[4];
    if (!palette)
        for (int channel = 0; channel < 4; ++channel)
            okgf_mask_shifts(masks[channel], &left[channel], &right[channel]);
    for (int32_t y = 0; y < context->height; ++y) {
        uint8_t *dest = (uint8_t *)pixels + (ptrdiff_t)y * pitch;
        const uint8_t *source = rows[y];
        if (palette) {
            memcpy(dest, source, (size_t)context->width);
        } else {
            for (int32_t x = 0; x < context->width; ++x) {
                uint32_t value = 0;
                for (int channel = 0; channel < 4; ++channel) {
                    uint32_t sample =
                        channel == 3 && color_type == PNG_COLOR_TYPE_RGB ? 0 : *source++;
                    value |= (sample >> right[channel]) << left[channel];
                }
                if (bytes_per_pixel >= 1 && bytes_per_pixel <= 4)
                    for (int byte = 0; byte < bytes_per_pixel; ++byte)
                        *dest++ = (uint8_t)(value >> (byte * 8));
            }
        }
    }
    free(data);
    free(rows);
    okgf_cancel_read_png(context);
    return 1;
}

int32_t OKGF_CALL OKGF_Read_PNG(OkgfPngReadContext *context, void *pixels, int32_t pitch_bytes,
                                uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask,
                                uint32_t alpha_mask, int32_t bytes_per_pixel) {
    const uint32_t masks[4] = {red_mask, green_mask, blue_mask, alpha_mask};
    return decode(context, pixels, pitch_bytes, masks, bytes_per_pixel, NULL);
}
int32_t OKGF_CALL OKGF_Read_PNGPAL(OkgfPngReadContext *context, void *pixels, int32_t pitch_bytes,
                                   void *palette_rgba) {
    return decode(context, pixels, pitch_bytes, NULL, 1, palette_rgba);
}
