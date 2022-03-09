#include "pgm_io.hpp"

int pgm_component::read(const std::string &filename) {
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
    printf("ERROR: %s is not a PGM file.\n", filename.c_str());
    fclose(fp);
    return EXIT_FAILURE;
  }

  d = fgetc(fp);
  switch (d) {
    // PGM
    case '2':
      isASCII = true;
    case '5':
      isBigendian = true;
      break;
    // error
    default:
      printf("ERROR: %s is not a PGM file.\n", filename.c_str());
      fclose(fp);
      return EXIT_FAILURE;
      break;
  }
  while (st != status::DONE) {
    d = fgetc(fp);
    eat_white(d, fp, comment);
    // read numerical value
    while (d != SPC && d != LF && d != CR) {
      val *= 10;
      val += d - '0';
      d = fgetc(fp);
    }
    // update status
    switch (st) {
      case status::READ_WIDTH:
        set_width(val);
        val = 0;
        st  = status::READ_HEIGHT;
        break;
      case status::READ_HEIGHT:
        set_height(val);
        val = 0;
        st  = status::READ_MAXVAL;
        break;
      case status::READ_MAXVAL:
        set_bpp(static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        val = 0;
        st  = status::DONE;
        break;
      default:
        break;
    }
  }
  eat_white(d, fp, comment);
  fseek(fp, -1, SEEK_CUR);

  // P5 (binary) read
  const uint32_t byte_per_sample = (get_bpp() + 8 - 1) / 8;
  uint32_t compw                 = get_width();
  uint32_t comph                 = get_height();
  create_buf(compw * comph);
  int32_t *dst          = get_buf();
  const uint32_t length = compw * comph;
#if defined(USE_ARM_NEON)
  const size_t simdgap = (byte_per_sample == 1) ? 16 : 8;
  const size_t simdlen = length - (length) % simdgap;
  if (byte_per_sample > 1) {  // > 8 bpp
    auto tmp = aligned_uptr<uint16_t>(32, length);
    if (fread(tmp.get(), sizeof(uint16_t), length, fp) < length) {
      printf("ERROR: not enough samples in the given pnm file.\n");
      fclose(fp);
      return EXIT_FAILURE;
    }
    auto line_buf = tmp.get();
    for (size_t i = 0; i < simdlen; i += simdgap) {
      auto src = vld1q_u16(line_buf + i);
      store_big_u16_to_s32(src, dst + i);
    }
    for (size_t i = simdlen; i < length; ++i) {
      dst[i] = (line_buf[i] >> 8) | ((line_buf[i] & 0xFF) << 8);
    }
  } else {  // <= 8bpp
    auto tmp = aligned_uptr<uint8_t>(32, length);
    if (fread(tmp.get(), sizeof(uint8_t), length, fp) < length) {
      printf("ERROR: not enough samples in the given pnm file.\n");
      fclose(fp);
      return EXIT_FAILURE;
    }
    auto line_buf = tmp.get();
    for (size_t i = 0; i < simdlen; i += simdgap) {
      auto src = vld1q_u8(line_buf + i);
      store_u8_to_s32(src, dst + i);
    }
    for (size_t i = simdlen; i < length; ++i) {
      dst[i] = line_buf[i];
    }
  }
#else
  if (byte_per_sample > 1) {  // > 8 bpp
    auto tmp = aligned_uptr<uint16_t>(32, length);
    if (fread(tmp.get(), sizeof(uint16_t), length, fp) < length) {
      printf("ERROR: not enough samples in the given pnm file.\n");
      fclose(fp);
      return EXIT_FAILURE;
    }
    auto line_buf = tmp.get();
    for (size_t i = 0; i < length; ++i) {
      dst[i] = (line_buf[i] >> 8) | ((line_buf[i] & 0xFF) << 8);
    }
  } else {  // <= 8bpp
    auto tmp = aligned_uptr<uint8_t>(32, length);
    if (fread(tmp.get(), sizeof(uint8_t), length, fp) < length) {
      printf("ERROR: not enough samples in the given pnm file.\n");
      fclose(fp);
      return EXIT_FAILURE;
    }
    auto line_buf = tmp.get();
    for (size_t i = 0; i < length; ++i) {
      dst[i] = line_buf[i];
    }
  }
#endif
  fclose(fp);
  return EXIT_SUCCESS;
}
