#pragma once
#include "image_io_local.hpp"

class pgx_component : public image_component {
 public:
  pgx_component(uint16_t idx) : image_component(idx) {}
  int read(const std::string &filename) override;
};