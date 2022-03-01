#include "pgm_io.hpp"

int pgm_component::read(const std::string &filename) {
  bool isASCII = false;
  bool isBigendian = false;
  bool isSigned = false;

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
        st = status::READ_HEIGHT;
        break;
      case status::READ_HEIGHT:
        set_height(val);
        val = 0;
        st = status::READ_MAXVAL;
        break;
      case status::READ_MAXVAL:
        set_bpp(static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        val = 0;
        st = status::DONE;
        break;
      default:
        break;
    }
  }
  eat_white(d, fp, comment);
  fseek(fp, -1, SEEK_CUR);

  // P5 (binary) read
  const uint32_t byte_per_sample = (get_bpp() + 8 - 1) / 8;
  uint32_t compw = get_width();
  uint32_t comph = get_height();
  create_buf(compw * comph);
  int32_t *dst = get_buf();
  if (byte_per_sample > 1) {  // > 8 bpp
    auto line_buf = std::make_unique<uint16_t[]>(compw);
    for (size_t i = 0; i < comph; ++i) {
      if (fread(line_buf.get(), sizeof(uint16_t), compw, fp) < compw) {
        printf("ERROR: not enough samples in the given pgm file.\n");
        fclose(fp);
        return EXIT_FAILURE;
      }
      for (size_t j = 0; j < compw; ++j) {
        dst[i * compw + j] = (line_buf[j] >> 8) | ((line_buf[j] & 0xFF) << 8);
      }
    }
  } else {  // <= 8bpp
    auto line_buf = std::make_unique<uint8_t[]>(compw);
    for (size_t i = 0; i < comph; ++i) {
      if (fread(line_buf.get(), sizeof(uint8_t), compw, fp) < compw) {
        printf("ERROR: not enough samples in the given pgm file.\n");
        fclose(fp);
        return EXIT_FAILURE;
      }
      for (size_t j = 0; j < compw; ++j) {
        dst[i * compw + j] = line_buf[j];
      }
    }
  }

  fclose(fp);
  return EXIT_SUCCESS;
}
