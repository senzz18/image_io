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
      d = fgetc(fp);
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
        st = status::READ_HEIGHT;
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
  const uint32_t compw = get_width();
  const uint32_t comph = get_height();
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
      if (get_is_signed()) {
        if (isBigendian) {
          for (size_t j = 0; j < compw; ++j) {
            dst[i * compw + j] = static_cast<int16_t>(
                (line_buf[j] >> 8) | ((line_buf[j] & 0xFF) << 8));
          }
        } else {
          for (size_t j = 0; j < compw; ++j) {
            dst[i * compw + j] = static_cast<int16_t>(line_buf[j]);
          }
        }
      } else {
        if (isBigendian) {
          for (size_t j = 0; j < compw; ++j) {
            dst[i * compw + j] =
                (line_buf[j] >> 8) | ((line_buf[j] & 0xFF) << 8);
          }
        } else {
          for (size_t j = 0; j < compw; ++j) {
            dst[i * compw + j] = line_buf[j];
          }
        }
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
      if (get_is_signed()) {
        for (size_t j = 0; j < compw; ++j) {
          dst[i * compw + j] = static_cast<int8_t>(line_buf[j]);
        }
      } else {
        for (size_t j = 0; j < compw; ++j) {
          dst[i * compw + j] = line_buf[j];
        }
      }
    }
  }

  fclose(fp);
  return EXIT_SUCCESS;
}
