#include "image_io.hpp"

using ui64 = uint64_t;
using ui32 = uint32_t;
using ui16 = uint16_t;
using ui8  = uint8_t;
using i64  = int64_t;
using i32  = int32_t;
using i16  = int16_t;

static ui64 _u64d_div__(ui64 b, ui64 c) { return b / c; }

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
static ui16 cbrt_fix(ui16 N) {
  ui32 a;
  ui16 x_n;
  ui16 x_n_1;
  ui8 N_sqrt;
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
  x_n = static_cast<ui16>(N_sqrt << 8);
  a   = (21845U * N) >> 2;
  for (i32 b_i{0}; b_i < 5; b_i++) {
    ui64 b0;
    ui32 x_squared;
    x_squared = ((static_cast<ui32>(x_n) * x_n) >> 2) + eps;
    if (x_squared == 0UL) {
      b0 = UINT64_MAX;
    } else {
      b0 = _u64d_div__(static_cast<ui64>(a) << 30, static_cast<ui64>(x_squared));
    }
    x_n_1 = static_cast<ui16>(
        (((static_cast<ui32>(x_n) << 14) - ((21845U * x_n) >> 2)) + static_cast<ui32>(b0)) >> 14);
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

  i32 *red, *grn, *blu;
  red = rgb_in.get_buf(0);
  grn = rgb_in.get_buf(1);
  blu = rgb_in.get_buf(2);

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
   * @return = floor(static_cast<double>(val) / 255.0 * 65536)
   */
  auto scale_pixel_value = [](i32 val, i32 bpp) { return static_cast<i32>(val) << (16 - bpp); };

  ui16 R, G, B;
  i32 X, Y, B_;
  const auto stride = width;
  for (ui32 y = 0; y < height; ++y) {
    for (ui32 x = 0; x < width; ++x) {
      size_t idx = y * stride + x;

      R = scale_pixel_value(red[idx], bpp);
      G = scale_pixel_value(grn[idx], bpp);
      B = scale_pixel_value(blu[idx], bpp);

      i32 Lmix_tmp = (5111U * B) >> 2;

      Lmix = 4915U * R + ((40763U * G) >> 2) + Lmix_tmp - bias;
      Lmix >>= 14;
      Lgamma = cbrt_fix(Lmix);

      Mmix = ((15073U * R) >> 2) + ((22675U * G) >> 1) + Lmix_tmp - bias;
      Mmix >>= 14;
      Mgamma = cbrt_fix(Mmix);

      Smix = (3988U * R + ((13419U * G) >> 2)) + ((36163U * B) >> 2) - bias;
      Smix >>= 14;
      Sgamma = ((cbrt_fix(Smix) << 14) + bias_cbrt) >> 14;

      // X = (Lgamma - Mgamma) / 2;
      X = ((Lgamma << 14) - (Mgamma << 14)) >> 15;

      // Y = (Lgamma + Mgamma) / 2;
      Y = static_cast<i32>((static_cast<ui32>(Lgamma) << 14) + (static_cast<ui32>(Mgamma) << 14));
      Y += bias_cbrt * 2;
      Y >>= 15;

      // B = Sgamma
      B_ = Sgamma;

      buf_X[idx] = X;
      buf_Y[idx] = Y;
      buf_B[idx] = B_;
    }
  }
}

// Lgamma = cbrt_fix(
//     static_cast<ui16>((static_cast<i32>((4915U * R + ((40763U * G) >> 2)) + Lmix_tmp) - bias) >>
//     14));
// Mgamma = cbrt_fix(static_cast<ui16>(
//     (static_cast<i32>((((15073U * R) >> 2) + ((22675U * G) >> 1)) + Lmix_tmp) - bias) >> 14));
// Sgamma = static_cast<ui16>(
//     ((cbrt_fix(static_cast<ui16>(
//           (static_cast<i32>((3988U * R + ((13419U * G) >> 2)) + ((36163U * B) >> 2)) - bias) >>
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