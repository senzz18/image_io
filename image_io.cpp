#include "image_io.hpp"

#include <algorithm>

image::image(const std::vector<std::string> &filenames)
    : width(0), height(0), buf(nullptr) {
  size_t num_files = filenames.size();
  if (num_files > 16384) {
    printf("ERROR: over 16384 components are not supported in the spec.\n");
    throw std::exception();
  }
  num_components = 0;
  for (const auto &fname : filenames) {
    size_t ext_pos = fname.find_last_of(".");
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
    this->buf =
        std::make_unique<unique_ptr_aligned<int32_t>[]>(this->num_components);
  }
  components.reserve(num_components);
  uint16_t c = 0;
  imgformat format;
  for (const auto &fname : filenames) {
    size_t ext_pos = fname.find_last_of(".");
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
        component_width.push_back(
            components[components.size() - 1]->get_width());
        component_height.push_back(
            components[components.size() - 1]->get_height());
        bits_per_pixel.push_back(components[components.size() - 1]->get_bpp());
        is_signed.push_back(components[components.size() - 1]->get_is_signed());
        this->buf[components.size() - 1] =
            components[components.size() - 1]->move_buf();
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
        component_width.push_back(
            components[components.size() - 1]->get_width());
        component_height.push_back(
            components[components.size() - 1]->get_height());
        bits_per_pixel.push_back(components[components.size() - 1]->get_bpp());
        is_signed.push_back(components[components.size() - 1]->get_is_signed());
        this->buf[components.size() - 1] =
            components[components.size() - 1]->move_buf();
        c++;
        break;
      deafult:
        printf("ERROR\n");
    }
  }
  width = *std::max_element(component_width.begin(), component_width.end());
  height = *std::max_element(component_height.begin(), component_height.end());
}

int image::read_ppm(const std::string &filename, uint16_t compidx) {
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
      isPPM = true;
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
        st = status::READ_HEIGHT;
        break;
      case status::READ_HEIGHT:
        this->components[compidx]->set_height(val);
        this->components[compidx + 1]->set_height(val);
        this->components[compidx + 2]->set_height(val);
        val = 0;
        st = status::READ_MAXVAL;
        break;
      case status::READ_MAXVAL:
        this->components[compidx]->set_bpp(
            static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        this->components[compidx + 1]->set_bpp(
            static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        this->components[compidx + 2]->set_bpp(
            static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f));
        val = 0;
        st = status::DONE;
        break;
      default:
        break;
    }
  }
  eat_white(d, fp, comment);
  fseek(fp, -1, SEEK_CUR);

  const uint32_t byte_per_sample = (components[compidx]->get_bpp() + 8 - 1) / 8;
  const uint32_t component_gap = 3 * byte_per_sample;
  const uint32_t compw = components[compidx]->get_width();
  const uint32_t comph = components[compidx]->get_height();
  const uint32_t line_width = component_gap * compw;
  std::unique_ptr<uint8_t[]> line_buf = std::make_unique<uint8_t[]>(line_width);

  // allocate memory
  for (size_t i = compidx; i < compidx + 3; ++i) {
    components[i]->create_buf(compw * comph);
    //   this->buf[i] = std::make_unique<int32_t[]>(compw * comph);
  }

  if (!isASCII) {
    for (size_t i = 0; i < comph; ++i) {
      if (fread(line_buf.get(), sizeof(uint8_t), line_width, fp) < line_width) {
        printf("ERROR: not enough samples in the given pnm file.\n");
        fclose(fp);
        return EXIT_FAILURE;
      }
      for (size_t c = compidx; c < compidx + 3; ++c) {
        uint8_t *src;
        int32_t *dst;
        src = &line_buf[c * byte_per_sample];

        dst = components[c]->get_buf(i * compw);  //  &this->buf[c][i * compw];

        switch (byte_per_sample) {
          case 1:
            for (size_t j = 0; j < compw; ++j) {
              *dst = (isSigned) ? static_cast<int8_t>(*src) : *src;
              dst++;
              src += component_gap;
            }
            break;
          case 2:
            if (isSigned) {
              if (isBigendian) {
                for (size_t j = 0; j < compw; ++j) {
                  *dst = static_cast<int_least16_t>(
                      (static_cast<uint_least16_t>(src[0]) << 8) |
                      static_cast<uint_least16_t>(src[1]));
                  dst++;
                  src += component_gap;
                }
              } else {
                for (size_t j = 0; j < compw; ++j) {
                  *dst = static_cast<int_least16_t>(
                      static_cast<uint_least16_t>(src[0]) |
                      (static_cast<uint_least16_t>(src[1]) << 8));
                  dst++;
                  src += component_gap;
                }
              }
            } else {
              if (isBigendian) {
                for (size_t j = 0; j < compw; ++j) {
                  *dst = (src[0] << 8) | src[1];
                  dst++;
                  src += component_gap;
                }
              } else {
                for (size_t j = 0; j < compw; ++j) {
                  *dst = src[0] | (src[1] << 8);
                  dst++;
                  src += component_gap;
                }
              }
            }
            break;
          default:
            printf("ERROR: bit-depth over 16 is not supported.\n");
            fclose(fp);
            return EXIT_FAILURE;
            break;
        }
      }
    }
  } else {
    for (size_t i = 0; i < compw * comph; ++i) {
      for (size_t c = compidx; c < compidx + 3; ++c) {
        int32_t *dst = components[c]->get_buf(i);
        val = 0;
        d = fgetc(fp);
        while (d != SP && d != CR && d != LF && d != EOF) {
          val *= 10;
          val += d - '0';
          d = fgetc(fp);
        }
        dst[0] = val;
      }
    }
  }
  fclose(fp);
  return EXIT_SUCCESS;
}

uint32_t image::get_component_width(uint16_t c) const {
  if (c > num_components) {
    printf("ERROR: component index %d is larger than maximum value %d.\n", c,
           num_components);
    throw std::exception();
  }
  return this->component_width[c];
}

uint32_t image::get_component_height(uint16_t c) const {
  if (c > num_components) {
    printf("ERROR: component index %d is larger than maximum value %d.\n", c,
           num_components);
    throw std::exception();
  }
  return this->component_height[c];
}

uint8_t image::get_Ssiz_value(uint16_t c) const {
  return (this->is_signed[c]) ? (this->bits_per_pixel[c] - 1) | 0x80
                              : this->bits_per_pixel[c] - 1;
}

uint8_t image::get_max_bpp() const {
  uint8_t max = 0;
  for (auto &v : bits_per_pixel) {
    max = (max < v) ? v : max;
  }
  return max;
}