#pragma once

#include "image_io_local.hpp"
#include "pgm_io.hpp"
#include "pgx_io.hpp"
#include "ppm_io.hpp"

class image {
 private:
  uint32_t width;
  uint32_t height;
  uint16_t num_components;
  std::vector<std::unique_ptr<image_component>> components;
  std::vector<uint32_t> component_width;
  std::vector<uint32_t> component_height;
  // std::unique_ptr<std::unique_ptr<int32_t[]>[]> buf;
  std::unique_ptr<unique_ptr_aligned<int32_t>[]> buf;
  std::vector<uint8_t> bits_per_pixel;
  std::vector<bool> is_signed;

 public:
  explicit image(const std::vector<std::string> &filenames);
  int read_ppm(const std::string &filename, uint16_t compidx);
  uint32_t get_width() const { return this->width; }
  uint32_t get_height() const { return this->height; }
  uint32_t get_component_width(uint16_t c) const;
  uint32_t get_component_height(uint16_t c) const;
  uint16_t get_num_components() const { return this->num_components; }
  uint8_t get_Ssiz_value(uint16_t c) const;
  uint8_t get_max_bpp() const;
  int32_t *get_buf(uint16_t c) const { return this->buf[c].get(); }
};
