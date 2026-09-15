#define _POSIX_C_SOURCE 200809L
#include "okgf.h"
#include "math/backend.h"
#include <stdatomic.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

static atomic_int mmx = -1;
static atomic_flag probed = ATOMIC_FLAG_INIT;
static volatile uint8_t timing_sink;

int32_t OKGF_CALL OKGF_IsMMX(void) {
    int cached = atomic_load_explicit(&mmx, memory_order_relaxed);
    if (cached >= 0)
        return cached;
    uint32_t model = 0, features = 0;
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
    unsigned a, b, c, d;
    if (__get_cpuid(1, &a, &b, &c, &d)) {
        model = a & 0xff0;
        features = d;
    }
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    int registers[4];
    __cpuid(registers, 0);
    if (registers[0] >= 1) {
        __cpuid(registers, 1);
        model = (uint32_t)registers[0] & 0xff0;
        features = (uint32_t)registers[3];
    }
#endif
    /* Original: 0x10060363..0x10060387.
     * The MMX check also accepts these three family/model signatures. */
    cached = model == 0x630 || model == 0x650 || model == 0x660 || (features & 0x800000) != 0;
    atomic_store_explicit(&mmx, cached, memory_order_relaxed);
    return cached;
}

static int64_t counter(void) {
#ifdef _WIN32
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value))
        return 0;
    return (int64_t)value.tv_sec * INT64_C(1000000000) + value.tv_nsec;
#endif
}

uint32_t OKGF_CALL OKGF_GetCPUFeatures(void) {
    /* Only the first call reports feature bits. Later calls return zero, so at most one copy
     * operation can select the timed MMX path. */
    if (atomic_flag_test_and_set_explicit(&probed, memory_order_relaxed))
        return 0;
    uint32_t flags = (uint32_t)OKGF_IsMMX();
    volatile uint8_t *memory = malloc(3200);
    if (!memory)
        return flags;
    double frequency = 1e9;
#ifdef _WIN32
    LARGE_INTEGER f;
    if (QueryPerformanceFrequency(&f))
        frequency = (double)f.QuadPart;
#endif
    OkgfMathState saved = fp_enter();
    OkgfFloat elapsed;
    double seconds[2];
    for (int pass = 0; pass < 2; ++pass) {
#ifdef _WIN32
        Sleep(0);
#else
        sched_yield();
#endif
        int64_t start = counter();
        for (int i = pass * 50; i < (pass + 1) * 50; ++i) {
            memory[i * 32] = 0;
            if (pass)
                timing_sink = memory[i * 32];
        }
        fp_set(elapsed, (double)(counter() - start));
        fp_div_d(elapsed, elapsed, frequency);
        seconds[pass] = fp_double(elapsed);
    }
    free((void *)memory);
    if (seconds[0] < 1e-10) {
        flags |= 2;
    } else {
        fp_set(elapsed, seconds[1]);
        fp_sub_d(elapsed, elapsed, seconds[0]);
        fp_div_d(elapsed, elapsed, seconds[0]);
        fp_abs(elapsed);
        if (fp_compare(elapsed, 0.3) < 0)
            flags |= 2;
    }
    fp_leave(saved);
    return flags;
}
