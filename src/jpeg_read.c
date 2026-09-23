#include "okgf.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* jpeglib requires FILE and size_t declarations first. */
#include <jpeglib.h>

/* Keep libjpeg's decoder and memory-source structures behind the public context. */
typedef struct JpegState {
    struct jpeg_decompress_struct jpeg;
    struct jpeg_error_mgr error;
    jmp_buf failure;
} JpegState;

static void fail(j_common_ptr jpeg) {
    JpegState *state = (JpegState *)jpeg;
    longjmp(state->failure, 1);
}

static void ignore_message(j_common_ptr jpeg) {
    (void)jpeg;
}

void OKGF_CALL okgf_cancel_read_jpeg(OkgfJpegReadContext *context) {
    if (!context)
        return;
    JpegState *state = context->decoder;
    if (state) {
        jpeg_destroy_decompress(&state->jpeg);
        free(state);
    }
    if (context->owns_source)
        free((void *)context->source_data);
    free(context);
}

/* Keep ownership in the caller, outside libjpeg's longjmp recovery frame. */
static int start_decoder(OkgfJpegReadContext *context, int32_t *width, int32_t *height) {
    JpegState *state = context->decoder;
    state->jpeg.err = jpeg_std_error(&state->error);
    state->error.error_exit = fail;
    state->error.output_message = ignore_message;
    if (setjmp(state->failure))
        return 0;
    jpeg_create_decompress(&state->jpeg);
    jpeg_mem_src(&state->jpeg, context->source_data, (unsigned long)context->source_size);
    jpeg_read_header(&state->jpeg, TRUE);
    jpeg_start_decompress(&state->jpeg);
    context->width = (int32_t)state->jpeg.output_width;
    context->height = (int32_t)state->jpeg.output_height;
    context->channels = state->jpeg.output_components;
    memcpy(width, &context->width, 4);
    memcpy(height, &context->height, 4);
    return 1;
}

OkgfJpegReadContext *OKGF_CALL OKGR_ReadStart_JPEG_Buf(const uint8_t *source, int32_t source_size,
                                                       int32_t *width, int32_t *height) {
    if (!source || source_size <= 0)
        return NULL;
    OkgfJpegReadContext *context = calloc(1, sizeof(*context));
    JpegState *state = calloc(1, sizeof(*state));
    if (!context || !state) {
        free(context);
        free(state);
        return NULL;
    }
    context->decoder = state;
    context->source_data = source;
    context->source_size = source_size;
    if (!start_decoder(context, width, height)) {
        okgf_cancel_read_jpeg(context);
        return NULL;
    }
    return context;
}

int32_t OKGF_CALL OKGF_Read_JPEG(OkgfJpegReadContext *context, void *pixels, int32_t pitch_bytes) {
    JpegState *state = context->decoder;
    if (setjmp(state->failure))
        return 0;
    while (state->jpeg.output_scanline < state->jpeg.output_height) {
        JSAMPROW row = (uint8_t *)pixels + (ptrdiff_t)state->jpeg.output_scanline * pitch_bytes;
        if (jpeg_read_scanlines(&state->jpeg, &row, 1) != 1)
            return 0;
    }
    jpeg_finish_decompress(&state->jpeg);
    okgf_cancel_read_jpeg(context);
    return 1;
}
