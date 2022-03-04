#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
  #define USE_ARM_NEON
  #include <arm_neon.h>
#endif

constexpr char SP = ' ';
constexpr char LF = '\n';
constexpr char CR = 0x0d;

enum class status { READ_WIDTH, READ_HEIGHT, READ_MAXVAL, DONE };
enum class imgformat { PGM, PPM, PGX };
// eat white/LF/CR and comments
static auto eat_white = [](int &d, FILE *fp, char *comment) {
  while (d == SP || d == LF || d == CR) {
    d = fgetc(fp);
    if (d == '#') {
      static_cast<void>(fgets(comment, 256, fp));
      d = fgetc(fp);
    }
  }
};

static inline void *aligned_mem_alloc(size_t size, size_t align) {
  void *result;
  if (posix_memalign(&result, align, size)) {
    result = nullptr;
  }
  return result;
}

template <class T>
struct delete_aligned {
  void operator()(T *data) const { free(data); }
};
template <class T>
using unique_ptr_aligned = std::unique_ptr<T, delete_aligned<T>>;
template <class T>
unique_ptr_aligned<T> aligned_uptr(size_t align, size_t size) {
  return unique_ptr_aligned<T>(static_cast<T *>(aligned_mem_alloc(size * sizeof(T), align)));
}

class image_component {
 private:
  uint16_t index;
  uint32_t width;
  uint32_t height;
  uint8_t bits_per_pixel;
  bool is_signed;
  // std::unique_ptr<int32_t[]> buf;
  unique_ptr_aligned<int32_t> buf;

 public:
  image_component(uint16_t c)
      : index(c), width(0), height(0), bits_per_pixel(0), is_signed(false), buf(nullptr) {}
  virtual ~image_component()                    = default;
  virtual int read(const std::string &filename) = 0;
  uint32_t get_width() { return width; }
  uint32_t get_height() { return height; }
  uint8_t get_bpp() { return bits_per_pixel; }
  bool get_is_signed() { return is_signed; }
  int32_t *get_buf(size_t offset = 0) { return buf.get() + offset; }
  void set_index(uint16_t val) { index = val; }
  void set_width(uint32_t val) { width = val; }
  void set_height(uint32_t val) { height = val; }
  void set_bpp(uint8_t val) { bits_per_pixel = val; }
  void set_is_signed(bool val) { is_signed = val; }
  void create_buf(uint32_t val) {
    buf = aligned_uptr<int32_t>(32, val);
    // buf = std::make_unique<int32_t[]>(val);
  }
  auto move_buf() { return std::move(buf); }
};

#if defined(USE_ARM_NEON)
// store uint8x16_t vector as int32
static auto store_u8_to_s32 = [](uint8x16_t &src, int32_t *dst) {
  int16x8_t l = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(src)));
  int16x8_t h = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(src)));
  auto ll     = vmovl_s16(vget_low_s16(l));
  auto lh     = vmovl_s16(vget_high_s16(l));
  auto hl     = vmovl_s16(vget_low_s16(h));
  auto hh     = vmovl_s16(vget_high_s16(h));
  vst1q_s32(dst, ll);
  vst1q_s32(dst + 4, lh);
  vst1q_s32(dst + 8, hl);
  vst1q_s32(dst + 12, hh);
};

// store uint16x8_t vector as int32
static auto store_u16_to_s32 = [](uint16x8_t &src, int32_t *dst) {
  auto little_big = vrev16q_u8(src);  // convert endianness from big to little
  auto x0         = vreinterpretq_s32_s16(little_big);
  auto xl         = vmovl_s16(vreinterpret_s16_s32(vget_low_s32(x0)));
  auto xh         = vmovl_s16(vreinterpret_s16_s32(vget_high_s32(x0)));
  vst1q_s32(dst, xl);
  vst1q_s32(dst + 4, xh);
};
#endif