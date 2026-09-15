#ifndef OKGF_RECOVERED_H
#define OKGF_RECOVERED_H
#include <stdint.h>
#if defined(_MSC_VER)
#define OKGF_CALL __cdecl
#elif defined(__i386__)
#define OKGF_CALL __attribute__((cdecl))
#else
#define OKGF_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Per-thread arithmetic precision. AUTO uses the current x87 setting on x86 and 24-bit
 * significands elsewhere. Operations round to nearest, ties to even. The host FPU settings are
 * unchanged; invalid precision values are ignored. NATIVE uses binary64 for both 53-bit and
 * 64-bit requests. */
typedef enum OkgfMathPrecision {
    OKGF_MATH_AUTO = 0,
    OKGF_MATH_GAME = 24,
    OKGF_MATH_DOUBLE = 53,
    OKGF_MATH_EXTENDED = 64
} OkgfMathPrecision;
void OKGF_CALL okgf_set_math_precision(OkgfMathPrecision precision);
OkgfMathPrecision OKGF_CALL okgf_get_math_precision(void);

/* MMX support is zero on non-x86 targets. GetCPUFeatures reports flags on its first call only:
 * bit 0 indicates MMX support; bit 1 indicates a measured preference for scalar copies. Later
 * calls return zero. */
int32_t OKGF_CALL OKGF_IsMMX(void);
uint32_t OKGF_CALL OKGF_GetCPUFeatures(void);

/* Image pitches are measured in bytes. Pixel and stream storage is little-endian. */
typedef enum OkgfImageKind {
    OKGF_IMAGE_UNKNOWN = 0,
    OKGF_IMAGE_BMP = 1,
    OKGF_IMAGE_INDEXED_BMP = 2,
    OKGF_IMAGE_JPEG = 3,
    OKGF_IMAGE_PNG = 4,
    OKGF_IMAGE_INDEXED_PSD = 5,
    OKGF_IMAGE_GRAYSCALE_PSD = 6,
    OKGF_IMAGE_RGB_PSD = 7,
    OKGF_IMAGE_CMYK_PSD = 8
} OkgfImageKind;

/* Windows helpers use GDI, DxDiag, and version APIs. Other hosts can supply an OkgfPlatform
 * backend. Bitmap creation failure leaves output unchanged. */
uint32_t OKGF_CALL DXVersion(void);
void OKGF_CALL OKGR_StretchGdi_WORD(void *dest, uint32_t width, uint32_t height, const void *source,
                                    uint32_t source_width, uint32_t source_height);

OkgfImageKind OKGF_CALL OKGF_Tip(const void *source, int32_t source_size);
void OKGF_CALL OKGF_ConvertRGBto565(const void *source, void *dest, int32_t dest_pitch,
                                    int32_t width, int32_t height);
/* Original: 0x1005B570. RGB555 helper, exposed in addition to the original exports. */
void OKGF_CALL okgf_convert_rgb_to555(const void *source, void *dest, int32_t dest_pitch,
                                      int32_t width, int32_t height);

#define OKGF_EXPAND_DECL(name)                                                                     \
    void OKGF_CALL name(const void *source, int32_t source_pitch, void *dest, int32_t dest_pitch,  \
                        int32_t width, int32_t height)
OKGF_EXPAND_DECL(OKGF_Convert565toRGB);
OKGF_EXPAND_DECL(OKGF_Convert565toBGR);
OKGF_EXPAND_DECL(OKGF_Convert565toRGBA);
OKGF_EXPAND_DECL(OKGF_Convert565toBGRA);
OKGF_EXPAND_DECL(OKGF_Convert5658toRGBA);
OKGF_EXPAND_DECL(OKGF_Convert5658toBGRA);
#undef OKGF_EXPAND_DECL

#define OKGF_RECT_ARGS                                                                             \
    void *dest, int32_t dest_pitch, int32_t dest_x, int32_t dest_y, const void *source,            \
        int32_t source_pitch, int32_t source_x, int32_t source_y, int32_t width, int32_t height
void OKGF_CALL OKGF_Convert_888to565(OKGF_RECT_ARGS);
void OKGF_CALL OKGF_Convert_8888to565(OKGF_RECT_ARGS);
void OKGF_CALL OKGF_Convert_RGBAtoRGB(OKGF_RECT_ARGS);
void OKGF_CALL OKGR_Copy_XY_XY_WORD(OKGF_RECT_ARGS);
void OKGF_CALL OKGR_CopyBed_XY_XY_WORD(OKGF_RECT_ARGS);
void OKGF_CALL OKGR_CopyTrans_XY_XY_WORD(OKGF_RECT_ARGS, uint16_t transparent_color);
void OKGF_CALL OKGR_HACopy_XY_XY_16(OKGF_RECT_ARGS);

#define OKGF_PAL_ARGS                                                                              \
    void *dest, int32_t dest_pitch, int32_t dest_x, int32_t dest_y, const void *source,            \
        int32_t source_pitch, int32_t source_x, int32_t source_y, const void *palette,             \
        int32_t width, int32_t height
void OKGF_CALL OKGR_PalCopy_XY_XY_WORD(OKGF_PAL_ARGS);
void OKGF_CALL OKGR_PalCopy_XY_XY_DWORD(OKGF_PAL_ARGS);
void OKGF_CALL OKGR_PalCopySwap_XY_XY_DWORD(OKGF_PAL_ARGS);

void OKGF_CALL OKGR_Fill_WORD(void *pixels, int32_t pitch, int32_t width, int32_t height,
                              uint16_t color);
void OKGF_CALL OKGR_HalfLight_16(void *pixels, int32_t pitch, int32_t width, int32_t height);
void OKGF_CALL OKGR_ShrLight_16(void *pixels, int32_t pitch, int32_t width, int32_t height,
                                int32_t shift);
void OKGF_CALL OKGR_Light_BYTE(void *pixels, int32_t pixel_step, int32_t row_skip, int32_t width,
                               int32_t height, uint8_t factor);
void OKGF_CALL OKGR_MulLightMask_16(void *pixels, int32_t pitch, const void *mask,
                                    int32_t mask_pitch, int32_t width, int32_t height);
uint8_t *OKGF_CALL OKGF_MulTable256x256(void);

typedef struct OkgfRect {
    int32_t left, top, right, bottom;
} OkgfRect;
/* Endpoint colors use 0xAARRGGBB; the destination uses RGB565. Gradient lines include both
 * endpoints and divide color steps by length + 1. Triangles exclude the rightmost pixel and
 * bottom scanline. */
void OKGF_CALL OKGF_LineIp_16(void *pixels, int32_t pitch, int32_t x1, int32_t y1, uint32_t color1,
                              int32_t x2, int32_t y2, uint32_t color2);
void OKGF_CALL OKGF_Triangle_16(void *pixels, int32_t pitch, int32_t x1, int32_t y1,
                                uint32_t color1, int32_t x2, int32_t y2, uint32_t color2,
                                int32_t x3, int32_t y3, uint32_t color3, const OkgfRect *clip);
/* Line clip rectangles have exclusive right/bottom boundaries. Rejected clips
 * leave endpoint/color outputs untouched; accepted clips may swap endpoints. */
int32_t OKGF_CALL OKGR_Line_Clip(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2,
                                 const OkgfRect *clip);
int32_t OKGF_CALL OKGR_LineColor_Clip(int32_t *x1, int32_t *y1, uint32_t *color1, int32_t *x2,
                                      int32_t *y2, uint32_t *color2, const OkgfRect *clip);
void OKGF_CALL OKGR_PixelAlpha_16(void *pixel, uint16_t color, uint8_t alpha);
#define OKGF_LINE_ARGS                                                                             \
    void *pixels, int32_t pitch, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t color
void OKGF_CALL OKGR_Line_Draw_WORD(OKGF_LINE_ARGS);
void OKGF_CALL OKGR_Line_DrawClip_WORD(OKGF_LINE_ARGS, const OkgfRect *clip);
/* Unclipped alpha line drawing; the clipped variant calls this helper. */
void OKGF_CALL OKGR_Line_Draw_Alpha_16(OKGF_LINE_ARGS, uint8_t alpha);
void OKGF_CALL OKGR_Line_DrawClip_Alpha_16(OKGF_LINE_ARGS, uint8_t alpha, const OkgfRect *clip);
int32_t OKGF_CALL OKGR_Line_CopyToBuf_WORD(void *linear_dest, const void *source, int32_t pitch,
                                           int32_t x1, int32_t y1, int32_t x2, int32_t y2);
int32_t OKGF_CALL OKGR_Line_CopyFromBuf_WORD(const void *linear_source, void *dest, int32_t pitch,
                                             int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void OKGF_CALL OKGR_AnimLine_Draw_16(OKGF_LINE_ARGS, int32_t phase, const OkgfRect *clip);
/* The last two arguments specify a shadow mask and its byte pitch. Mask values must be in
 * 0..7. */
void OKGF_CALL OKGR_AnimShadowLine_Draw_16(OKGF_LINE_ARGS, int32_t phase, const OkgfRect *clip,
                                           const uint8_t *shadow, int32_t shadow_pitch);
/* The raw source is BGRA; the indexed variant's palette is RGBA. */
void OKGF_CALL OKGR_AlphaSimpleBuf_Draw_16(OKGF_RECT_ARGS);
void OKGF_CALL OKGR_AlphaSimpleBufPalAlpha_Draw_16(OKGF_RECT_ARGS, const void *palette);
void OKGF_CALL OKGR_ShrLightMask_16(void *pixels, int32_t pitch, const void *mask,
                                    int32_t mask_pitch, int32_t width, int32_t height);

/* A 16-byte little-endian header precedes the RLE commands. Builders return the total size,
 * including the header; dest = NULL measures the size without writing. Builders with RGBA in
 * their names read BGRA bytes. */
typedef struct OkgfRleHeader {
    int32_t stream_bytes, width, height;
    uint32_t format_info;
} OkgfRleHeader;
int32_t OKGF_CALL OKGR_TransBuf_Build_WORD(const void *source, int32_t pitch, int32_t width,
                                           int32_t height, void *dest, uint16_t transparent_color);
int32_t OKGF_CALL OKGR_TransBuf_BuildFromRGBA_16(const void *source, int32_t pitch, int32_t width,
                                                 int32_t height, void *dest);
int32_t OKGF_CALL OKGR_TransAlphaBuf_BuildFromRGBA_16(const void *source, int32_t pitch,
                                                      int32_t width, int32_t height, void *dest);
int32_t OKGF_CALL OKGR_AlphaBuf_BuildFromRGBA(const void *source, int32_t pitch, int32_t width,
                                              int32_t height, void *dest);
#define OKGF_RLE_ARGS void *dest, int32_t pitch, const OkgfRleHeader *source
void OKGF_CALL OKGR_TransBuf_Draw_WORD(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_TransBuf_Draw_RGBA(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_TransBuf_HADraw_16(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_TransAlphaBuf_Draw_WORD(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_TransAlphaBuf_Draw_RGBA(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_AlphaBuf_Draw_16(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_AlphaBuf_Draw_RGBA(OKGF_RLE_ARGS);
#define OKGF_RLE_CLIP_ARGS                                                                         \
    void *dest, int32_t pitch, int32_t x, int32_t y, const OkgfRleHeader *source,                  \
        const OkgfRect *clip
void OKGF_CALL OKGR_TransBuf_DrawClip_WORD(OKGF_RLE_CLIP_ARGS);
void OKGF_CALL OKGR_TransBuf_HADrawClip_16(OKGF_RLE_CLIP_ARGS);
void OKGF_CALL OKGR_TransAlphaBuf_DrawClip_WORD(OKGF_RLE_CLIP_ARGS);
void OKGF_CALL OKGR_AlphaBuf_DrawClip_16(OKGF_RLE_CLIP_ARGS);
void OKGF_CALL OKGR_MaskBuf_Draw_WORD(OKGF_RLE_ARGS, uint16_t color);
void OKGF_CALL OKGR_MaskBuf_Draw_DWORD(OKGF_RLE_ARGS, uint32_t color);
void OKGF_CALL OKGR_MaskBuf_DrawClip_WORD(void *dest, int32_t pitch, int32_t x, int32_t y,
                                          const OkgfRleHeader *source, uint16_t color,
                                          const OkgfRect *clip);
void OKGF_CALL OKGR_MaskBuf_DrawClip_DWORD(void *dest, int32_t pitch, int32_t x, int32_t y,
                                           const OkgfRleHeader *source, uint32_t color,
                                           const OkgfRect *clip);

/* These in-memory objects contain native pointers. Their layouts match the original DLL on
 * 32-bit targets. Serialized PE32 pointers need translation before use on other targets. */
typedef struct OkgfLightBuffer {
    int32_t width, height, pitch_bytes;
    int32_t origin_x, origin_y;
    uint8_t *pixels;
} OkgfLightBuffer;
typedef struct OkgfRotationScanline {
    int32_t dest_x, pixel_count;
    int32_t source_x_start, source_y_start, source_x_end, source_y_end;
    int32_t source_dx_16_16, source_dy_16_16;
} OkgfRotationScanline;
typedef struct OkgfRotationFrame {
    int32_t dest_y, scanline_count, min_x, max_x;
    OkgfRotationScanline *scanlines;
} OkgfRotationFrame;
typedef struct OkgfRotationBuffer {
    OkgfRotationFrame frames[256];
} OkgfRotationBuffer;

OkgfLightBuffer *OKGF_CALL OKGR_LightBuf_Create(int32_t width, int32_t height);
void OKGF_CALL OKGR_LightBuf_Destroy(OkgfLightBuffer *buffer);
void OKGF_CALL OKGR_LightBuf_SetSme(OkgfLightBuffer *buffer, int32_t origin_x, int32_t origin_y);
void OKGF_CALL OKGR_LightBuf_Init(OkgfLightBuffer *buffer, uint8_t value);
void OKGF_CALL OKGR_LightBuf_LoadFromPalBuf(OkgfLightBuffer *buffer, const uint8_t *source,
                                            int32_t source_width, int32_t source_height,
                                            int32_t source_pitch, const void *palette);
void OKGF_CALL OKGR_LightBuf_Rotate(OkgfLightBuffer *dest, const OkgfLightBuffer *source,
                                    const OkgfRotationBuffer *rotation, uint8_t angle);
/* Width and height must be at least 3, source dimensions must be positive, and coordinate
 * arithmetic must be representable. Returns NULL for invalid dimensions or allocation failure. */
OkgfRotationBuffer *OKGF_CALL OKGR_RotateBuf_Build(int32_t width, int32_t height,
                                                   int32_t source_width, int32_t source_height,
                                                   int32_t center_x, int32_t center_y);
/* Original: 0x10061DF0. Edge walker; outputs source X/Y pairs at 32-byte strides. */
void OKGF_CALL okgf_rotation_edge(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t *dest_x,
                                  int32_t sx0, int32_t sy0, int32_t sx1, int32_t sy1,
                                  int32_t *source_coordinates);
/* Frees the buffer and its scanline arrays. NULL is accepted. */
void OKGF_CALL OKGR_RotateBuf_Free(OkgfRotationBuffer *buffer);
void OKGF_CALL OKGR_RotateBuf_Size(int32_t dest_x, int32_t dest_y, uint8_t angle,
                                   const OkgfRotationBuffer *rotation, OkgfRect *bounds);
#define OKGF_ROTATE_ARGS                                                                           \
    void *dest, int32_t dest_pitch, const void *source, int32_t source_pitch, int32_t dest_x,      \
        int32_t dest_y, uint8_t angle, const OkgfRotationBuffer *rotation
void OKGF_CALL OKGR_RotateBuf_Draw_BYTE(OKGF_ROTATE_ARGS);
void OKGF_CALL OKGR_RotateBuf_Draw_DWORD(OKGF_ROTATE_ARGS);
void OKGF_CALL OKGR_RotateBuf_DrawTrans_WORD(OKGF_ROTATE_ARGS);
void OKGF_CALL OKGR_RotateBuf_DrawTransClip_WORD(OKGF_ROTATE_ARGS, const OkgfRect *clip);
/* BYTE transparency tests and stores a WORD at each byte position. Source and destination
 * therefore need one extra accessible byte, including at a clip edge. */
void OKGF_CALL OKGR_RotateBuf_DrawTrans_BYTE(OKGF_ROTATE_ARGS);
void OKGF_CALL OKGR_RotateBuf_DrawTransClip_BYTE(OKGF_ROTATE_ARGS, const OkgfRect *clip);

/* Planet templates own their scanlines and samples. TemplBuild reports the original 32-bit
 * allocation size in byte_count, including on 64-bit hosts. Variant 2 reads texture indices;
 * variant 3 adds a lighting byte; variant 4 adds an opacity byte. The palette holds 256 * 64
 * little-endian RGB565 colors. Clip right and bottom bounds are exclusive. Edge alpha values
 * are in 0..63; scanline run counts must be valid. */
typedef struct OkgfPlanetSample {
    uint16_t source_x, edge_alpha63;
} OkgfPlanetSample;
typedef struct OkgfPlanetScanline {
    int32_t source_y, dest_x, pixel_count;
    int32_t left_alpha_count, opaque_count, right_alpha_count;
    OkgfPlanetSample *samples;
} OkgfPlanetScanline;
typedef struct OkgfPlanetTemplate {
    int32_t width, height, origin_x, origin_y;
    OkgfPlanetScanline *scanlines;
} OkgfPlanetTemplate;
/* Each source row must have a nonempty, single-peaked alpha silhouette and one readable
 * neighboring pixel at each end. Source Y coordinates are rounded without clamping. Partial
 * masks that touch a boundary can produce negative run counts; those templates cannot be drawn
 * safely. Invalid geometry or allocation failure returns NULL. TemplDel accepts NULL. */
OkgfPlanetTemplate *OKGF_CALL OKGR_Planet2_TemplBuild(const void *source, int32_t source_pitch,
                                                      int32_t diameter, int32_t texture_width,
                                                      int32_t texture_height, int32_t *byte_count);
void OKGF_CALL OKGR_Planet2_TemplDel(OkgfPlanetTemplate *template_data);
#define OKGF_PLANET_ARGS                                                                           \
    void *dest, int32_t dest_pitch, const OkgfPlanetTemplate *template_data,                       \
        const uint8_t *texture, int32_t texture_pitch, uint32_t texture_x_mask,                    \
        int32_t texture_x_offset, const OkgfLightBuffer *light, const uint16_t *light_palette,     \
        int32_t dest_x, int32_t dest_y
void OKGF_CALL OKGR_Planet2_DrawAndLight_32(OKGF_PLANET_ARGS);
void OKGF_CALL OKGR_Planet3_DrawAndLight_32(OKGF_PLANET_ARGS);
void OKGF_CALL OKGR_Planet4_DrawAndLight_32(OKGF_PLANET_ARGS);
void OKGF_CALL OKGR_Planet2_DrawAndLight_16(OKGF_PLANET_ARGS);
void OKGF_CALL OKGR_Planet3_DrawAndLight_16(OKGF_PLANET_ARGS);
void OKGF_CALL OKGR_Planet4_DrawAndLight_16(OKGF_PLANET_ARGS);
void OKGF_CALL OKGR_Planet2_DrawAndLightClip_16(OKGF_PLANET_ARGS, const OkgfRect *clip);
void OKGF_CALL OKGR_Planet3_DrawAndLightClip_16(OKGF_PLANET_ARGS, const OkgfRect *clip);
void OKGF_CALL OKGR_Planet4_DrawAndLightClip_16(OKGF_PLANET_ARGS, const OkgfRect *clip);

/* Circle and span clip bounds are inclusive. Outline circles use unsigned coordinate
 * comparisons and require a positive radius. Filled circles use the original radius - 1
 * raster, including its degenerate cases. A WORD span clipped on the left is not clipped again
 * on the right, so destination storage must cover the original span. Negative span lengths
 * include X and extend left; INT_MIN is unsupported. */
#define OKGF_CIRCLE_ARGS                                                                           \
    void *pixels, int32_t pitch, int32_t center_x, int32_t center_y, int32_t radius
void OKGF_CALL OKGR_Circle_DrawClip_BYTE(OKGF_CIRCLE_ARGS, uint8_t color, const OkgfRect *clip);
void OKGF_CALL OKGR_Circle_DrawClip_WORD(OKGF_CIRCLE_ARGS, uint16_t color, const OkgfRect *clip);
void OKGF_CALL OKGR_Circle_DrawFillClip_BYTE(OKGF_CIRCLE_ARGS, uint8_t color, const OkgfRect *clip);
void OKGF_CALL OKGR_Circle_DrawFillClip_WORD(OKGF_CIRCLE_ARGS, uint16_t color,
                                             const OkgfRect *clip);
#define OKGF_SPAN_ARGS void *pixels, int32_t pitch, int32_t x, int32_t y, int32_t length
void OKGF_CALL OKGR_GLine_DrawClip_BYTE(OKGF_SPAN_ARGS, uint8_t color, const OkgfRect *clip);
void OKGF_CALL OKGR_GLine_DrawClip_WORD(OKGF_SPAN_ARGS, uint16_t color, const OkgfRect *clip);
/* All three trapezium functions use the coordinate order shown below. Both rows and span
 * endpoints are included; clip right and bottom bounds are exclusive. Left and right edges
 * must remain ordered. Edge deltas and height must fit int32_t, with valid signed division. */
#define OKGF_TRAPEZIUM_ARGS                                                                        \
    void *pixels, int32_t pitch, int32_t top_left, int32_t top_right, int32_t top_y,               \
        int32_t bottom_left, int32_t bottom_right, int32_t bottom_y
void OKGF_CALL OKGR_Alpha64Trapezium_16(OKGF_TRAPEZIUM_ARGS, uint16_t color, const OkgfRect *clip);
void OKGF_CALL OKGR_Alpha128Trapezium_16(OKGF_TRAPEZIUM_ARGS, uint16_t color, const OkgfRect *clip);
void OKGF_CALL OKGR_FillTrapezium_DWORD(OKGF_TRAPEZIUM_ARGS, uint32_t color, const OkgfRect *clip);

/* Copies WORD pixels within a buffer. Dimensions must be positive and any alignment padding
 * must be accessible. Copy direction depends on the coordinates. Row cursors retain the
 * original alignment behavior. The first CPU probe can select the MMX entry point; its
 * implementation is portable C. */
void OKGF_CALL OKGR_CopySingleBuf_XY_XY_WORD(void *pixels, int32_t pitch, int32_t dest_x,
                                             int32_t dest_y, int32_t source_x, int32_t source_y,
                                             int32_t width, int32_t height);
/* MMX grouping requires width >= the initial alignment prefix plus tail.
 * Invalid/narrow shapes that underflow the original group count are ignored. */
void OKGF_CALL OKGR_CopySingleBuf_XY_XY_WORD_MMX(void *pixels, int32_t pitch, int32_t dest_x,
                                                 int32_t dest_y, int32_t source_x, int32_t source_y,
                                                 int32_t width, int32_t height);
typedef enum OkgfRescaleFilter {
    OKGF_RESCALE_BOX = 0,
    OKGF_RESCALE_TRIANGLE = 1,
    OKGF_RESCALE_BELL = 2,
    OKGF_RESCALE_BSPLINE = 3,
    OKGF_RESCALE_CUBIC = 4,
    OKGF_RESCALE_LANCZOS3 = 5,
    OKGF_RESCALE_MITCHELL = 6
} OkgfRescaleFilter;
/* Two-pass byte-channel rescaling using OkgfRescaleFilter. Each pass truncates pixels to
 * bytes; filter weights are not normalized. Dimensions and destination pitch must be positive.
 * Source indices are reflected once and must then lie within the image, including taps with
 * zero weight. Invalid input or allocation failure leaves output unchanged. Arithmetic follows
 * the selected math backend; NATIVE can produce different rounding results. */
void OKGF_CALL OKGF_Rescale(void *dest, int32_t width, int32_t height, int32_t dest_pitch,
                            const void *source, int32_t source_width, int32_t source_height,
                            int32_t source_pitch, int32_t bytes_per_pixel, int32_t filter);

/* BMP contexts borrow the source until decoding finishes. Successful decoding consumes the
 * context. The RGB decoder writes three bytes per pixel, including for accepted depths above
 * 24. Indexed begin accepts 1-, 4-, and 8-bit headers; indexed decode supports only
 * uncompressed 8-bit data and a packed RGB palette.
 * Row stride rounds up only when (row_bytes & 2) is nonzero.
 * Input dimensions must be positive and all input data readable. Begin returns
 * NULL on failure; failed decode returns 0 and leaves the context owned by the caller. */
typedef struct OkgfBmpReadContext {
    const uint8_t *header;
    const uint8_t *pixels;
    int32_t palette_count;
    const uint8_t *source_data;
    int32_t source_size, owns_source;
} OkgfBmpReadContext;
OkgfBmpReadContext *OKGF_CALL OKGR_ReadStart_BMP_Buf(const uint8_t *source, int32_t source_size,
                                                     int32_t *width, int32_t *height);
OkgfBmpReadContext *OKGF_CALL OKGR_ReadStart_BMPPAL_Buf(const uint8_t *source, int32_t source_size,
                                                        int32_t *width, int32_t *height,
                                                        int32_t *palette_count);
int32_t OKGF_CALL OKGF_Read_BMP(OkgfBmpReadContext *context, void *pixels, int32_t pitch_bytes);
int32_t OKGF_CALL OKGF_Read_BMPPAL(OkgfBmpReadContext *context, void *pixels, int32_t pitch_bytes,
                                   void *palette_rgb);
/* PNG decoding reduces 16-bit samples to 8 bits and unpacks sub-byte samples. Ordinary decoding
 * expands palettes and transparency, then writes RGB or RGBA through the supplied masks; RGB
 * sources use alpha 0. Indexed decoding accepts palettes or grayscale, ignores tRNS, and writes
 * RGBA palette entries with alpha 0. Low-depth grayscale values are unpacked without scaling. The
 * source is borrowed. Successful decoding frees the context and, when owns_source is set, the
 * source. Begin returns NULL on failure; failed decode returns 0 and leaves the context owned
 * by the caller. Codec structures belong to the linked libpng version. */
typedef struct OkgfPngReadContext {
    void *png, *info;
    const uint8_t *palette_rgb;
    int32_t palette_count;
    const uint8_t *source_data;
    int32_t source_cursor, source_size, owns_source;
    int32_t width, height;
} OkgfPngReadContext;
OkgfPngReadContext *OKGF_CALL OKGR_ReadStart_PNG_Buf(const uint8_t *source, int32_t source_size,
                                                     int32_t *width, int32_t *height);
OkgfPngReadContext *OKGF_CALL OKGR_ReadStart_PNGPAL_Buf(const uint8_t *source, int32_t source_size,
                                                        int32_t *width, int32_t *height,
                                                        int32_t *palette_count);
int32_t OKGF_CALL OKGF_Read_PNG(OkgfPngReadContext *context, void *pixels, int32_t pitch_bytes,
                                uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask,
                                uint32_t alpha_mask, int32_t bytes_per_pixel);
int32_t OKGF_CALL OKGF_Read_PNGPAL(OkgfPngReadContext *context, void *pixels, int32_t pitch_bytes,
                                   void *palette_rgba);

/* JPEG decoding uses libjpeg defaults. RGB, grayscale, and CMYK produce 3, 1, and 4 bytes per
 * pixel, respectively. The source is borrowed; successful decoding consumes the context and
 * frees an owned source. Begin returns NULL on failure; failed decode returns 0 and leaves the
 * context owned by the caller. Codec structures belong to the linked libjpeg version. */
typedef struct OkgfJpegReadContext {
    void *decoder;
    const uint8_t *source_data;
    int32_t source_size, owns_source, width, height, channels;
} OkgfJpegReadContext;
OkgfJpegReadContext *OKGF_CALL OKGR_ReadStart_JPEG_Buf(const uint8_t *source, int32_t source_size,
                                                       int32_t *width, int32_t *height);
int32_t OKGF_CALL OKGF_Read_JPEG(OkgfJpegReadContext *context, void *pixels, int32_t pitch_bytes);

/* PSD input requires positive dimensions, 1..4 channels, and 8-bit samples, stored raw or with
 * row RLE. Channels are interleaved without color conversion. Indexed mode 2 creates four-byte
 * RGBA entries from 768-byte RGB planes or a 1024-byte RGBA extension. RLE command 128 repeats
 * 129 bytes. Rows must be complete and fit the reader's scratch buffers. Begin copies and
 * byte-swaps the 26-byte header while borrowing the source. Successful decoding frees the
 * context, header, and any owned source. Begin returns NULL on failure; decode returns 0. The
 * header copy uses little-endian fields. */
typedef struct OkgfPsdReadContext {
    uint8_t *header;
    const uint8_t *source_data;
    int32_t source_size, owns_source;
} OkgfPsdReadContext;
OkgfPsdReadContext *OKGF_CALL OKGR_ReadStart_PSD_Buf(const uint8_t *source, int32_t source_size,
                                                     int32_t *width, int32_t *height);
OkgfPsdReadContext *OKGF_CALL OKGR_ReadStart_PSDPAL_Buf(const uint8_t *source, int32_t source_size,
                                                        int32_t *width, int32_t *height,
                                                        int32_t *palette_count);
int32_t OKGF_CALL OKGF_Read_PSD(OkgfPsdReadContext *context, void *pixels, int32_t pitch_bytes);
int32_t OKGF_CALL OKGF_Read_PSDPAL(OkgfPsdReadContext *context, void *pixels, int32_t pitch_bytes,
                                   void *palette_rgba);

/* Image readers own a format-specific context and borrow the source. Successful Read/ReadPal
 * calls consume both contexts and free the source when owns_source is set. ReadStartPal
 * accepts PNG and indexed PSD with one or two channels. It fills the caller's palette count
 * but leaves the outer context's palette_count at zero.
 *
 * Read packs the supplied masks and uses alpha 0 for three-channel sources. Four-plane PSD
 * data is treated as RGBA, including CMYK input. The direct RGB-mask path for a four-plane PSD
 * writes four bytes even when bytes_per_pixel is 3; destination storage must account for this.
 * Use ReadPal for indexed PSD. Ordinary JPEG Read supports RGB only. Dimensions must be
 * positive; negative destination pitches are accepted. Begin returns NULL on failure. Failed
 * decoding returns 0 and leaves the context owned by the caller. */
typedef struct OkgfReadContext {
    void *codec_context;
    OkgfImageKind image_kind;
    int32_t width, height, palette_count;
    const uint8_t *source_data;
    int32_t source_size, owns_source;
} OkgfReadContext;
OkgfReadContext *OKGF_CALL OKGF_ReadStart_Buf(const void *source, int32_t source_size,
                                              int32_t *width, int32_t *height);
OkgfReadContext *OKGF_CALL OKGF_ReadStartPal_Buf(const void *source, int32_t source_size,
                                                 int32_t *width, int32_t *height,
                                                 int32_t *palette_count, int32_t *bytes_per_pixel);
int32_t OKGF_CALL OKGF_Read(OkgfReadContext *context, void *pixels, int32_t pitch_bytes,
                            uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask,
                            uint32_t alpha_mask, int32_t bytes_per_pixel);
int32_t OKGF_CALL OKGF_ReadPal(OkgfReadContext *context, void *pixels, int32_t pitch_bytes,
                               void *palette_rgba);

/* BMP writes uncompressed 16-, 24-, or 32-bit pixels, copying full positive-pitch rows in
 * reverse order. Its BI_RGB header ignores color masks and clears reserved bytes. PNG writes
 * RGB or RGBA, with optional red/blue swapping. PNG returns 0 on failure. BMP retains the
 * original return value of 1, including when opening or writing the file fails. */
int32_t OKGF_CALL OKGF_Write_BMP_File(const char *filename, const void *pixels, int32_t pitch_bytes,
                                      int32_t bits_per_pixel, uint32_t red_mask,
                                      uint32_t green_mask, uint32_t blue_mask, uint32_t alpha_mask,
                                      int32_t width, int32_t height);
int32_t OKGF_CALL OKGF_Write_PNG_File(const char *filename, const void *pixels, int32_t pitch_bytes,
                                      int32_t width, int32_t height, int32_t has_alpha,
                                      int32_t swap_red_blue);

/* Indexed sprites store a palette after the RLE header, followed by one index per literal
 * pixel. The low byte of format_info gives the palette length; zero means 256. stream_bytes
 * excludes the palette. CopyDraw/Draw use RGB565 entries. AlphaDraw uses a premultiplied
 * RGB565 WORD and an inverse-alpha WORD in 0..63. Clip right and bottom bounds are inclusive.
 * The optional alpha argument follows the clip pointer. */
typedef struct OkgfIndexedAlphaEntry {
    uint16_t premultiplied_color;
    uint16_t inverse_alpha;
} OkgfIndexedAlphaEntry;
void OKGF_CALL OKGR_AlphaIndexed_CopyDraw_WORD(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_AlphaIndexed_CopyDraw_Alpha_16(OKGF_RLE_ARGS, uint8_t alpha);
void OKGF_CALL OKGR_AlphaIndexed_Draw_RGBA(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_AlphaIndexed_AlphaDraw_16(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_AlphaIndexed_AlphaDraw_Alpha_16(OKGF_RLE_ARGS, uint8_t alpha);
void OKGF_CALL OKGR_AlphaIndexed_AlphaDraw_RGBA(OKGF_RLE_ARGS);
void OKGF_CALL OKGR_AlphaIndexed_CopyDrawClip_WORD(OKGF_RLE_CLIP_ARGS);
void OKGF_CALL OKGR_AlphaIndexed_CopyDrawClip_Alpha_16(OKGF_RLE_CLIP_ARGS, uint8_t alpha);
void OKGF_CALL OKGR_AlphaIndexed_AlphaDrawClip_16(OKGF_RLE_CLIP_ARGS);
void OKGF_CALL OKGR_AlphaIndexed_AlphaDrawClip_Alpha_16(OKGF_RLE_CLIP_ARGS, uint8_t alpha);

/* Font alpha masks have one literal opacity byte per pixel. RGB565 blends;
 * BGRA replaces RGB with color's low 24 bits and alpha with the literal byte. */
void OKGF_CALL OKGR_TransBuf_FillAlpha_16(OKGF_RLE_ARGS, uint16_t color);
void OKGF_CALL OKGR_TransBuf_FillAlpha_RGBA(OKGF_RLE_ARGS, uint32_t color);
void OKGF_CALL OKGR_TransBuf_FillAlphaClip_16(OKGF_RLE_CLIP_ARGS, uint16_t color);
void OKGF_CALL OKGR_TransBuf_FillAlphaClip_RGBA(OKGF_RLE_CLIP_ARGS, uint32_t color);

/* Delta streams modify existing BGRA bytes in place, wrapping modulo 256.
 * F5 has a 16-byte header plus a row-index table; F6 has a 4-byte header.
 * Both require valid streams and enough destination storage; neither clips. */
typedef struct OkgfF5Header {
    uint8_t ignored_prefix[8];
    uint32_t row_index_count;
    uint8_t channel_precision[2];
    uint8_t ignored_suffix[2];
} OkgfF5Header;
typedef struct OkgfF6Header {
    uint8_t channel_precision[2];
    uint16_t segment_count;
} OkgfF6Header;
void OKGF_CALL OKGR_F5_DrawRGBA(void *dest, int32_t pitch, const OkgfF5Header *source);
void OKGF_CALL OKGR_F6_DrawRGBA(void *dest, int32_t pitch, const OkgfF6Header *source);

#ifdef __cplusplus
}
#endif
#endif
