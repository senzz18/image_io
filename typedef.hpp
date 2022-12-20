//
// Created by OSAMU WATANABE on 2022/12/20.
//

#ifndef IMAGE_IO_TEST_TYPEDEF_HPP
#define IMAGE_IO_TEST_TYPEDEF_HPP

#include <arm_neon.h>
#include <cstdint>
using ui64 = uint64_t;
using ui32 = uint32_t;
using ui16 = uint16_t;
using ui8  = uint8_t;
using i64  = int64_t;
using i32  = int32_t;
using i16  = int16_t;

/**
 * @brief Saturate a value with a limit
 *
 * @tparam T
 * @param val value
 * @param limit limit
 * @return T saturated output
 */
template <class T>
static T saturate(T val, T limit) {
  assert(limit != 0);
  T ret;
  if ((val & limit) != 0) {
    ret = val | -limit;
  } else {
    ret = val & (limit - 1);
  }
  return ret;
}

class mat_coeff {
 public:
  const ui32 val;
  const i32 rshift;
  mat_coeff() : val(0), rshift(0) {}
  explicit mat_coeff(ui32 v, ui32 rs) : val(v), rshift(rs) {}
  inline i32 mul(ui32 v) const { return (val * v) >> rshift; }
};

class mat_coeff_neon {
 public:
  const uint32x4_t val;
  const int32x4_t rshift;
  // mat_coeff() : val(0), rshift(0) {}
  explicit mat_coeff_neon(ui32 v, ui32 rs) : val(vdupq_n_u32(v)), rshift(vdupq_n_s32(rs)) {}
  inline int32x4_t mul(uint32x4_t v) const { return vshlq_u32(vmulq_u32(val, v), -rshift); }
};

#endif  // IMAGE_IO_TEST_TYPEDEF_HPP
