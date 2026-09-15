#include "okgf_platform.h"
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
extern const OkgfPlatform okgf_win32_platform;
#endif
static _Thread_local const OkgfPlatform *thread_platform;
void OKGF_CALL okgf_set_platform(const OkgfPlatform *platform) {
    thread_platform = platform;
}
static const OkgfPlatform *platform_api(void) {
    if (thread_platform)
        return thread_platform;
#ifdef _WIN32
    return &okgf_win32_platform;
#else
    return NULL;
#endif
}

void OKGF_CALL OKGR_StretchGdi_WORD(void *dest, uint32_t width, uint32_t height, const void *source,
                                    uint32_t source_width, uint32_t source_height) {
    const OkgfPlatform *api = platform_api();
    if (!api)
        return;
    uintptr_t screen = api->get_dc(0);
    uintptr_t source_dc = api->create_compatible_dc(screen);
    uintptr_t dest_dc = api->create_compatible_dc(screen);
    uintptr_t source_bitmap =
        api->create_bitmap((int32_t)source_width, (int32_t)source_height, 1, 16, source);
    uintptr_t dest_bitmap = 0;
    if (!source_bitmap)
        goto cleanup;
    dest_bitmap = api->create_bitmap((int32_t)width, (int32_t)height, 1, 16, NULL);
    if (!dest_bitmap)
        goto cleanup;
    api->select_object(source_dc, source_bitmap);
    api->select_object(dest_dc, dest_bitmap);
    api->set_stretch_mode(source_dc, 4);
    api->set_stretch_mode(dest_dc, 4);
    api->stretch_blt(dest_dc, 0, 0, (int32_t)width, (int32_t)height, source_dc, 0, 0,
                     (int32_t)source_width, (int32_t)source_height, 0x00cc0020);
    api->get_bitmap_bits(dest_bitmap, (int32_t)(2u * height * width), dest);
cleanup:
    if (source_bitmap)
        api->delete_object(source_bitmap);
    if (dest_bitmap)
        api->delete_object(dest_bitmap);
    if (screen)
        api->release_dc(0, screen);
    if (source_dc)
        api->delete_dc(source_dc);
    if (dest_dc)
        api->delete_dc(dest_dc);
}

#define VERSION(a, b, c, d) ((uint64_t)(a) << 48 | (uint64_t)(b) << 32 | (uint64_t)(c) << 16 | (d))
static int version_at(const OkgfPlatform *api, const char *directory, const char *name,
                      uint64_t *version) {
    char path[512];
    size_t length = strlen(directory);
    memcpy(path, directory, length);
    strcpy(path + length, name);
    return api->file_version(path, version) >= 0;
}
static void file_versions(const OkgfPlatform *api, uint32_t *major, uint32_t *minor, char *suffix) {
    char directory[512] = {0};
    uint64_t v;
    int found = 0;
#define SET_VERSION(a, b, c) (*major = (a), *minor = (b), *suffix = (c), found = 1)
    if (!api->system_directory(directory, 260))
        goto none;
    directory[259] = 0;
    if (version_at(api, directory, "\\ddraw.dll", &v)) {
        if (v >= VERSION(4, 2, 0, 95))
            SET_VERSION(1, 0, ' ');
        if (v >= VERSION(4, 3, 0, 1096))
            SET_VERSION(2, 0, ' ');
        if (v >= VERSION(4, 4, 0, 68))
            SET_VERSION(3, 0, ' ');
    }
    if (version_at(api, directory, "\\d3drg8x.dll", &v) && v >= VERSION(4, 4, 0, 70))
        SET_VERSION(3, 0, 'a');
    if (version_at(api, directory, "\\ddraw.dll", &v)) {
        if (v >= VERSION(4, 5, 0, 155))
            SET_VERSION(5, 0, ' ');
        if (v >= VERSION(4, 6, 0, 318))
            SET_VERSION(6, 0, ' ');
        if (v >= VERSION(4, 6, 0, 436))
            SET_VERSION(6, 1, ' ');
    }
    if (version_at(api, directory, "\\dplayx.dll", &v) && v >= VERSION(4, 6, 3, 518))
        SET_VERSION(6, 1, 'a');
    if (version_at(api, directory, "\\ddraw.dll", &v) && v >= VERSION(4, 7, 0, 700))
        SET_VERSION(7, 0, ' ');
    if (version_at(api, directory, "\\dinput.dll", &v) && v >= VERSION(4, 7, 0, 716))
        SET_VERSION(7, 0, 'a');
    if (version_at(api, directory, "\\ddraw.dll", &v) &&
        ((v >> 48 == 4 && v >= VERSION(4, 8, 0, 400)) ||
         (v >> 48 == 5 && v >= VERSION(5, 1, 2258, 400))))
        SET_VERSION(8, 0, ' ');
    if (version_at(api, directory, "\\d3d8.dll", &v)) {
        if ((v >> 48 == 4 && v >= VERSION(4, 8, 1, 881)) ||
            (v >> 48 == 5 && v >= VERSION(5, 1, 2600, 881)))
            SET_VERSION(8, 1, ' ');
        if ((v >> 48 == 4 && v >= VERSION(4, 8, 1, 901)) ||
            (v >> 48 == 5 && v >= VERSION(5, 1, 2600, 901)))
            SET_VERSION(8, 1, 'a');
    }
    if (version_at(api, directory, "\\mpg2splt.ax", &v) && v >= VERSION(6, 3, 1, 885))
        SET_VERSION(8, 1, 'b');
    if (version_at(api, directory, "\\dpnet.dll", &v) &&
        ((v >> 48 == 4 && v >= VERSION(4, 9, 0, 134)) ||
         (v >> 48 == 5 && v >= VERSION(5, 2, 3677, 134))))
        SET_VERSION(8, 2, ' ');
    if (version_at(api, directory, "\\d3d9.dll", &v))
        SET_VERSION(9, 0, ' '); /* A readable version is sufficient; its value is not compared with
                                   a threshold. */
none:
    if (!found) {
        *major = *minor = 0;
        *suffix = ' ';
    }
#undef SET_VERSION
}
uint32_t OKGF_CALL DXVersion(void) {
    const OkgfPlatform *api = platform_api();
    if (!api)
        return 0;
    uint32_t major = 0, minor = 0;
    char suffix = ' ';
    if (api->dxdiag_version(&major, &minor, &suffix) < 0)
        file_versions(api, &major, &minor, &suffix);
    unsigned char letter = (unsigned char)suffix;
    if (letter >= 'A' && letter <= 'Z')
        letter += 'a' - 'A';
    uint32_t result = (minor + (major << 8)) << 8;
    if (letter >= 'a' && letter <= 'z')
        result += letter - 'a' + 1;
    return result;
}
