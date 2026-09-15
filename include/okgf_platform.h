#ifndef OKGF_PLATFORM_H
#define OKGF_PLATFORM_H
#include "okgf.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Platform callbacks for Windows services. All callbacks are required and use cdecl; handles
 * are pointer-sized. The table is borrowed per thread until reset with NULL. A NULL table
 * selects the Windows APIs on Windows. On other hosts, DXVersion returns 0 and GDI operations
 * leave output unchanged until a backend is supplied. */
typedef struct OkgfPlatform {
    uintptr_t (*get_dc)(uintptr_t window);
    uintptr_t (*create_compatible_dc)(uintptr_t dc);
    uintptr_t (*create_bitmap)(int32_t width, int32_t height, uint32_t planes, uint32_t bits,
                               const void *pixels);
    uintptr_t (*select_object)(uintptr_t dc, uintptr_t object);
    int32_t (*set_stretch_mode)(uintptr_t dc, int32_t mode);
    int32_t (*stretch_blt)(uintptr_t dest, int32_t x, int32_t y, int32_t width, int32_t height,
                           uintptr_t source, int32_t sx, int32_t sy, int32_t sw, int32_t sh,
                           uint32_t operation);
    int32_t (*get_bitmap_bits)(uintptr_t bitmap, int32_t count, void *pixels);
    int32_t (*delete_object)(uintptr_t object);
    int32_t (*release_dc)(uintptr_t window, uintptr_t dc);
    int32_t (*delete_dc)(uintptr_t dc);
    int32_t (*dxdiag_version)(uint32_t *major, uint32_t *minor, char *suffix);
    uint32_t (*system_directory)(char *path, uint32_t capacity);
    int32_t (*file_version)(const char *path, uint64_t *version);
} OkgfPlatform;
void OKGF_CALL okgf_set_platform(const OkgfPlatform *platform);
#ifdef __cplusplus
}
#endif
#endif
