#include "image_io.hpp"

using ui64 = uint64_t;
using ui32 = uint32_t;
using ui16 = uint16_t;
using ui8  = uint8_t;
using i64  = int64_t;
using i32  = int32_t;
using i16  = int16_t;

constexpr ui32 MAXVAL = 65535U;

static ui64 _u64d_div__(ui64 b, ui64 c) { return b / c; }

static ui32 _u32d_div__(ui32 b, ui32 c) { return b / c; }

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
  i32 mul(ui32 v) const { return (val * v) >> rshift; }
};

/**
 * @brief Calculate cubic root of input
 *
 * @param N input
 * @return cubic root of N
 */
static i32 cbrt_fix(i32 N) {
  ui32 numerator;
  mat_coeff k1_3(21845U, 2);  // = 1/3 * 2^16
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
  numerator = k1_3.mul(N);
  for (i32 i = 0; i < 5; ++i) {
    ui64 x_n_partial;
    ui32 x_squared;
    x_squared = ((static_cast<ui32>(x_n) * x_n) >> 2) + eps;
    if (x_squared == 0UL) {
      x_n_partial = MAXVAL;
    } else {
      x_n_partial = _u64d_div__(static_cast<ui64>(numerator) << 30, static_cast<ui64>(x_squared));
      // x_n_partial = _u32d_div__(numerator << 14, x_squared >> 16);
    }
    x_n_1 =
        (((static_cast<ui32>(x_n) << 14) - k1_3.mul(x_n)) + ((static_cast<ui32>(x_n_partial) >> 0) << 0))
        >> 14;
    // x_n_1 =
    //     (((static_cast<ui32>(x_n) << 14) - k1_3.mul(x_n)) + (static_cast<ui32>(x_n_partial) << 16)) >>
    //     14;
    x_n = x_n_1;
  }
  return x_n_1;
}

void rgb2xyb(image &rgb_in, image &xyb_out) {
  // Set matrix coefficients
  const mat_coeff T00(4915U, 0);     // 0.3 << 14
  const mat_coeff T01(40763U, 2);    // 0.622 << 16
  const mat_coeff T02(5111U, 2);     // 0.078 << 16
  const mat_coeff T10(15073U, 2);    // 0.23 << 16
  const mat_coeff T11(22675U, 1);    // 0.692 << 15
  const mat_coeff T12(5111U, 2);     // 0.078 << 16
  const mat_coeff T20(3988U, 0);     // 0.24342268924547819 << 14
  const mat_coeff T21(13419U, 2);    // 0.20476744424496821 << 16
  const mat_coeff T22(36163U, 2);    // 0.55180986650955360 << 16
  const i32 bias      = -4079616;    // / 2^30 = -0.003799438476562
  const i32 bias_cbrt = -167460864;  // / 2^30 = -0.155960083007812

  const ui32 width  = rgb_in.get_width();
  const ui32 height = rgb_in.get_height();

  if (rgb_in.get_num_components() != 3) {
    printf("Number of components shall be 3!\n");
    exit(EXIT_FAILURE);
  }

  i32 *buf_red, *buf_grn, *buf_blu;
  buf_red = rgb_in.get_buf(0);
  buf_grn = rgb_in.get_buf(1);
  buf_blu = rgb_in.get_buf(2);

  i32 *buf_X, *buf_Y, *buf_B;
  buf_X = xyb_out.get_buf(0);
  buf_Y = xyb_out.get_buf(1);
  buf_B = xyb_out.get_buf(2);

  const i32 bpp = rgb_in.get_max_bpp();
  i32 Lmix, Mmix, Smix;
  i32 Lgamma, Mgamma, Sgamma;

  /**
   * @brief Scale input pixel value into Q16 format
   *
   * @param val input
   * @return Q16 foramt of input
   */
  auto scale_pixel_value = [](i32 val, i32 bpp) { return static_cast<ui32>(val) << (16 - bpp); };

  i32 r, g, b;  // RGB inputs are limited in 16bpp, ui16(0-65535)
  i32 X, Y, B;
  size_t idx        = 0;
  const auto stride = width;
  for (ui32 y = 0; y < height; ++y) {
    idx = y * stride;
    for (ui32 x = 0; x < width; ++x) {
      r = scale_pixel_value(buf_red[idx], bpp);
      g = scale_pixel_value(buf_grn[idx], bpp);
      b = scale_pixel_value(buf_blu[idx], bpp);

      Lmix = (T00.mul(r) + T01.mul(g) + T02.mul(b) - bias) >> 14;
      Mmix = (T10.mul(r) + T11.mul(g) + T12.mul(b) - bias) >> 14;
      Smix = (T20.mul(r) + T21.mul(g) + T22.mul(b) - bias) >> 14;

      // Limit _mix values to prevent overlow
      Lmix = Lmix > 65535 ? 65535 : Lmix;
      Mmix = Mmix > 65535 ? 65535 : Mmix;
      Smix = Smix > 65535 ? 65535 : Smix;

      Lgamma = cbrt_fix(Lmix);
      Mgamma = cbrt_fix(Mmix);
      Sgamma = ((static_cast<ui32>(cbrt_fix(Smix)) << 14) + bias_cbrt) >> 14;

      // X = (Lgamma - Mgamma) / 2;
      X = (Lgamma - Mgamma) >> 1;
      // Y = (Lgamma + Mgamma) / 2;
      Y = static_cast<i32>((static_cast<ui32>(Lgamma + Mgamma) << 14));
      Y += bias_cbrt * 2;
      Y >>= 15;

      // B = Sgamma
      B = Sgamma;

      buf_X[idx] = X;
      buf_Y[idx] = Y;
      buf_B[idx] = B;

      idx++;
    }
  }
}

// Lgamma = cbrt_fix(
//     static_cast<ui16>((static_cast<i32>((4915U * r + ((40763U * g) >> 2)) + Lmix_tmp) - bias) >>
//     14));
// Mgamma = cbrt_fix(static_cast<ui16>(
//     (static_cast<i32>((((15073U * r) >> 2) + ((22675U * g) >> 1)) + Lmix_tmp) - bias) >> 14));
// Sgamma = static_cast<ui16>(
//     ((cbrt_fix(static_cast<ui16>(
//           (static_cast<i32>((3988U * r + ((13419U * g) >> 2)) + ((36163U * b) >> 2)) - bias) >>
//           14))
//       << 14)
//      + bias_cbrt)
//     >> 14);
// // buf_X[idx] = static_cast<i16>((static_cast<i64>((Lgamma << 14) - (Mgamma << 14)) << 1) >> 16);
// buf_X[idx] = static_cast<i16>(((Lgamma << 14) - (Mgamma << 14)) >> 15);
// buf_X[idx] = saturate(buf_X[idx], 1024);
// // buf_Y[idx] = static_cast<ui16>((static_cast<i64>(static_cast<i32>((static_cast<ui32>(Lgamma) << 14)
// //                                                               + (static_cast<ui32>(Mgamma) << 14))
// //                                              + bias_cbrt * 2)
// //                             << 1)
// //                            >> 16);