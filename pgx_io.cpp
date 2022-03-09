#include "pgx_io.hpp"

int pgx_component::read(const std::string &filename) {
  bool isBigendian = false;

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
    printf("ERROR: %s is not a PGX file.\n", filename.c_str());
    fclose(fp);
    return EXIT_FAILURE;
  }
  d = fgetc(fp);
  if (d != 'G') {
    printf("ERROR: input PGX file %s is broken.\n", filename.c_str());
    fclose(fp);
    return EXIT_FAILURE;
  }

  // read endian
  do {
    d = fgetc(fp);
  } while (d != 'M' && d != 'L');
  switch (d) {
    case 'M':
      isBigendian = true;
      d           = fgetc(fp);
      if (d != 'L') {
        printf("ERROR: input PGX file %s is broken.\n", filename.c_str());
      }
      break;
    case 'L':
      d = fgetc(fp);
      if (d != 'M') {
        printf("ERROR: input PGX file %s is broken.\n", filename.c_str());
      }
      break;
    default:
      printf("ERROR: input file does not conform to PGX format.\n");
      return EXIT_FAILURE;
  }
  // check signed or not
  do {
    d = fgetc(fp);
  } while (d != '+' && d != '-' && isdigit(d) == false);
  if (d == '+' || d == '-') {
    if (d == '-') {
      set_is_signed(true);
    }
    do {
      d = fgetc(fp);
    } while (isdigit(d) == false);
  }
  do {
    val *= 10;
    val += d - '0';
    d = fgetc(fp);
  } while (d != SP && d != LF && d != CR);
  set_bpp(val);
  val = 0;

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
        set_width(val);
        val = 0;
        st  = status::READ_HEIGHT;
        break;
      case status::READ_HEIGHT:
        set_height(val);
        st = status::DONE;
        break;
      default:
        break;
    }
  }
  eat_white(d, fp, comment);
  fseek(fp, -1, SEEK_CUR);

  const uint32_t byte_per_sample = (get_bpp() + 8 - 1) / 8;
  const uint32_t compw           = get_width();
  const uint32_t comph           = get_height();
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
    if (get_is_signed()) {
      if (isBigendian) {
        for (size_t i = 0; i < simdlen; i += simdgap) {
          auto src = vld1q_u16(line_buf + i);
          store_big_s16_to_s32(src, dst + i);
        }
        for (size_t i = simdlen; i < length; ++i) {
          dst[i] = static_cast<int16_t>((line_buf[i] >> 8) | ((line_buf[i] & 0xFF) << 8));
        }
      } else {
        for (size_t i = 0; i < simdlen; i += simdgap) {
          auto src = vld1q_u16(line_buf + i);
          store_little_s16_to_s32(src, dst + i);
        }
        for (size_t i = simdlen; i < length; ++i) {
          dst[i] = static_cast<int16_t>(line_buf[i]);
        }
      }
    } else {
      if (isBigendian) {
        for (size_t i = 0; i < simdlen; i += simdgap) {
          auto src = vld1q_u16(line_buf + i);
          store_big_u16_to_s32(src, dst + i);
        }
        for (size_t i = simdlen; i < length; ++i) {
          dst[i] = (line_buf[i] >> 8) | ((line_buf[i] & 0xFF) << 8);
        }
      } else {
        for (size_t i = 0; i < simdlen; i += simdgap) {
          auto src = vld1q_u16(line_buf + i);
          store_little_u16_to_s32(src, dst + i);
        }
        for (size_t i = simdlen; i < length; ++i) {
          dst[i] = line_buf[i];
        }
      }
    }
  } else {  // <= 8bpp
    auto tmp = aligned_uptr<uint8_t>(32, length);
    if (fread(tmp.get(), sizeof(uint8_t), length, fp) < length) {
      printf("ERROR: not enough samples in the given pnm file.\n");
      fclose(fp);
      return EXIT_FAILURE;
    }
    auto line_buf = tmp.get();
    if (get_is_signed()) {
      for (size_t i = 0; i < simdlen; i += simdgap) {
        auto src = vld1q_s8((int8_t *)line_buf + i);
        store_s8_to_s32(src, dst + i);
      }
      for (size_t i = simdlen; i < length; ++i) {
        dst[i] = static_cast<int8_t>(line_buf[i]);
      }
    } else {
      for (size_t i = 0; i < simdlen; i += simdgap) {
        auto src = vld1q_u8(line_buf + i);
        store_u8_to_s32(src, dst + i);
      }
      for (size_t i = simdlen; i < length; ++i) {
        dst[i] = line_buf[i];
      }
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
    if (get_is_signed()) {
      if (isBigendian) {
        for (size_t i = 0; i < length; ++i) {
          dst[i] = static_cast<int16_t>((line_buf[i] >> 8) | ((line_buf[i] & 0xFF) << 8));
        }
      } else {
        for (size_t i = 0; i < length; ++i) {
          dst[i] = static_cast<int16_t>(line_buf[i]);
        }
      }
    } else {
      if (isBigendian) {
        for (size_t i = 0; i < length; ++i) {
          dst[i] = (line_buf[i] >> 8) | ((line_buf[i] & 0xFF) << 8);
        }
      } else {
        for (size_t i = 0; i < length; ++i) {
          dst[i] = line_buf[i];
        }
      }
    }
  } else {  // <= 8bpp
    auto tmp = aligned_uptr<uint8_t>(32, length);
    if (fread(tmp.get(), sizeof(uint8_t), length, fp) < length) {
      printf("ERROR: not enough samples in the given pnm file.\n");
      fclose(fp);
      return EXIT_FAILURE;
    }
    auto line_buf = tmp.get();
    if (get_is_signed()) {
      for (size_t i = 0; i < length; ++i) {
        dst[i] = static_cast<int8_t>(line_buf[i]);
      }
    } else {
      for (size_t i = 0; i < length; ++i) {
        dst[i] = line_buf[i];
      }
    }
  }

#endif
  fclose(fp);
  return EXIT_SUCCESS;
}
