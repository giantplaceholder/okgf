#include "okgf.h"

#ifdef _MSC_VER
static __declspec(thread) OkgfMathPrecision math_precision;
#else
static _Thread_local OkgfMathPrecision math_precision;
#endif

void OKGF_CALL okgf_set_math_precision(OkgfMathPrecision precision) {
    if (precision == OKGF_MATH_AUTO || precision == OKGF_MATH_GAME ||
        precision == OKGF_MATH_DOUBLE || precision == OKGF_MATH_EXTENDED)
        math_precision = precision;
}

OkgfMathPrecision OKGF_CALL okgf_get_math_precision(void) {
    if (math_precision != OKGF_MATH_AUTO)
        return math_precision;
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
    unsigned short control;
    __asm__ volatile("fnstcw %0" : "=m"(control));
    return (control & 0x300) == 0       ? OKGF_MATH_GAME
           : (control & 0x300) == 0x200 ? OKGF_MATH_DOUBLE
                                        : OKGF_MATH_EXTENDED;
#elif defined(_MSC_VER) && defined(_M_IX86)
    unsigned short control;
    __asm { fnstcw control }
    return (control & 0x300) == 0       ? OKGF_MATH_GAME
           : (control & 0x300) == 0x200 ? OKGF_MATH_DOUBLE
                                        : OKGF_MATH_EXTENDED;
#else
    return OKGF_MATH_GAME;
#endif
}
