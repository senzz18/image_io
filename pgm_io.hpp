#pragma once
#include "image_io_local.hpp"

class pgm_component : public image_component {
 public:
  pgm_component(uint16_t idx) : image_component(idx) {}
  int read(const std::string &filename) override;
};
