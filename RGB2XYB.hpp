#include "image_io.hpp"

// #define CBRT_CALC16
//   #define CBRT_CALC32
#define CBRT_LUT16

#if defined(CBRT_CALC32) || defined(CBRT_CALC16)
  #include "cbrt_calc_fix.hpp"  // fixed-point calculation of cubic root by newton method
#elif defined(CBRT_LUT16)
  #include "cbrt_tbl_fix.hpp"  //  fixed-point calculation of cubic root by LUT
#endif

/**
 * @brief Scale input pixel value into Q16 format
 *
 * @param val input
 * @return Q16 foramt of input
 */
static inline ui32 scale_pixel_value(i32 val, i32 bpp) { return static_cast<ui32>(val) << (16 - bpp); };

void rgb2xyb(image &rgb_in, image &xyb_out) {
  // Set matrix coefficients
  const mat_coeff T00(4915U, 0);       // 0.3 << 14
  const mat_coeff T01(40763U, 2);      // 0.622 << 16
  const mat_coeff T02(5111U, 2);       // 0.078 << 16
  const mat_coeff T10(15073U, 2);      // 0.23 << 16
  const mat_coeff T11(22675U, 1);      // 0.692 << 15
  const mat_coeff T12(5111U, 2);       // 0.078 << 16
  const mat_coeff T20(3988U, 0);       // 0.24342268924547819 << 14
  const mat_coeff T21(13419U, 2);      // 0.20476744424496821 << 16
  const mat_coeff T22(36163U, 2);      // 0.55180986650955360 << 16
  const i32 bias        = -4079616;    // / 2^30 = -0.003799438476562
  const i32 bias_cbrt   = -167460864;  // / 2^30 = -0.155960083007812
  const i16 bias_cbrt16 = -10221;      // / 2^16 = -0.155960083007812

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
  i32 Lmax = INT32_MIN, Lmin = INT32_MAX, Mmax = INT32_MIN, Mmin = INT32_MAX, Smax = INT32_MIN,
      Smin = INT32_MAX;
  ui32 r, g, b;  // RGB inputs are limited in 16bpp, ui16(0-65535)
  i32 X, Y, B;   // XYB output are scaled by 2^16
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

      Lmax = Lmax < Lmix ? Lmix : Lmax;
      Lmin = Lmin > Lmix ? Lmix : Lmin;
      Mmax = Mmax < Mmix ? Mmix : Mmax;
      Mmin = Mmin > Mmix ? Mmix : Mmin;
      Smax = Smax < Smix ? Smix : Smax;
      Smin = Smin > Smix ? Smix : Smin;

      // Limit _mix values to prevent overflow
      Lmix = Lmix > 65535 ? 65535 : Lmix;
      Mmix = Mmix > 65535 ? 65535 : Mmix;
      Smix = Smix > 65535 ? 65535 : Smix;

#if defined(CBRT_CALC32)
      //  use i32 version of cbrt
      Lgamma    = cbrt_fix(Lmix);
      Mgamma    = cbrt_fix(Mmix);
      auto Stmp = cbrt_fix(Smix);
      Sgamma    = ((static_cast<ui32>(Stmp) << 14) + bias_cbrt) >> 14;
#elif defined(CBRT_CALC16)
      // use ui16 version of cbrt (applicable to images less than 16 bpp)
      Lgamma = cbrt_fix(static_cast<ui16>(Lmix));
      Mgamma = cbrt_fix(static_cast<ui16>(Mmix));
      Sgamma = cbrt_fix(static_cast<ui16>(Smix)) + bias_cbrt16;
#elif defined(CBRT_LUT16)
      Lgamma = cbrt_lut(static_cast<ui16>(Lmix));
      Mgamma = cbrt_lut(static_cast<ui16>(Mmix));
      Sgamma = cbrt_lut(static_cast<ui16>(Smix)) + bias_cbrt16;
#endif

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
  printf("Lmax = %d, Lmin = %d\n", Lmax, Lmin);
  printf("Mmax = %d, Mmin = %d\n", Mmax, Mmin);
  printf("Smax = %d, Smin = %d\n", Smax, Smin);
}