#include "okgf.h"
#include "okgf_internal.h"
#include "math/edge.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Dispatch on matching RGB channels and four special alpha values. Color interpolation keeps
 * 16 fractional bits. */
static uint16_t pack565(const int32_t c[4]) {
    return (uint16_t)(((c[2] >> 8) & 0xf800) | ((c[1] >> 13) & 0x7e0) | ((c[0] >> 19) & 31));
}
static unsigned pack_scaled(unsigned a, unsigned r, unsigned g, unsigned b) {
    return ((okgf_scale63_rounded(a, r) & 62) << 10) | (okgf_scale63_rounded(a, g) << 5) |
           (okgf_scale63_rounded(a, b) >> 1);
}
static void shade(uint8_t *p, const int32_t c[4], unsigned mode) {
    unsigned old = p[0] | (unsigned)p[1] << 8, s = pack565(c), out;
    switch (mode) {
    case 255:
        out = s;
        break;
    case 128:
        out = ((s >> 1) & 0x7bef) + ((old >> 1) & 0x7bef);
        break;
    case 192:
        out = s - ((s >> 2) & 0x39e7) + ((old >> 2) & 0x39e7);
        break;
    case 64:
        out = ((s >> 2) & 0x39e7) + old - ((old >> 2) & 0x39e7);
        break;
    default: {
        unsigned a = (c[3] >> 18) & 63;
        out = pack_scaled(a, (unsigned)c[2] >> 18, (unsigned)c[1] >> 18, (unsigned)c[0] >> 18) +
              pack_scaled(63 - a, (old >> 10) & 62, (old >> 5) & 63, (old & 31) * 2);
        break;
    }
    }
    p[0] = (uint8_t)out;
    p[1] = (uint8_t)(out >> 8);
}
static void channels(int32_t c[4], uint32_t color) {
    for (int i = 0; i < 4; ++i)
        c[i] = ((color >> (8 * i)) & 255) * 65536;
}
static unsigned alpha_mode(uint32_t a, uint32_t b, uint32_t c) {
    a >>= 24;
    b >>= 24;
    c >>= 24;
    return a == b && b == c && (a == 64 || a == 128 || a == 192 || a == 255) ? a : 0;
}
void OKGF_CALL OKGF_LineIp_16(void *pixels, int32_t pitch, int32_t x1, int32_t y1, uint32_t color1,
                              int32_t x2, int32_t y2, uint32_t color2) {
    if (!((color1 | color2) & 0xff000000))
        return;
    int32_t c[4], end[4], step[4];
    channels(c, color1);
    channels(end, color2);
    int64_t dx = llabs((int64_t)x2 - x1), dy = llabs((int64_t)y2 - y1);
    int64_t major = dx >= dy ? dx : dy, minor = dx >= dy ? dy : dx;
    ptrdiff_t sx = x2 < x1 ? -2 : 2, sy = y2 < y1 ? -(ptrdiff_t)pitch : pitch;
    ptrdiff_t along = dx >= dy ? sx : sy, across = dx >= dy ? sy : sx;
    for (int k = 0; k < 4; ++k)
        step[k] = (int32_t)((end[k] - c[k]) / (major + 1));
    unsigned mode = alpha_mode(color1, color2, color2);
    uint8_t *p = (uint8_t *)pixels + (ptrdiff_t)y1 * pitch + (ptrdiff_t)x1 * 2;
    int64_t error = 2 * minor - major;
    for (int64_t i = 0;; ++i) {
        shade(p, c, mode);
        if (i == major)
            break;
        p += along;
        if (error > 0) {
            p += across;
            error -= 2 * major;
        }
        error += 2 * minor;
        for (int k = 0; k < 4; ++k)
            c[k] += step[k];
    }
}

typedef struct ColorVertex {
    int32_t x, y;
    uint32_t color;
} ColorVertex;

/* Emit the previous inside vertex before the intersection. Clip geometry against the left,
 * right, and top bounds; apply the bottom bound during scanline traversal. Division and
 * multiplication use the selected x87 precision before truncating coordinate and color deltas. */
static ColorVertex intersect(ColorVertex a, ColorVertex b, int axis, int32_t bound) {
    ColorVertex out;
    int32_t av = axis ? a.y : a.x, bv = axis ? b.y : b.x;
    /* Original: 0x10037D8E..0x10037DB9.
     * Integer subtraction wraps before conversion to floating point. */
    OkgfEdgeFloat ratio =
        okgf_signed_ratio(okgf_sub32(bound, av), okgf_sub32(bv, av), okgf_get_math_precision());
    int32_t delta = axis ? okgf_sub32(b.x, a.x) : okgf_sub32(b.y, a.y);
    int32_t other =
        okgf_add32(axis ? a.x : a.y, okgf_truncate64(okgf_multiply_integer(ratio, delta), 0));
    out.x = axis ? other : bound;
    out.y = axis ? bound : other;
    out.color = 0;
    for (int k = 0; k < 4; ++k) {
        int ca = (a.color >> (8 * k)) & 255, cb = (b.color >> (8 * k)) & 255;
        int32_t change = okgf_truncate64(okgf_multiply_integer(ratio, cb - ca), 0);
        out.color |= (uint32_t)(uint8_t)okgf_add32(ca, change) << (8 * k);
    }
    return out;
}
static int clip_polygon(ColorVertex vertices[12], const OkgfRect *clip) {
    int32_t minx = vertices[0].x, maxx = minx, miny = vertices[0].y, maxy = miny;
    for (int i = 1; i < 3; ++i) {
        if (vertices[i].x < minx)
            minx = vertices[i].x;
        if (vertices[i].x > maxx)
            maxx = vertices[i].x;
        if (vertices[i].y < miny)
            miny = vertices[i].y;
        if (vertices[i].y > maxy)
            maxy = vertices[i].y;
    }
    if (clip->left >= maxx || clip->right <= minx || clip->top >= maxy || clip->bottom <= miny)
        return 0;
    int needed[3] = {(minx < clip->left), (maxx > clip->right), (miny < clip->top)};
    if (!needed[0] && !needed[1] && !needed[2])
        return 3;
    int32_t bounds[3] = {clip->left, clip->right, clip->top};
    int count = 3;
    for (int side = 0; side < 3 && count; ++side) {
        if (!needed[side])
            continue;
        ColorVertex result[12];
        int n = 0;
        for (int i = 0; i < count; ++i) {
            ColorVertex a = vertices[(i + count - 1) % count], b = vertices[i];
            int32_t av = side == 2 ? a.y : a.x, bv = side == 2 ? b.y : b.x;
            int ai = side == 1 ? av <= bounds[side] : av >= bounds[side];
            int bi = side == 1 ? bv <= bounds[side] : bv >= bounds[side];
            if (ai)
                result[n++] = a;
            if (ai != bi)
                result[n++] = intersect(a, b, side == 2, bounds[side]);
        }
        memcpy(vertices, result, n * sizeof(*vertices));
        count = n;
    }
    return count;
}

typedef struct ColorEdge {
    int index, next, direction;
    int32_t x, dx, c[4], dc[4];
} ColorEdge;
static void edge_segment(ColorEdge *edge, const ColorVertex *v, int count) {
    int from = edge->index, to = from;
    do {
        from = to;
        to = (to + count + edge->direction) % count;
    } while (v[from].y == v[to].y);
    edge->index = from;
    edge->next = to;
    int32_t height = v[to].y - v[from].y;
    /* SHL wraps before signed IDIV, even for negative coordinates. */
    edge->x = (int32_t)((uint32_t)v[from].x << 16);
    edge->dx = (int32_t)(((uint32_t)v[to].x - (uint32_t)v[from].x) << 16) / height;
    channels(edge->c, v[from].color);
    int32_t end[4];
    channels(end, v[to].color);
    for (int k = 0; k < 4; ++k)
        edge->dc[k] = (end[k] - edge->c[k]) / height;
}
void OKGF_CALL OKGF_Triangle_16(void *pixels, int32_t pitch, int32_t x1, int32_t y1,
                                uint32_t color1, int32_t x2, int32_t y2, uint32_t color2,
                                int32_t x3, int32_t y3, uint32_t color3, const OkgfRect *clip) {
    ColorVertex v[12] = {{x1, y1, color1}, {x2, y2, color2}, {x3, y3, color3}};
    OkgfRect bounds;
    memcpy(&bounds, clip, sizeof(bounds));
    int count = clip_polygon(v, &bounds);
    if (count < 2 || !((color1 | color2 | color3) & 0xff000000))
        return;
    int top = 0;
    int32_t bottom = v[0].y;
    for (int i = 1; i < count; ++i) {
        if (v[i].y < v[top].y)
            top = i;
        if (v[i].y > bottom)
            bottom = v[i].y;
    }
    if (bottom <= v[top].y)
        return;
    if (bottom > bounds.bottom)
        bottom = bounds.bottom;
    ColorEdge backward = {.index = top, .direction = -1};
    ColorEdge forward = {.index = top, .direction = 1};
    edge_segment(&backward, v, count);
    edge_segment(&forward, v, count);
    unsigned mode = alpha_mode(color1, color2, color3);
    int32_t constant[4];
    channels(constant, color1);
    uint32_t different = (color1 ^ color2) | (color1 ^ color3);
    for (int32_t y = v[top].y; y < bottom; ++y) {
        ColorEdge *edges[2] = {&backward, &forward};
        for (int e = 0; e < 2; ++e) {
            if (y >= v[edges[e]->next].y) {
                edges[e]->index = edges[e]->next;
                edge_segment(edges[e], v, count);
            }
        }
        ColorEdge *left = backward.x < forward.x ? &backward : &forward;
        ColorEdge *right = left == &backward ? &forward : &backward;
        int32_t x = left->x >> 16, length = (right->x >> 16) - x;
        if (length > 0) {
            int32_t c[4], dc[4];
            for (int k = 0; k < 4; ++k) {
                int varying = k == 3 || ((different >> (8 * k)) & 255);
                c[k] = varying ? left->c[k] : constant[k];
                dc[k] = varying ? (right->c[k] - left->c[k]) / length : 0;
            }
            uint8_t *p = (uint8_t *)pixels + (ptrdiff_t)y * pitch + (ptrdiff_t)x * 2;
            for (int32_t i = 0; i < length; ++i, p += 2) {
                shade(p, c, mode);
                for (int k = 0; k < 4; ++k)
                    c[k] += dc[k];
            }
        }
        for (int e = 0; e < 2; ++e) {
            edges[e]->x = (int32_t)((uint32_t)edges[e]->x + (uint32_t)edges[e]->dx);
            for (int k = 0; k < 4; ++k)
                edges[e]->c[k] += edges[e]->dc[k];
        }
    }
}
