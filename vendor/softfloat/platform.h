#ifndef OKGF_SOFTFLOAT_PLATFORM_H
#define OKGF_SOFTFLOAT_PLATFORM_H

#if defined(_WIN32) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define LITTLEENDIAN 1
#elif !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
#error Define the target byte order for SoftFloat
#endif

/* Plain C11 inline definitions need external bodies at -O0; keep these local. */
#define INLINE static inline
#ifdef _MSC_VER
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined(OKGF_PORTABLE_MATH)
#define SOFTFLOAT_BUILTIN_CLZ 1
#if defined(__SIZEOF_INT128__)
#define SOFTFLOAT_INTRINSIC_INT128 1
#endif
#include "opts-GCC.h"
#endif

#endif
