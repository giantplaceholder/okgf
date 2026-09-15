#ifdef _WIN32
#define COBJMACROS
#define INITGUID
#include "okgf_platform.h"
#include <stdlib.h>
#include <windows.h>

/* DxDiag declarations depend on Windows COM types. */
#include <dxdiag.h>

static uintptr_t get_dc(uintptr_t window) {
    return (uintptr_t)GetDC((HWND)window);
}
static uintptr_t compatible_dc(uintptr_t dc) {
    return (uintptr_t)CreateCompatibleDC((HDC)dc);
}
static uintptr_t bitmap(int32_t w, int32_t h, uint32_t planes, uint32_t bits, const void *data) {
    return (uintptr_t)CreateBitmap(w, h, planes, bits, data);
}
static uintptr_t select_object(uintptr_t dc, uintptr_t object) {
    return (uintptr_t)SelectObject((HDC)dc, (HGDIOBJ)object);
}
static int32_t stretch_mode(uintptr_t dc, int32_t mode) {
    return SetStretchBltMode((HDC)dc, mode);
}
static int32_t stretch(uintptr_t dest, int32_t x, int32_t y, int32_t w, int32_t h, uintptr_t source,
                       int32_t sx, int32_t sy, int32_t sw, int32_t sh, uint32_t rop) {
    return StretchBlt((HDC)dest, x, y, w, h, (HDC)source, sx, sy, sw, sh, rop);
}
static int32_t bitmap_bits(uintptr_t bitmap_handle, int32_t count, void *pixels) {
    return GetBitmapBits((HBITMAP)bitmap_handle, count, pixels);
}
static int32_t delete_object(uintptr_t object) {
    return DeleteObject((HGDIOBJ)object);
}
static int32_t release_dc(uintptr_t window, uintptr_t dc) {
    return ReleaseDC((HWND)window, (HDC)dc);
}
static int32_t delete_dc(uintptr_t dc) {
    return DeleteDC((HDC)dc);
}
static uint32_t system_directory(char *path, uint32_t capacity) {
    return GetSystemDirectoryA(path, capacity);
}
static int32_t file_version(const char *path, uint64_t *version) {
    DWORD ignored;
    DWORD size = GetFileVersionInfoSizeA(path, &ignored);
    if (!size)
        return (int32_t)E_FAIL;
    void *data = malloc(size);
    if (!data)
        return (int32_t)E_OUTOFMEMORY;
    VS_FIXEDFILEINFO *info = NULL;
    UINT length;
    int32_t result = (int32_t)E_FAIL;
    if (GetFileVersionInfoA(path, 0, size, data) &&
        VerQueryValueA(data, "\\", (void **)&info, &length) && info) {
        *version = (uint64_t)info->dwFileVersionMS << 32 | info->dwFileVersionLS;
        result = 0;
    }
    free(data);
    return result;
}
static int32_t dxdiag(uint32_t *major, uint32_t *minor, char *suffix) {
    int initialized = SUCCEEDED(CoInitialize(NULL));
    IDxDiagProvider *provider = NULL;
    IDxDiagContainer *root = NULL, *system = NULL;
    int have_major = 0, have_minor = 0, have_suffix = 0;
    if (FAILED(CoCreateInstance(&CLSID_DxDiagProvider, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IDxDiagProvider, (void **)&provider)))
        goto cleanup;
    DXDIAG_INIT_PARAMS params = {sizeof(params), 111, FALSE, NULL};
    if (FAILED(IDxDiagProvider_Initialize(provider, &params)) ||
        FAILED(IDxDiagProvider_GetRootContainer(provider, &root)) ||
        FAILED(IDxDiagContainer_GetChildContainer(root, L"DxDiag_SystemInfo", &system)))
        goto cleanup;
    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(IDxDiagContainer_GetProp(system, L"dwDirectXVersionMajor", &value)) &&
        V_VT(&value) == VT_UI4) {
        *major = V_UI4(&value);
        have_major = 1;
    }
    VariantClear(&value);
    if (SUCCEEDED(IDxDiagContainer_GetProp(system, L"dwDirectXVersionMinor", &value)) &&
        V_VT(&value) == VT_UI4) {
        *minor = V_UI4(&value);
        have_minor = 1;
    }
    VariantClear(&value);
    if (SUCCEEDED(IDxDiagContainer_GetProp(system, L"szDirectXVersionLetter", &value)) &&
        V_VT(&value) == VT_BSTR && V_BSTR(&value)) {
        char text[10] = {0};
        WideCharToMultiByte(CP_ACP, 0, V_BSTR(&value), -1, text, 10, NULL, NULL);
        *suffix = text[0];
        have_suffix = 1;
    }
    VariantClear(&value);
cleanup:
    if (system)
        IDxDiagContainer_Release(system);
    if (root)
        IDxDiagContainer_Release(root);
    if (provider)
        IDxDiagProvider_Release(provider);
    if (initialized)
        CoUninitialize();
    return have_major && have_minor && have_suffix ? 0 : (int32_t)E_FAIL;
}
const OkgfPlatform okgf_win32_platform = {
    get_dc,  compatible_dc,    bitmap,        select_object, stretch_mode,
    stretch, bitmap_bits,      delete_object, release_dc,    delete_dc,
    dxdiag,  system_directory, file_version};
#endif
