#include "okgf.h"
#include "okgf_internal.h"
#include <stddef.h>

_Static_assert(sizeof(OkgfF5Header) == 16, "Serialized F5 header must be 16 bytes");
_Static_assert(sizeof(OkgfF6Header) == 4, "Serialized F6 header must be 4 bytes");

static void channel_shifts(const uint8_t *p, unsigned shifts[4]) {
    for (unsigned i = 0; i < 4; ++i) {
        unsigned precision = (p[i / 2] >> (4 * (i & 1))) & 15;
        /* The x86 variable shift uses only the low five count bits. */
        shifts[i] = (8u - precision) & 31;
    }
}

void OKGF_CALL OKGR_F5_DrawRGBA(void *dest, int32_t pitch, const OkgfF5Header *source) {
    const uint8_t *header = (const uint8_t *)source;
    const uint8_t *stream = header + 16 + (ptrdiff_t)4 * okgf_load32(header + 8);
    uint8_t *row = dest;
    ptrdiff_t offset = 0;
    unsigned shifts[4], channel = 0;
    channel_shifts(header + 12, shifts);
    for (;;) {
        unsigned command = *stream++;
        if (command & 128) {
            unsigned count = (command & 15) + 1;
            unsigned mode = (command >> 4) & 7;
            unsigned bits = 1u << (mode & 3);
            unsigned packed = 0, available = 0;
            for (unsigned i = 0; i < count; ++i) {
                if (!available) {
                    packed = *stream++;
                    available = 8;
                }
                unsigned delta = ((packed & ((1u << bits) - 1)) + 1) << shifts[channel];
                row[offset] = (uint8_t)(mode & 4 ? row[offset] - delta : row[offset] + delta);
                offset += 4;
                packed >>= bits;
                available -= bits;
            }
        } else if (!command) {
            if (++channel == 4) {
                channel = 0;
                row += pitch;
            }
            offset = channel;
        } else if (command == 63) {
            offset += 4 * (ptrdiff_t)okgf_load16(stream);
            stream += 2;
        } else if (command < 64) {
            offset += 4 * (ptrdiff_t)command;
        } else if (command == 64) {
            return;
        }
    }
}

typedef struct DeltaBits {
    const uint8_t *next;
    unsigned offset;
} DeltaBits;

/* Read bits from least to most significant within each byte. When a field crosses a byte
 * boundary, concatenate the fragments in the opposite order. For example, a 10-bit field at
 * offset zero is (source[0] << 2) | (source[1] & 3). */
static unsigned read_bits(DeltaBits *stream, unsigned count) {
    unsigned result = 0;
    while (count) {
        unsigned take = 8 - stream->offset;
        if (take > count)
            take = count;
        result = (result << take) | ((*stream->next >> stream->offset) & ((1u << take) - 1));
        stream->offset += take;
        if (stream->offset == 8) {
            stream->offset = 0;
            ++stream->next;
        }
        count -= take;
    }
    return result;
}

void OKGF_CALL OKGR_F6_DrawRGBA(void *dest, int32_t pitch, const OkgfF6Header *source) {
    const uint8_t *header = (const uint8_t *)source;
    unsigned shifts[4];
    channel_shifts(header, shifts);
    DeltaBits stream = {header + 4, 0};
    unsigned segments = okgf_load16(header + 2);
    for (unsigned segment = 0; segment < segments; ++segment) {
        unsigned x = read_bits(&stream, 10);
        unsigned y = read_bits(&stream, 10);
        ptrdiff_t row = (ptrdiff_t)y * pitch + 4 * (ptrdiff_t)x;
        for (;;) {
            unsigned bits = read_bits(&stream, 3);
            unsigned channel = read_bits(&stream, 2);
            if (!bits && channel == 0) {
                row += pitch;
                continue;
            }
            if (!bits && channel == 1)
                break;
            unsigned subtract = bits ? read_bits(&stream, 1) : 0;
            ptrdiff_t offset = row + (bits ? channel : 0);
            for (;;) {
                if (read_bits(&stream, 1)) {
                    uint8_t *pixel = (uint8_t *)dest + offset;
                    if (bits) {
                        unsigned delta = (read_bits(&stream, bits) + 1) << shifts[channel];
                        *pixel = (uint8_t)(subtract ? *pixel - delta : *pixel + delta);
                    } else {
                        pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
                    }
                    offset += 4;
                } else {
                    unsigned skip = read_bits(&stream, 3);
                    if (!skip)
                        break;
                    offset += 4 * (ptrdiff_t)skip;
                }
            }
        }
    }
}
