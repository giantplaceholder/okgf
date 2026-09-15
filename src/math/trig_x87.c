#include "math/backend.h"

#ifdef OKGF_NATIVE_X87_TRIG

/* Evaluate FSIN/FCOS using this processor's x87 instructions. Temporarily mask exceptions and
 * select extended precision with round-to-nearest, then restore the caller's control word. */
void okgf_x87_sin(OkgfFloat out, const OkgfFloat value) {
    extFloat80_t input = *value, result = {0};
    unsigned short saved, control;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm { fnstcw saved }
    control = (unsigned short)((saved & ~0x0f00u) | 0x033fu);
    __asm {
        fldcw control
        fld tbyte ptr input
        fsin
        fstp tbyte ptr result
        fldcw saved
    }
#else
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control = (unsigned short)((saved & ~0x0f00u) | 0x033fu);
    __asm__ volatile("fldcw %2; fldt %1; fsin; fstpt %0; fldcw %3"
                     : "=m"(result)
                     : "m"(input), "m"(control), "m"(saved)
                     : "st");
#endif
    *out = result;
}

void okgf_x87_cos(OkgfFloat out, const OkgfFloat value) {
    extFloat80_t input = *value, result = {0};
    unsigned short saved, control;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm { fnstcw saved }
    control = (unsigned short)((saved & ~0x0f00u) | 0x033fu);
    __asm {
        fldcw control
        fld tbyte ptr input
        fcos
        fstp tbyte ptr result
        fldcw saved
    }
#else
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control = (unsigned short)((saved & ~0x0f00u) | 0x033fu);
    __asm__ volatile("fldcw %2; fldt %1; fcos; fstpt %0; fldcw %3"
                     : "=m"(result)
                     : "m"(input), "m"(control), "m"(saved)
                     : "st");
#endif
    *out = result;
}

#endif
