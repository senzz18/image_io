//
// Created by OSAMU WATANABE on 2022/12/19.
//

#ifndef IMAGE_IO_TEST_CBRT32_FIX_HPP
#define IMAGE_IO_TEST_CBRT32_FIX_HPP

#include "typedef.hpp"

constexpr ui32 MAXVAL = 65535U;

static ui64 _u64d_div__(ui64 b, ui64 c) { return b / c; }

static ui16 div64(ui32 a, ui32 b) {
  ui64 ret;
  if (b == 0U) {
    ret = UINT64_MAX;
  } else {
    ui64 u;
    if (b == 0UL) {
      u = UINT64_MAX;
    } else {
      u = _u64d_div__(static_cast<ui64>(a) << 30, static_cast<ui64>(b));
    }
    ret = u;
  }
  return static_cast<ui16>(static_cast<ui16>(ret >> 14) & 0x7FFFU);
}

template <class T>
T cbrt_fix(T N) {
  return N;
}

/**
 * @brief Calculate cubic root of input(unsigned 16 bit)
 *
 * @param N input
 * @return cubic root of N
 */
template <>
ui16 cbrt_fix<ui16>(ui16 N) {
  ui16 x_n;
  ui16 x_n_1;
  ui8 N_sqrt;
  N_sqrt = 0U;
  if (N > 0) {
    for (int i{7}; i >= 0; i--) {
      ui8 tmp;
      tmp = static_cast<unsigned char>(N_sqrt | 1 << i);
      if (((static_cast<ui32>(tmp) * tmp) << 14) <= (static_cast<ui32>(N) << 14)) {
        N_sqrt = tmp;
      }
    }
  }
  x_n = static_cast<ui16>(N_sqrt << 8);
  for (int b_i{0}; b_i < 5; b_i++) {
    ui32 x_squared = (static_cast<ui32>(x_n) * x_n) >> 2;
    ui64 x_partial = div64((21845U * N) >> 2, x_squared);
    ui32 tmp =
        ((static_cast<ui32>(x_n) << 14) - ((21845U * x_n) >> 2)) + (static_cast<ui32>(x_partial) << 14);
    tmp >>= 14;
    x_n_1 = static_cast<ui16>((tmp > 65535) ? 65535 : tmp);
    x_n   = x_n_1;
  }
  return x_n_1;
}

/**
 * @brief Calculate cubic root of input(32 bit)
 *
 * @param N input
 * @return cubic root of N
 */
template <>
i32 cbrt_fix<i32>(i32 N) {
  ui32 numerator;
  //  mat_coeff k1_3(21845U, 2);  // = 1/3 * 2^16
  i32 x_n, x_n_1;
  ui16 N_sqrt;
  const ui32 eps = 65536U;

  N_sqrt = 0U;
  if (N > 0) {
    for (int i{7}; i >= 0; i--) {
      ui8 tmp;
      tmp = static_cast<ui8>(N_sqrt | 1 << i);
      if (((static_cast<ui32>(tmp) * tmp) << 14) <= (static_cast<ui32>(N) << 14)) {
        N_sqrt = tmp;
      }
    }
  }
  x_n       = static_cast<i32>(N_sqrt << 8);
  numerator = (21845U * N) >> 2;
  for (i32 i = 0; i < 5; ++i) {
    ui64 x_n_partial;
    ui32 x_squared;
    x_squared = ((static_cast<ui32>(x_n) * x_n) >> 2) + eps;
    if (x_squared == 0UL) {
      x_n_partial = MAXVAL;
    } else {
      int bit = 16;
      // printf("%llu\n", static_cast<ui64>(x_squared) >> 16);
      x_n_partial =
          _u64d_div__(static_cast<ui64>(numerator) << (30 - bit), static_cast<ui64>(x_squared) >> bit);

      // x_n_partial = _u32d_div__(static_cast<ui64>(numerator) << 14, static_cast<ui64>(x_squared) >> 16);

      // x_n_partial = static_cast<ui64>(numerator) >> 14 * static_cast<ui64>(x_squared) >> 16;
    }
    x_n_1 =
        (((static_cast<ui32>(x_n) << 14) - ((21845U * x_n) >> 2)) + static_cast<ui32>(x_n_partial)) >> 14;
    // x_n_1 =
    //     (((static_cast<ui32>(x_n) << 14) - k1_3.mul(x_n)) + (static_cast<ui32>(x_n_partial) << 16)) >>
    //     14;
    x_n = x_n_1;
  }
  return x_n_1;
}

#endif  // IMAGE_IO_TEST_CBRT32_FIX_HPP
