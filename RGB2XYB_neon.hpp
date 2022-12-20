#include "image_io.hpp"
#include "cbrt_tbl_fix.hpp"  //  fixed-point calculation of cubic root by LUT

/**
 * @brief Scale input pixel value into Q16 format
 *
 * @param val input
 * @return Q16 foramt of input
 */
static inline uint32x4_t scale_pixel_value(i32 *val, int32x4_t shift) {
  return vshlq_s32(vreinterpretq_u32_s32(vld1q_s32(val)), shift);
}

void rgb2xyb_neon(image &rgb_in, image &xyb_out) {
  // Set matrix coefficients
  const mat_coeff_neon T00(4915U, 0);                   // 0.3 << 14
  const mat_coeff_neon T01(40763U, 2);                  // 0.622 << 16
  const mat_coeff_neon T02(5111U, 2);                   // 0.078 << 16
  const mat_coeff_neon T10(15073U, 2);                  // 0.23 << 16
  const mat_coeff_neon T11(22675U, 1);                  // 0.692 << 15
  const mat_coeff_neon T12(5111U, 2);                   // 0.078 << 16
  const mat_coeff_neon T20(3988U, 0);                   // 0.24342268924547819 << 14
  const mat_coeff_neon T21(13419U, 2);                  // 0.20476744424496821 << 16
  const mat_coeff_neon T22(36163U, 2);                  // 0.55180986650955360 << 16
  const int32x4_t bias      = vdupq_n_s32(-4079616);    // / 2^30 = -0.003799438476562
  const int32x4_t bias_cbrt = vdupq_n_s32(-167460864);  // / 2^30 = -0.155960083007812
  const i16 bias_cbrt16     = -10221;                   // / 2^16 = -0.155960083007812

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

  const int32x4_t shift = vdupq_n_s32(16 - rgb_in.get_max_bpp());
  int32x4_t Lmix, Mmix, Smix;
  int32x4_t Lgamma, Mgamma, Sgamma;

  uint32x4_t r, g, b;  // RGB inputs are limited in 16bpp, ui16(0-65535)
  int32x4_t X, Y, B;   // XYB output are scaled by 2^16
  size_t idx        = 0;
  const auto stride = width;
  for (ui32 y = 0; y < height; ++y) {
    idx = y * stride;
    for (ui32 x = 0; x < width; x += 4) {
      r = scale_pixel_value(buf_red + idx, shift);
      g = scale_pixel_value(buf_grn + idx, shift);
      b = scale_pixel_value(buf_blu + idx, shift);

      Lmix = (T00.mul(r) + T01.mul(g) + T02.mul(b) - bias) >> 14;
      Mmix = (T10.mul(r) + T11.mul(g) + T12.mul(b) - bias) >> 14;
      Smix = (T20.mul(r) + T21.mul(g) + T22.mul(b) - bias) >> 14;

      // Limit _mix values to prevent overflow
      Lmix = Lmix > 65535 ? 65535 : Lmix;
      Mmix = Mmix > 65535 ? 65535 : Mmix;
      Smix = Smix > 65535 ? 65535 : Smix;

      for (int i = 0; i < 4; ++i) {
        Lgamma[i] = cbrt_lut(static_cast<ui16>(Lmix[i]));
        Mgamma[i] = cbrt_lut(static_cast<ui16>(Mmix[i]));
        Sgamma[i] = cbrt_lut(static_cast<ui16>(Smix[i])) + bias_cbrt16;
      }
      // X = (Lgamma - Mgamma) / 2;
      X = (Lgamma - Mgamma) >> 1;
      // Y = (Lgamma + Mgamma) / 2;
      Y = vreinterpretq_s32_u32((vreinterpretq_u32_s32(Lgamma + Mgamma) << 14));
      Y += bias_cbrt * 2;
      Y >>= 15;

      // B = Sgamma
      B = Sgamma;

      vst1q_s32(buf_X + idx, X);
      vst1q_s32(buf_Y + idx, Y);
      vst1q_s32(buf_B + idx, B);

      idx += 4;
    }
  }
}