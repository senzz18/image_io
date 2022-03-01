#include <chrono>
#include <cstdio>
#include <string>

#include "image_io.hpp"

int main(int argc, char *argv[]) {
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::string> fnames;
  for (int i = 1; i < argc; ++i) {
    fnames.push_back(argv[i]);
  }
  image img(fnames);
  auto duration = std::chrono::high_resolution_clock::now() - start;
  auto count =
      std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  double time = count / 1000.0;
  printf("elapsed time %-15.3lf[ms]\n", time);
  printf("number of components: %d\n", img.get_num_components());
  for (int i = 0; i < img.get_num_components(); ++i) {
    uint8_t bpp = (img.get_Ssiz_value(i) & 0x7F) + 1;
    uint8_t s = (img.get_Ssiz_value(i) & 0x80) >> 7;
    printf("component[%d]: width = %4d, height = %4d, %2d bpp, signed = %d\n",
           i, img.get_component_width(i), img.get_component_height(i), bpp, s);
  }
  // {
  //   constexpr int len = 100;
  //   auto p = aligned_uptr<int32_t>(32, len);
  //   auto pp = p.get();
  //   for (int i = 0; i < len; ++i) {
  //     pp[i] = i;
  //   }
  //   int a = 10;
  // }
  return EXIT_SUCCESS;
}