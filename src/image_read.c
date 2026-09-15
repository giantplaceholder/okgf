#include "okgf.h"
#include "okgf_internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Original: 0x1005EF10.
 * Identify formats from their signatures. Preserve the original PSD offset and permissive PNG
 * check; this function does not validate the full image. */
OkgfImageKind OKGF_CALL OKGF_Tip(const void *source, int32_t source_size) {
    const uint8_t *s = source;
    if (source_size < 34)
        return OKGF_IMAGE_UNKNOWN;
    if (s[0] == 'B' && s[1] == 'M' && okgf_load32(s + 14) == 40)
        return okgf_load32(s + 28) < 24 ? OKGF_IMAGE_INDEXED_BMP : OKGF_IMAGE_BMP;
    if (okgf_load32(s) == UINT32_C(0xE0FFD8FF) && okgf_load32(s + 4) == UINT32_C(0x464A1000) &&
        s[8] == 'I' && s[9] == 'F')
        return OKGF_IMAGE_JPEG;
    if (okgf_load32(s) == UINT32_C(0x474E5089))
        return OKGF_IMAGE_PNG;
    if (okgf_load32(s) == UINT32_C(0x53504238)) {
        switch ((unsigned)s[12] * 256 + s[13]) {
        case 1:
            return OKGF_IMAGE_GRAYSCALE_PSD;
        case 2:
            return OKGF_IMAGE_INDEXED_PSD;
        case 3:
            return OKGF_IMAGE_RGB_PSD;
        case 4:
            return OKGF_IMAGE_CMYK_PSD;
        }
    }
    return OKGF_IMAGE_UNKNOWN;
}

static void output32(int32_t *output, int32_t value) {
    memcpy(output, &value, 4);
}
static OkgfReadContext *allocate_context(const void *source, int32_t size) {
    OkgfReadContext *context = calloc(1, sizeof(*context));
    if (context) {
        context->source_data = source;
        context->source_size = size;
        context->image_kind = OKGF_Tip(source, size);
    }
    return context;
}
static OkgfReadContext *finish_begin(OkgfReadContext *context, int32_t *width, int32_t *height) {
    if (!context->codec_context) {
        free(context);
        return NULL;
    }
    memcpy(&context->width, width, 4);
    memcpy(&context->height, height, 4);
    return context;
}
OkgfReadContext *OKGF_CALL OKGF_ReadStart_Buf(const void *source, int32_t source_size,
                                              int32_t *width, int32_t *height) {
    OkgfReadContext *context = allocate_context(source, source_size);
    if (!context)
        return NULL;
    switch (context->image_kind) {
    case OKGF_IMAGE_BMP:
        context->codec_context = OKGR_ReadStart_BMP_Buf(source, source_size, width, height);
        break;
    case OKGF_IMAGE_INDEXED_BMP:
        context->codec_context =
            OKGR_ReadStart_BMPPAL_Buf(source, source_size, width, height, &context->palette_count);
        break;
    case OKGF_IMAGE_JPEG:
        context->codec_context = OKGR_ReadStart_JPEG_Buf(source, source_size, width, height);
        break;
    case OKGF_IMAGE_PNG:
        context->codec_context = OKGR_ReadStart_PNG_Buf(source, source_size, width, height);
        break;
    case OKGF_IMAGE_RGB_PSD:
    case OKGF_IMAGE_CMYK_PSD:
        context->codec_context = OKGR_ReadStart_PSD_Buf(source, source_size, width, height);
        break;
    case OKGF_IMAGE_INDEXED_PSD:
    case OKGF_IMAGE_GRAYSCALE_PSD:
        context->codec_context =
            OKGR_ReadStart_PSDPAL_Buf(source, source_size, width, height, &context->palette_count);
        break;
    default:
        break;
    }
    return finish_begin(context, width, height);
}
OkgfReadContext *OKGF_CALL OKGF_ReadStartPal_Buf(const void *source, int32_t source_size,
                                                 int32_t *width, int32_t *height,
                                                 int32_t *palette_count, int32_t *bytes_per_pixel) {
    OkgfReadContext *context = allocate_context(source, source_size);
    if (!context)
        return NULL;
    if (context->image_kind == OKGF_IMAGE_PNG) {
        context->codec_context =
            OKGR_ReadStart_PNGPAL_Buf(source, source_size, width, height, palette_count);
        output32(bytes_per_pixel, 1);
    } else if (context->image_kind == OKGF_IMAGE_GRAYSCALE_PSD ||
               context->image_kind == OKGF_IMAGE_INDEXED_PSD) {
        output32(bytes_per_pixel, context->image_kind == OKGF_IMAGE_INDEXED_PSD ? 2 : 1);
        context->codec_context =
            OKGR_ReadStart_PSDPAL_Buf(source, source_size, width, height, palette_count);
    }
    return finish_begin(context, width, height);
}
static int32_t finish_read(OkgfReadContext *context, int32_t success) {
    if (success) {
        if (context->owns_source)
            free((void *)context->source_data);
        free(context);
    }
    return success;
}
int32_t OKGF_CALL OKGF_ReadPal(OkgfReadContext *context, void *pixels, int32_t pitch_bytes,
                               void *palette_rgba) {
    int32_t success;
    if (context->image_kind == OKGF_IMAGE_PNG)
        success = OKGF_Read_PNGPAL(context->codec_context, pixels, pitch_bytes, palette_rgba);
    else if (context->image_kind == OKGF_IMAGE_GRAYSCALE_PSD ||
             context->image_kind == OKGF_IMAGE_INDEXED_PSD)
        success = OKGF_Read_PSDPAL(context->codec_context, pixels, pitch_bytes, palette_rgba);
    else
        return 0;
    return finish_read(context, success);
}

int32_t OKGF_CALL OKGF_Read(OkgfReadContext *context, void *pixels, int32_t pitch_bytes,
                            uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask,
                            uint32_t alpha_mask, int32_t bytes_per_pixel) {
    int32_t width = context->width, height = context->height;
    if (width <= 0 || height <= 0 || bytes_per_pixel < 1 || bytes_per_pixel > 4 ||
        (uint64_t)(uint32_t)pitch_bytes < (uint64_t)(uint32_t)width * (uint32_t)bytes_per_pixel)
        return 0;
    OkgfImageKind kind = context->image_kind;
    if (kind == OKGF_IMAGE_PNG)
        return finish_read(context,
                           OKGF_Read_PNG(context->codec_context, pixels, pitch_bytes, red_mask,
                                         green_mask, blue_mask, alpha_mask, bytes_per_pixel));
    int direct_rgb =
        red_mask == 255 && green_mask == 65280 && blue_mask == 16711680 && bytes_per_pixel == 3;
    if (kind == OKGF_IMAGE_BMP && direct_rgb)
        return finish_read(context, OKGF_Read_BMP(context->codec_context, pixels, pitch_bytes));
    if ((kind == OKGF_IMAGE_RGB_PSD && direct_rgb) ||
        (kind == OKGF_IMAGE_CMYK_PSD && direct_rgb && alpha_mask == 0xff000000))
        return finish_read(context, OKGF_Read_PSD(context->codec_context, pixels, pitch_bytes));
    if (kind == OKGF_IMAGE_JPEG && ((OkgfJpegReadContext *)context->codec_context)->channels != 3)
        return 0; /* The outer reader expects RGB JPEG output. */
    int indexed = kind == OKGF_IMAGE_INDEXED_BMP || kind == OKGF_IMAGE_GRAYSCALE_PSD ||
                  kind == OKGF_IMAGE_INDEXED_PSD;
    int channels = indexed ? 1 : kind == OKGF_IMAGE_CMYK_PSD ? 4 : 3;
    if ((size_t)width > SIZE_MAX / (size_t)height / (unsigned)channels ||
        width > INT32_MAX / channels)
        return 0;
    uint8_t *decoded = malloc((size_t)width * height * channels);
    /* PSD palettes need 1024 bytes; the original 768-byte allocation was too small. */
    uint8_t palette[1024];
    if (!decoded)
        return 0;
    int32_t success = 0;
    switch (kind) {
    case OKGF_IMAGE_BMP:
        success = OKGF_Read_BMP(context->codec_context, decoded, width * channels);
        break;
    case OKGF_IMAGE_INDEXED_BMP:
        success = OKGF_Read_BMPPAL(context->codec_context, decoded, width, palette);
        break;
    case OKGF_IMAGE_JPEG:
        success = OKGF_Read_JPEG(context->codec_context, decoded, width * channels);
        break;
    case OKGF_IMAGE_RGB_PSD:
    case OKGF_IMAGE_CMYK_PSD:
        success = OKGF_Read_PSD(context->codec_context, decoded, width * channels);
        break;
    case OKGF_IMAGE_GRAYSCALE_PSD:
        success = OKGF_Read_PSDPAL(context->codec_context, decoded, width, palette);
        break;
    case OKGF_IMAGE_INDEXED_PSD:
        success = OKGF_Read_PSDPAL(context->codec_context, pixels, pitch_bytes, palette);
        free(decoded);
        return finish_read(context, success);
    default:
        break;
    }
    if (!success) {
        free(decoded);
        return 0;
    }
    const uint32_t masks[4] = {red_mask, green_mask, blue_mask, alpha_mask};
    unsigned left[4], right[4];
    for (int channel = 0; channel < 4; ++channel)
        okgf_mask_shifts(masks[channel], &left[channel], &right[channel]);
    for (int32_t y = 0; y < height; ++y) {
        uint8_t *dest = (uint8_t *)pixels + (ptrdiff_t)y * pitch_bytes;
        for (int32_t x = 0; x < width; ++x) {
            const uint8_t *rgb = decoded + ((size_t)y * width + x) * channels;
            if (indexed)
                rgb = palette + 3 * *rgb;
            uint32_t value = 0;
            for (int channel = 0; channel < (channels == 4 ? 4 : 3); ++channel)
                value |= ((uint32_t)rgb[channel] >> right[channel]) << left[channel];
            for (int byte = 0; byte < bytes_per_pixel; ++byte)
                *dest++ = (uint8_t)(value >> (8 * byte));
        }
    }
    free(decoded);
    return finish_read(context, 1);
}
