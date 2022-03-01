#include <arm_neon.h>
#include <algorithm>

#include "image_io.hpp"

image::image(const std::vector<std::string> &filenames) : width(0), height(0), buf(nullptr) {
  size_t num_files = filenames.size();
  if (num_files > 16384) {
    printf("ERROR: over 16384 components are not supported in the spec.\n");
    throw std::exception();
  }
  num_components = 0;
  for (const auto &fname : filenames) {
    size_t ext_pos       = fname.find_last_of(".");
    std::string ext_name = fname.substr(ext_pos, fname.size() - ext_pos);
    if (ext_name == ".pgm" || ext_name == ".PGM") {
      num_components++;
    }
    if (ext_name == ".ppm" || ext_name == ".PPM") {
      num_components += 3;
    }
    if (ext_name == ".pgx" || ext_name == ".PGX") {
      num_components++;
    }
  }
  // allocate memory once
  if (this->buf == nullptr) {
    // this->buf =
    //     std::make_unique<std::unique_ptr<int32_t[]>[]>(this->num_components);
    this->buf = std::make_unique<unique_ptr_aligned<int32_t>[]>(this->num_components);
  }
  components.reserve(num_components);
  uint16_t c = 0;
  imgformat format;
  for (const auto &fname : filenames) {
    size_t ext_pos       = fname.find_last_of(".");
    std::string ext_name = fname.substr(ext_pos, fname.size() - ext_pos);
    if (ext_name == ".pgm" || ext_name == ".PGM") {
      format = imgformat::PGM;
    }
    if (ext_name == ".ppm" || ext_name == ".PPM") {
      format = imgformat::PPM;
    }
    if (ext_name == ".pgx" || ext_name == ".PGX") {
      format = imgformat::PGX;
    }
    switch (format) {
      case imgformat::PGM:
        printf("PGM\n");
        components.emplace_back(std::make_unique<pgm_component>(c));
        components[components.size() - 1]->read(fname);
        component_width.push_back(components[components.size() - 1]->get_width());
        component_height.push_back(components[components.size() - 1]->get_height());
        bits_per_pixel.push_back(components[components.size() - 1]->get_bpp());
        is_signed.push_back(components[components.size() - 1]->get_is_signed());
        this->buf[components.size() - 1] = components[components.size() - 1]->move_buf();
        c++;
        break;
      case imgformat::PPM:
        printf("PPM\n");
        components.emplace_back(std::make_unique<pgm_component>(c));
        components.emplace_back(std::make_unique<pgm_component>(c + 1));
        components.emplace_back(std::make_unique<pgm_component>(c + 2));
        read_ppm(fname, c);
        for (uint16_t i = c; i < c + 3; ++i) {
          component_width.push_back(components[i]->get_width());
          component_height.push_back(components[i]->get_height());
          bits_per_pixel.push_back(components[i]->get_bpp());
          is_signed.push_back(false);
          this->buf[i] = components[i]->move_buf();
        }
        c += 3;
        break;
      case imgformat::PGX:
        printf("PGX\n");
        components.emplace_back(std::make_unique<pgx_component>(c));
        components[components.size() - 1]->read(fname);
        component_width.push_back(components[components.size() - 1]->get_width());
        component_height.push_back(components[components.size() - 1]->get_height());
        bits_per_pixel.push_back(components[components.size() - 1]->get_bpp());
        is_signed.push_back(components[components.size() - 1]->get_is_signed());
        this->buf[components.size() - 1] = components[components.size() - 1]->move_buf();
        c++;
        break;
      deafult:
        printf("ERROR\n");
    }
  }
  width  = *std::max_element(component_width.begin(), component_width.end());
  height = *std::max_element(component_height.begin(), component_height.end());
}

int image::read_ppm(const std::string &filename, uint16_t compidx) {
  bool isASCII     = false;
  bool isBigendian = false;
  bool isSigned    = false;

  FILE *fp = fopen(filename.c_str(), "rb");
  if (fp == nullptr) {
    printf("ERROR: File %s is not found.\n", filename.c_str());
    return EXIT_FAILURE;
  }
  status st = status::READ_WIDTH;
  int d;
  uint32_t val = 0;
  char comment[256];
  d = fgetc(fp);
  if (d != 'P') {
    printf("ERROR: %s is not a PPM file.\n", filename.c_str());
    fclose(fp);
    return EXIT_FAILURE;
  }

  bool isPPM = false;

  d = fgetc(fp);
  switch (d) {
    // PPM
    case '3':
      isASCII = true;
    case '6':
      isPPM       = true;
      isBigendian = true;
      break;
    // error
    default:
      printf("ERROR: %s is not a PPM file.\n", filename.c_str());
      fclose(fp);
      return EXIT_FAILURE;
      break;
  }
  while (st != status::DONE) {
    d = fgetc(fp);
    eat_white(d, fp, comment);
    // read numerical value
    while (d != SP && d != LF && d != CR) {
      val *= 10;
      val += d - '0';
      d = fgetc(fp);
    }
    // update status
    switch (st) {
      case status::READ_WIDTH:
        this->components[compidx]->set_width(val);
        this->components[compidx + 1]->set_width(val);
        this->components[compidx + 2]->set_width(val);
        val = 0;
        st  = status::READ_HEIGHT;
        break;
      case status::READ_HEIGHT:
        this->components[compidx]->set_height(val);
        this->components[compidx + 1]->set_height(val);
        this->components[compidx + 2]->set_height(val);
        val = 0;
        st  = status::READ_MAXVAL;
        break;
      case status::READ_MAXVAL:
        this->components[compidx]->set_bpp(static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        this->components[compidx + 1]->set_bpp(static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        this->components[compidx + 2]->set_bpp(static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        val = 0;
        st  = status::DONE;
        break;
      default:
        break;
    }
  }
  eat_white(d, fp, comment);
  fseek(fp, -1, SEEK_CUR);

  const uint32_t byte_per_sample      = (components[compidx]->get_bpp() + 8 - 1) / 8;
  const uint32_t component_gap        = 3 * byte_per_sample;
  const uint32_t compw                = components[compidx]->get_width();
  const uint32_t comph                = components[compidx]->get_height();
  const uint32_t line_width           = component_gap * compw;
  const uint32_t length               = line_width * comph;
  std::unique_ptr<uint8_t[]> line_buf = std::make_unique<uint8_t[]>(line_width);
  // allocate memory
  for (size_t i = compidx; i < compidx + 3; ++i) {
    components[i]->create_buf(compw * comph);
    //   this->buf[i] = std::make_unique<int32_t[]>(compw * comph);
  }
  auto tmp = aligned_uptr<uint8_t>(32, length);
  if (fread(tmp.get(), sizeof(uint8_t), length, fp) < length) {
    printf("ERROR: not enough samples in the given pnm file.\n");
    fclose(fp);
    return EXIT_FAILURE;
  }
  auto R               = components[compidx]->get_buf();
  auto G               = components[compidx + 1]->get_buf();
  auto B               = components[compidx + 2]->get_buf();
  const int32_t shift0 = (isBigendian && (byte_per_sample > 1)) ? 8 : 0;
  auto src             = tmp.get();
  switch (byte_per_sample) {
    case 1:  // <= 8bpp
      for (size_t i = 0; i < compw * comph; i += 16) {
        uint8x16x3_t vsrc = vld3q_u8((src + i * component_gap));
        uint8x16_t aR     = vsrc.val[0];
        uint8x16_t aG     = vsrc.val[1];
        uint8x16_t aB     = vsrc.val[2];

        int16x8_t rl = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(aR)));
        int16x8_t rh = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(aR)));
        auto rll     = vmovl_s16(vget_low_s16(rl));
        auto rlh     = vmovl_s16(vget_high_s16(rl));
        auto rhl     = vmovl_s16(vget_low_s16(rh));
        auto rhh     = vmovl_s16(vget_high_s16(rh));
        vst1q_s32(R + i, rll);
        vst1q_s32(R + i + 4, rlh);
        vst1q_s32(R + i + 8, rhl);
        vst1q_s32(R + i + 12, rhh);

        int16x8_t gl = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(aG)));
        int16x8_t gh = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(aG)));
        auto gll     = vmovl_s16(vget_low_s16(gl));
        auto glh     = vmovl_s16(vget_high_s16(gl));
        auto ghl     = vmovl_s16(vget_low_s16(gh));
        auto ghh     = vmovl_s16(vget_high_s16(gh));
        vst1q_s32(G + i, gll);
        vst1q_s32(G + i + 4, glh);
        vst1q_s32(G + i + 8, ghl);
        vst1q_s32(G + i + 12, ghh);

        int16x8_t bl = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(aB)));
        int16x8_t bh = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(aB)));
        auto bll     = vmovl_s16(vget_low_s16(bl));
        auto blh     = vmovl_s16(vget_high_s16(bl));
        auto bhl     = vmovl_s16(vget_low_s16(bh));
        auto bhh     = vmovl_s16(vget_high_s16(bh));
        vst1q_s32(B + i, bll);
        vst1q_s32(B + i + 4, blh);
        vst1q_s32(B + i + 8, bhl);
        vst1q_s32(B + i + 12, bhh);
      }
      break;
    case 2:  // > 8bpp
      for (size_t i = 0; i < compw * comph; i += 8) {
        uint16x8x3_t vsrc = vld3q_u16((uint16_t *)(src + i * component_gap));
        uint16x8_t aR     = vrev16q_u8(vsrc.val[0]);
        uint16x8_t aG     = vrev16q_u8(vsrc.val[1]);
        uint16x8_t aB     = vrev16q_u8(vsrc.val[2]);

        auto r0 = vreinterpretq_s32_s16(aR);
        auto rl = vmovl_s16(vget_low_s32(r0));
        auto rh = vmovl_s16(vget_high_s32(r0));
        vst1q_s32(R + i, rl);
        vst1q_s32(R + i + 4, rh);

        auto g0 = vreinterpretq_s32_s16(aG);
        auto gl = vmovl_s16(vget_low_s32(g0));
        auto gh = vmovl_s16(vget_high_s32(g0));
        vst1q_s32(G + i, gl);
        vst1q_s32(G + i + 4, gh);

        auto b0 = vreinterpretq_s32_s16(aB);
        auto bl = vmovl_s16(vget_low_s32(b0));
        auto bh = vmovl_s16(vget_high_s32(b0));
        vst1q_s32(B + i, bl);
        vst1q_s32(B + i + 4, bh);
      }
      break;
    default:
      break;
  }
  // for (size_t i = 0; i < compw * comph; ++i) {
  //   R[i] = src[component_gap * i] << shift0;
  //   G[i] = src[component_gap * i + byte_per_sample] << shift0;
  //   B[i] = src[component_gap * i + 2 * byte_per_sample] << shift0;
  // }
  // const int32_t shift1 = (isBigendian && (byte_per_sample > 1)) ? 0 : 8;
  // for (size_t i = 0; i < compw * comph * (byte_per_sample - 1); ++i) {
  //   R[i] |= src[component_gap * i + 1] << shift1;
  //   G[i] |= src[component_gap * i + byte_per_sample + 1] << shift1;
  //   B[i] |= src[component_gap * i + 2 * byte_per_sample + 1] << shift1;
  // }

  // if (!isASCII) {
  //   for (size_t i = 0; i < comph; ++i) {
  //     if (fread(line_buf.get(), sizeof(uint8_t), line_width, fp) < line_width) {
  //       printf("ERROR: not enough samples in the given pnm file.\n");
  //       fclose(fp);
  //       return EXIT_FAILURE;
  //     }
  //     for (size_t c = compidx; c < compidx + 3; ++c) {
  //       uint8_t *src;
  //       int32_t *dst;
  //       src = &line_buf[c * byte_per_sample];

  //       dst = components[c]->get_buf(i * compw);  //  &this->buf[c][i * compw];

  //       switch (byte_per_sample) {
  //         case 1:
  //           for (size_t j = 0; j < compw; ++j) {
  //             *dst = (isSigned) ? static_cast<int8_t>(*src) : *src;
  //             dst++;
  //             src += component_gap;
  //           }
  //           break;
  //         case 2:
  //           if (isSigned) {
  //             if (isBigendian) {
  //               for (size_t j = 0; j < compw; ++j) {
  //                 *dst = static_cast<int_least16_t>(
  //                     (static_cast<uint_least16_t>(src[0]) << 8) |
  //                     static_cast<uint_least16_t>(src[1]));
  //                 dst++;
  //                 src += component_gap;
  //               }
  //             } else {
  //               for (size_t j = 0; j < compw; ++j) {
  //                 *dst = static_cast<int_least16_t>(
  //                     static_cast<uint_least16_t>(src[0]) |
  //                     (static_cast<uint_least16_t>(src[1]) << 8));
  //                 dst++;
  //                 src += component_gap;
  //               }
  //             }
  //           } else {
  //             if (isBigendian) {
  //               for (size_t j = 0; j < compw; ++j) {
  //                 *dst = (src[0] << 8) | src[1];
  //                 dst++;
  //                 src += component_gap;
  //               }
  //             } else {
  //               for (size_t j = 0; j < compw; ++j) {
  //                 *dst = src[0] | (src[1] << 8);
  //                 dst++;
  //                 src += component_gap;
  //               }
  //             }
  //           }
  //           break;
  //         default:
  //           printf("ERROR: bit-depth over 16 is not supported.\n");
  //           fclose(fp);
  //           return EXIT_FAILURE;
  //           break;
  //       }
  //     }
  //   }
  // } else {
  //   for (size_t i = 0; i < compw * comph; ++i) {
  //     for (size_t c = compidx; c < compidx + 3; ++c) {
  //       int32_t *dst = components[c]->get_buf(i);
  //       val = 0;
  //       d = fgetc(fp);
  //       while (d != SP && d != CR && d != LF && d != EOF) {
  //         val *= 10;
  //         val += d - '0';
  //         d = fgetc(fp);
  //       }
  //       dst[0] = val;
  //     }
  //   }
  // }
  fclose(fp);
  return EXIT_SUCCESS;
}

uint32_t image::get_component_width(uint16_t c) const {
  if (c > num_components) {
    printf("ERROR: component index %d is larger than maximum value %d.\n", c, num_components);
    throw std::exception();
  }
  return this->component_width[c];
}

uint32_t image::get_component_height(uint16_t c) const {
  if (c > num_components) {
    printf("ERROR: component index %d is larger than maximum value %d.\n", c, num_components);
    throw std::exception();
  }
  return this->component_height[c];
}

uint8_t image::get_Ssiz_value(uint16_t c) const {
  return (this->is_signed[c]) ? (this->bits_per_pixel[c] - 1) | 0x80 : this->bits_per_pixel[c] - 1;
}

uint8_t image::get_max_bpp() const {
  uint8_t max = 0;
  for (auto &v : bits_per_pixel) {
    max = (max < v) ? v : max;
  }
  return max;
}