#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
  #define USE_ARM_NEON
  #include <arm_neon.h>
#endif

constexpr char SPC = 0x20;
constexpr char LF  = '\n';
constexpr char CR  = 0x0d;

enum class status { READ_WIDTH, READ_HEIGHT, READ_MAXVAL, DONE };
enum class imgformat { PGM, PPM, PGX };
class openhtj2k_file {
 private:
  int fd;
  size_t pos;
  bool is_open;

 public:
  openhtj2k_file() : fd(-1), pos(0), is_open(false){};
  bool open(const char *fname) {
    fd = ::open(fname, O_RDONLY, 00644);
    if (fd != -1) {
      is_open = true;
    }
    return is_open;
  }
  bool close() {
    if (is_open) {
      ::close(fd);
    }
    return true;
  }
  int fgetc() {
    uint8_t val = -1;
    if (!is_open) {
      return -1;
    }
    ::read(fd, &val, (size_t)1);
    pos++;
    return (int)val;
  }
  int read(uint8_t *buf, size_t numbytes) {
    if (!is_open) {
      return -1;
    }
    int total_bytes_read = 0;
    do {
      int num_rd;
      num_rd = (int)::read(fd, buf, numbytes);
      if (num_rd == 0) break;
      pos += num_rd;
      numbytes -= num_rd;
      buf += num_rd;
      total_bytes_read += num_rd;
    } while (numbytes > 0);
    return total_bytes_read;
  }
  int64_t get_pos() {
    if (!is_open) return 0;
    return (int64_t)::lseek(fd, 0, SEEK_CUR);
  }
  int64_t seek(int64_t pos) {
    if (!is_open) return 0;
    if (pos < 0)
      return (int64_t)::lseek(fd, 0, SEEK_END);
    else {
      off_t offset = (off_t)pos;
      if (pos != (int64_t)offset) {
        offset = INT32_MAX;
      };
      return (int64_t)::lseek(fd, offset, SEEK_SET);
    }
  }
};
// eat white/LF/CR and comments
static auto eat_white = [](int &d, FILE *fp, char *comment) {
  while (d == SPC || d == LF || d == CR) {
    d = fgetc(fp);
    if (d == '#') {
      static_cast<void>(fgets(comment, 256, fp));
      d = fgetc(fp);
    }
  }
};
static auto eat_white_nb = [](int &d, openhtj2k_file *fp, char *comment) {
  while (d == SPC || d == LF || d == CR) {
    d = fp->fgetc();
    if (d == '#') {
      // static_cast<void>(fgets(comment, 256, fp));
      do {
        d = fp->fgetc();
      } while (d != LF || d != CR);
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

// store int8x16_t vector as int32
static auto store_s8_to_s32 = [](int8x16_t &src, int32_t *dst) {
  int16x8_t l = vreinterpretq_s16_u16(vmovl_s8(vget_low_s8(src)));
  int16x8_t h = vreinterpretq_s16_u16(vmovl_s8(vget_high_s8(src)));
  auto ll     = vmovl_s16(vget_low_s16(l));
  auto lh     = vmovl_s16(vget_high_s16(l));
  auto hl     = vmovl_s16(vget_low_s16(h));
  auto hh     = vmovl_s16(vget_high_s16(h));
  vst1q_s32(dst, ll);
  vst1q_s32(dst + 4, lh);
  vst1q_s32(dst + 8, hl);
  vst1q_s32(dst + 12, hh);
};

// store big-endian uint16x8_t vector as int32
static auto store_big_u16_to_s32 = [](uint16x8_t &src, int32_t *dst) {
  auto little_big = vrev16q_u8(src);  // convert endianness from big to little
  auto x0         = vreinterpretq_s32_u16(little_big);
  auto xl         = vmovl_s16(vreinterpret_s16_s32(vget_low_s32(x0)));
  auto xh         = vmovl_s16(vreinterpret_s16_s32(vget_high_s32(x0)));
  vst1q_s32(dst, xl);
  vst1q_s32(dst + 4, xh);
};

// store little-endian uint16x8_t vector as int32
static auto store_little_u16_to_s32 = [](uint16x8_t &src, int32_t *dst) {
  auto x0 = vreinterpretq_s32_u16(vreinterpretq_u16_u8(src));
  auto xl = vmovl_s16(vreinterpret_s16_s32(vget_low_s32(x0)));
  auto xh = vmovl_s16(vreinterpret_s16_s32(vget_high_s32(x0)));
  vst1q_s32(dst, xl);
  vst1q_s32(dst + 4, xh);
};

// store big-endian int16x8_t vector as int32
static auto store_big_s16_to_s32 = [](uint16x8_t &src, int32_t *dst) {
  auto x0 = vreinterpretq_s32_s16(vreinterpretq_s16_u8(vrev16q_u8(src)));
  auto xl = vmovl_s16(vreinterpret_s16_s32(vget_low_s32(x0)));
  auto xh = vmovl_s16(vreinterpret_s16_s32(vget_high_s32(x0)));
  vst1q_s32(dst, xl);
  vst1q_s32(dst + 4, xh);
};

// store little-endian int16x8_t vector as int32
static auto store_little_s16_to_s32 = [](uint16x8_t &src, int32_t *dst) {
  auto x0 = vreinterpretq_s32_s16(vreinterpretq_s16_u8(src));
  auto xl = vmovl_s16(vreinterpret_s16_s32(vget_low_s32(x0)));
  auto xh = vmovl_s16(vreinterpret_s16_s32(vget_high_s32(x0)));
  vst1q_s32(dst, xl);
  vst1q_s32(dst + 4, xh);
};
#endif