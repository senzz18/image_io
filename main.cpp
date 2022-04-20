#include <chrono>
#include <cstdio>
#include <string>
#if defined(USE_OPENCV)
  #include <opencv2/highgui.hpp>
#endif
#include "image_io.hpp"

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
#if defined(USE_OPENCV)
  cv::Mat test(img.get_component_height(0), img.get_component_width(0), CV_8UC1);
  int32_t *src = img.get_buf(0);
  uint8_t bpp  = (img.get_Ssiz_value(0) & 0x7F) + 1;
  uint8_t s    = (img.get_Ssiz_value(0) & 0x80) >> 7;
  for (int i = 0; i < test.rows; ++i) {
    for (int j = 0; j < test.cols; ++j) {
      if (bpp - 8 > 0) {
        test.data[i * test.cols + j] = (src[0] + (1 << (bpp - 1)) * s) >> (bpp - 8);
      } else {
        test.data[i * test.cols + j] = (src[0] + (1 << (bpp - 1)) * s) << (8 - bpp);
      }
      src++;
    }
  }
  // cv::Mat test(img.get_component_height(0), img.get_component_width(0), CV_8UC3);
  // int32_t *srcR = img.get_buf(0);
  // int32_t *srcG = img.get_buf(1);
  // int32_t *srcB = img.get_buf(2);
  // uint8_t bpp   = (img.get_Ssiz_value(0) & 0x7F) + 1;
  // uint8_t s     = (img.get_Ssiz_value(0) & 0x80) >> 7;
  // for (int i = 0; i < test.rows; ++i) {
  //   for (int j = 0; j < test.cols; ++j) {
  //     test.data[3 * (i * test.cols + j)]     = (srcB[0] + (1 << (bpp - 1)) * s) >> (bpp - 8);
  //     test.data[3 * (i * test.cols + j) + 1] = (srcG[0] + (1 << (bpp - 1)) * s) >> (bpp - 8);
  //     test.data[3 * (i * test.cols + j) + 2] = (srcR[0] + (1 << (bpp - 1)) * s) >> (bpp - 8);
  //     srcB++;
  //     srcG++;
  //     srcR++;
  //   }
  // }
  cv::imshow("Monochrome preview in 8bpp", test);
  cv::waitKey();
  cv::destroyAllWindows();
#endif
  return EXIT_SUCCESS;
}