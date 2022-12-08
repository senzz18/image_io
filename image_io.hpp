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
  explicit image(uint32_t w, uint32_t h, uint16_t nc, uint8_t bpp, bool issigned) {
    width          = w;
    height         = h;
    num_components = nc;
    this->buf      = std::make_unique<unique_ptr_aligned<int32_t>[]>(this->num_components);
    for (int c = 0; c < num_components; ++c) {
      component_width.push_back(width);
      component_height.push_back(height);
      bits_per_pixel.push_back(bpp);
      is_signed.push_back(issigned);
      this->buf[c] = aligned_uptr<int32_t>(32, width * height);
    }
  }
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
