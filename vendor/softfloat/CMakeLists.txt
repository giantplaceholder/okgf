# Berkeley SoftFloat Release 3e; see COPYING.txt and README.md.
add_library(okgf_softfloat STATIC
  source/extF80_add.c
  source/extF80_div.c
  source/extF80_lt.c
  source/extF80_mul.c
  source/extF80_sub.c
  source/extF80_to_f128.c
  source/extF80_to_f64.c
  source/extF80_to_i64.c
  source/f128_mul.c
  source/f128_mulAdd.c
  source/f128_sub.c
  source/f128_to_extF80.c
  source/f64_to_extF80.c
  source/s_add128.c
  source/s_add256M.c
  source/s_addMagsExtF80.c
  source/s_addMagsF128.c
  source/s_approxRecip32_1.c
  source/s_approxRecipSqrt32_1.c
  source/s_approxRecipSqrt_1Ks.c
  source/s_approxRecip_1Ks.c
  source/8086-SSE/s_commonNaNToExtF80UI.c
  source/8086-SSE/s_commonNaNToF128UI.c
  source/8086-SSE/s_commonNaNToF64UI.c
  source/s_countLeadingZeros16.c
  source/s_countLeadingZeros32.c
  source/s_countLeadingZeros64.c
  source/s_countLeadingZeros8.c
  source/s_eq128.c
  source/8086-SSE/s_extF80UIToCommonNaN.c
  source/8086-SSE/s_f128UIToCommonNaN.c
  source/8086-SSE/s_f64UIToCommonNaN.c
  source/s_le128.c
  source/s_lt128.c
  source/s_mul128By32.c
  source/s_mul128To256M.c
  source/s_mul64ByShifted32To128.c
  source/s_mul64To128.c
  source/s_mulAddF128.c
  source/s_normRoundPackToExtF80.c
  source/s_normRoundPackToF128.c
  source/s_normSubnormalExtF80Sig.c
  source/s_normSubnormalF128Sig.c
  source/s_normSubnormalF64Sig.c
  source/8086-SSE/s_propagateNaNExtF80UI.c
  source/8086-SSE/s_propagateNaNF128UI.c
  source/s_roundPackToExtF80.c
  source/s_roundPackToF128.c
  source/s_roundPackToF64.c
  source/s_roundToI64.c
  source/s_shiftRightJam128.c
  source/s_shiftRightJam128Extra.c
  source/s_shiftRightJam256M.c
  source/s_shiftRightJam32.c
  source/s_shiftRightJam64.c
  source/s_shiftRightJam64Extra.c
  source/s_shortShiftLeft128.c
  source/s_shortShiftRight128.c
  source/s_shortShiftRightJam128.c
  source/s_shortShiftRightJam128Extra.c
  source/s_shortShiftRightJam64.c
  source/s_shortShiftRightJam64Extra.c
  source/s_sub128.c
  source/s_sub256M.c
  source/s_subMagsExtF80.c
  source/s_subMagsF128.c
  source/8086-SSE/softfloat_raiseFlags.c
  source/softfloat_state.c
)
set_target_properties(okgf_softfloat PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden)
target_compile_features(okgf_softfloat PUBLIC c_std_11)
target_include_directories(okgf_softfloat SYSTEM PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/include")
target_include_directories(okgf_softfloat PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/source/8086-SSE")
target_compile_definitions(okgf_softfloat PUBLIC SOFTFLOAT_FAST_INT64
  PRIVATE INLINE_LEVEL=5 SOFTFLOAT_FAST_DIV64TO32)
if(OKGF_PORTABLE_MATH)
  target_compile_definitions(okgf_softfloat PUBLIC OKGF_PORTABLE_MATH=1)
endif()
