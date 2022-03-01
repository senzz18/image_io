#include <chrono>
#include <cstdio>
#include <string>
#include <opencv2/opencv.hpp>
#include "image_io.hpp"
using namespace cv;
int main(int argc, char *argv[]) {
  auto start = std::chrono::high_resolution_clock::now();
  std::vector<std::string> fnames;
  for (int i = 1; i < argc; ++i) {
    fnames.push_back(argv[i]);
  }
  image img(fnames);
  auto duration = std::chrono::high_resolution_clock::now() - start;
  auto count    = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  double time   = count / 1000.0;
  printf("elapsed time %-15.3lf[ms]\n", time);
  printf("number of components: %d\n", img.get_num_components());
  for (int i = 0; i < img.get_num_components(); ++i) {
    uint8_t bpp = (img.get_Ssiz_value(i) & 0x7F) + 1;
    uint8_t s   = (img.get_Ssiz_value(i) & 0x80) >> 7;
    printf("component[%d]: width = %4d, height = %4d, %2d bpp, signed = %d\n", i,
           img.get_component_width(i), img.get_component_height(i), bpp, s);
  }
  cv::Mat test(img.get_component_height(0), img.get_component_width(0), CV_8UC1);
  int32_t *src = img.get_buf(1);
  uint8_t bpp  = (img.get_Ssiz_value(0) & 0x7F) + 1;
  for (int i = 0; i < test.rows; ++i) {
    for (int j = 0; j < test.cols; ++j) {
      test.data[i * test.cols + j] = src[0] >> (bpp - 8);
      src++;
    }
  }
  cv::imshow("Monochrome preview in 8bpp", test);
  cv::waitKey();
  cv::destroyAllWindows();
  return EXIT_SUCCESS;
}