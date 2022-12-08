#include "image_io.hpp"

static unsigned long _u64d_div__(unsigned long b, unsigned long c) { return b / c; }

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
  const uint32_t val;
  const int32_t rshift;
  mat_coeff() : val(0), rshift(0) {}
  explicit mat_coeff(uint32_t v, uint32_t rs) : val(v), rshift(rs) {}
  int32_t mul(uint32_t v) { return (val * v) >> rshift; }
};

uint32_t cbrt_fix(uint32_t N) {
  const uint32_t eps = 4U;
  mat_coeff k1_3(21845U, 16);  // = 1/3

  uint32_t x_n, x_n_1, a;
  uint16_t y = 0U;
  if (N > 0U) {
    for (int i = 8; i >= 0; --i) {
      uint16_t ytmp;
      ytmp = static_cast<uint16_t>(y | 1 << i);
      if (static_cast<int32_t>(ytmp) * ytmp <= N) {
        y = ytmp;
      }
    }
  }
  x_n = static_cast<uint32_t>(y) << 8;

  a = k1_3.mul(N);  // * (1/3)

  for (int i = 0; i < 5; ++i) {
    unsigned long b0;
    uint32_t b;
    // b = (static_cast<uint32_t>((static_cast<unsigned long>(x_n) * x_n) >> 16) + eps) & 262143U;
    b = (static_cast<uint32_t>((static_cast<unsigned long>(x_n) * x_n) >> 16) + eps);
    if (b == 0U) {
      b0 = 17592186044415UL;
    } else {
      unsigned long u;
      if (b == 0UL) {
        u = 17592186044415UL;
      } else {
        u = _u64d_div__(static_cast<unsigned long>(static_cast<uint32_t>(a)) << 24,
                        static_cast<unsigned long>(b));
      }
      b0 = u;
    }
    // x_n_1 = (((x_n - k1_3.mul(x_n)) & 262143U) + (static_cast<uint32_t>(b0 >> 8) & 262143U)) & 131071U;
    x_n_1 = x_n - static_cast<uint32_t>(k1_3.mul(x_n)) + (static_cast<uint32_t>(b0 >> 8));
    x_n   = x_n_1;
  }
  return x_n_1;
}

void rgb2xyb(image &rgb_in, image &xyb_out) {
  mat_coeff T00(4915U, 14);   // 0.3
  mat_coeff T01(40763U, 16);  // 0.622
  mat_coeff T02(5111U, 16);   // 0.078

  mat_coeff T10(15073U, 16);  // 0.23
  mat_coeff T11(22675U, 15);  // 0.692
  mat_coeff T12(5111U, 16);   // 0.078

  mat_coeff T20(997U, 12);    // 0.24342268924547819
  mat_coeff T21(13419U, 16);  // 0.20476744424496821
  mat_coeff T22(36163U, 16);  // 0.55180986650955360

  const int32_t bias      = 249;
  const int32_t bias_cbrt = 10221;  // /2^16 = -0.155960083007812

  uint32_t width  = rgb_in.get_width();
  uint32_t height = rgb_in.get_height();

  if (rgb_in.get_num_components() != 3) {
    printf("Number of components shall be 3!\n");
    exit(EXIT_FAILURE);
  }

  int32_t *red, *grn, *blu;
  int32_t *X, *Y, *B;

  red = rgb_in.get_buf(0);
  grn = rgb_in.get_buf(1);
  blu = rgb_in.get_buf(2);

  X = xyb_out.get_buf(0);
  Y = xyb_out.get_buf(1);
  B = xyb_out.get_buf(2);

  int32_t Lmix, Mmix, Smix;
  uint32_t Lgamma, Mgamma, Sgamma;
  int32_t Rval, Gval, Bval;

  /**
   * @brief return_value = floor(static_cast<double>(val) / 255.0 * 65536);
   *
   */
  auto scale_pixel_value = [](int32_t val) {
    return static_cast<int32_t>(val) + (static_cast<int32_t>(val) << 8);
  };

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      size_t idx = y * width + x;

      Rval = scale_pixel_value(red[idx]);
      Gval = scale_pixel_value(grn[idx]);
      Bval = scale_pixel_value(blu[idx]);

      // Lmix = static_cast<int32_t>(T00.mul(Rval) + static_cast<uint32_t>((40763UL * Gval) >> 16))
      //            + T02.mul(Bval)
      //        & 262143U;
      Lmix = T00.mul(Rval) + T01.mul(Gval) + T02.mul(Bval);  // & 262143U;
      // Lmix = saturate(Lmix, 131072);
      Lmix += bias;
      // Lmix = saturate(Lmix, 131072);

      Mmix = T10.mul(Rval) + T11.mul(Gval) + T12.mul(Bval);
      // Mmix = saturate(Mmix, 131072);
      Mmix += bias;
      // Mmix = saturate(Mmix, 131072);

      // Smix = static_cast<int32_t>(T20.mul(Rval) + T21.mul(Gval)
      //                             + static_cast<uint32_t>((36163UL * Bval) >> 16));
      Smix = T20.mul(Rval) + T21.mul(Gval) + T22.mul(Bval);
      Smix += bias;
      // Smix = saturate(Smix, 131072);

      // Lgamma = cbrt_fix(Lmix & 131071U);
      // Mgamma = cbrt_fix(Mmix & 131071U);
      Lgamma = cbrt_fix(Lmix);
      Mgamma = cbrt_fix(Mmix);

      // X[idx] = (Lgamma - Mgamma) >> 1;
      // int16_t tmp = static_cast<int16_t>(((static_cast<uint16_t>(static_cast<int>(Lgamma) - bias_cbrt)
      //                                      - static_cast<uint16_t>(static_cast<int>(Mgamma) - bias_cbrt))
      //                                     << 1)
      //                                    >> 2);
      // tmp         = saturate(tmp, 1024);
      X[idx] = (static_cast<int32_t>(Lgamma) - static_cast<int32_t>(Mgamma)) >> 1;
      // Y[idx] = (Lgamma + Mgamma) >> 1;
      // Y[idx] = static_cast<int32_t>(
      //     ((static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int>(Lgamma) - bias_cbrt))
      //       + static_cast<uint16_t>(static_cast<int32_t>(Mgamma) - bias_cbrt))
      //      << 1)
      //     >> 2);
      Y[idx] = (static_cast<int32_t>(Lgamma) + static_cast<int32_t>(Mgamma) - (bias_cbrt << 1)) >> 1;

      // B = Sgamma
      // Sgamma = static_cast<uint16_t>(static_cast<int32_t>(cbrt_fix(Smix & 131071U)) - bias_cbrt);
      Sgamma = static_cast<uint32_t>(static_cast<int32_t>(cbrt_fix(Smix)) - bias_cbrt);
      B[idx] = Sgamma;
    }
  }
}