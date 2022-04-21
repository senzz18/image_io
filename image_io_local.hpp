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
#elif defined(__AVX2__) || defined(__MINGW64__)
  #define USEAVX2
  #if defined(_MSC_VER)
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
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
#if defined(_MSC_VER)
  result = _aligned_malloc(size, align);
#elif defined(__MINGW32__) || defined(__MINGW64__)
  result = __mingw_aligned_malloc(size, align);
#else
  if (posix_memalign(&result, align, size)) {
    result = nullptr;
  }
#endif
  return result;
}

template <class T>
struct delete_aligned {
  void operator()(T *data) const {
#if defined(_MSC_VER)
    _aligned_free(data);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(data);
#else
    free(data);
#endif
  }
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
#elif defined(__AVX2__)
auto load_u8_store_s32 = [](uint8_t const *src, int32_t *const R, int32_t *const G, int32_t *const B) {
  __m128i tmp0, tmp1, tmp2, tmp3;
  alignas(16) static const int8_t mask8_R[16] = {0, 3, 6, 9, 12, 15, 1, 4, 7, 10, 13, 2, 5, 8, 11, 14};
  alignas(16) static const int8_t mask8_G[16] = {2, 5, 8, 11, 14, 0, 3, 6, 9, 12, 15, 1, 4, 7, 10, 13};
  alignas(16) static const int8_t mask8_B[16] = {1, 4, 7, 10, 13, 2, 5, 8, 11, 14, 0, 3, 6, 9, 12, 15};

  __m128i v0 = _mm_loadu_si128((__m128i *)src);
  __m128i v1 = _mm_loadu_si128((__m128i *)(src + 16));
  __m128i v2 = _mm_loadu_si128((__m128i *)(src + 32));

  tmp0 = _mm_shuffle_epi8(v0, *(__m128i *)mask8_R);  // a:0,3,6,9,12,15,1,4,7,10,13,2,5,8,11
  tmp1 = _mm_shuffle_epi8(v1, *(__m128i *)mask8_G);  // b:2,5,8,11,14,0,3,6,9,12,15,1,4,7,10,13
  tmp2 = _mm_shuffle_epi8(v2, *(__m128i *)mask8_B);  // c:1,4,7,10,13,2,5,8,11,14,3,6,9,12,15

  tmp3 = _mm_slli_si128(tmp0, 10);         // 0,0,0,0,0,0,0,0,0,0,a0,a3,a6,a9,a12,a15
  tmp3 = _mm_alignr_epi8(tmp1, tmp3, 10);  // a:0,3,6,9,12,15,b:2,5,8,11,14,x,x,x,x,x
  tmp3 = _mm_slli_si128(tmp3, 5);          // 0,0,0,0,0,a:0,3,6,9,12,15,b:2,5,8,11,14,
  tmp3 = _mm_srli_si128(tmp3, 5);          // a:0,3,6,9,12,15,b:2,5,8,11,14,:0,0,0,0,0
  v0 = _mm_slli_si128(tmp2, 11);           // 0,0,0,0,0,0,0,0,0,0,0,0, 1,4,7,10,13,
  v0 = _mm_or_si128(v0, tmp3);             // a:0,3,6,9,12,15,b:2,5,8,11,14,c:1,4,7,10,13,

  tmp3 = _mm_slli_si128(tmp0, 5);   // 0,0,0,0,0,a:0,3,6,9,12,15,1,4,7,10,13,
  tmp3 = _mm_srli_si128(tmp3, 11);  // a:1,4,7,10,13, 0,0,0,0,0,0,0,0,0,0,0
  v1 = _mm_srli_si128(tmp1, 5);     // b:0,3,6,9,12,15,C:1,4,7,10,13, 0,0,0,0,0
  v1 = _mm_slli_si128(v1, 5);       // 0,0,0,0,0,b:0,3,6,9,12,15,C:1,4,7,10,13,
  v1 = _mm_or_si128(v1, tmp3);      // a:1,4,7,10,13,b:0,3,6,9,12,15,C:1,4,7,10,13,
  v1 = _mm_slli_si128(v1, 5);       // 0,0,0,0,0,a:1,4,7,10,13,b:0,3,6,9,12,15,
  v1 = _mm_srli_si128(v1, 5);       // a:1,4,7,10,13,b:0,3,6,9,12,15,0,0,0,0,0
  tmp3 = _mm_srli_si128(tmp2, 5);   // c:2,5,8,11,14,0,3,6,9,12,15,0,0,0,0,0
  tmp3 = _mm_slli_si128(tmp3, 11);  // 0,0,0,0,0,0,0,0,0,0,0,c:2,5,8,11,14,
  v1 = _mm_or_si128(v1, tmp3);      // a:1,4,7,10,13,b:0,3,6,9,12,15,c:2,5,8,11,14,

  tmp3 = _mm_srli_si128(tmp2, 10);  // c:0,3,6,9,12,15, 0,0,0,0,0,0,0,0,0,0,
  tmp3 = _mm_slli_si128(tmp3, 10);  // 0,0,0,0,0,0,0,0,0,0, c:0,3,6,9,12,15,
  v2 = _mm_srli_si128(tmp1, 11);    // b:1,4,7,10,13,0,0,0,0,0,0,0,0,0,0,0
  v2 = _mm_slli_si128(v2, 5);       // 0,0,0,0,0,b:1,4,7,10,13, 0,0,0,0,0,0
  v2 = _mm_or_si128(v2, tmp3);      // 0,0,0,0,0,b:1,4,7,10,13,c:0,3,6,9,12,15,
  tmp0 = _mm_srli_si128(tmp0, 11);  // a:2,5,8,11,14, 0,0,0,0,0,0,0,0,0,0,0,
  v2 = _mm_or_si128(v2, tmp0);      // a:2,5,8,11,14,b:1,4,7,10,13,c:0,3,6,9,12,15,
  auto Rhigh = _mm_srli_si128(v0, 8);
  auto Rlow = _mm_move_epi64(v0);
  _mm256_storeu_si256((__m256i *)R, _mm256_cvtepi8_epi32(Rlow));
  _mm256_storeu_si256((__m256i *)(R + 8), _mm256_cvtepi8_epi32(Rhigh));
  auto Ghigh = _mm_srli_si128(v1, 8);
  auto Glow = _mm_move_epi64(v1);
  _mm256_storeu_si256((__m256i *)G, _mm256_cvtepi8_epi32(Glow));
  _mm256_storeu_si256((__m256i *)(G + 8), _mm256_cvtepi8_epi32(Ghigh));
  auto Bhigh = _mm_srli_si128(v2, 8);
  auto Blow = _mm_move_epi64(v2);
  _mm256_storeu_si256((__m256i *)B, _mm256_cvtepi8_epi32(Blow));
  _mm256_storeu_si256((__m256i *)(B + 8), _mm256_cvtepi8_epi32(Bhigh));
};

auto load_u16_store_s32 = [](uint16_t const *src, int32_t *const R, int32_t *const G, int32_t *const B) {
  __m128i tmp0, tmp1, tmp2, tmp3;
  alignas(16) static const int8_t mask16_0[16] = {0, 1, 6, 7, 12, 13, 2, 3, 8, 9, 14, 15, 4, 5, 10, 11};
  alignas(16) static const int8_t mask16_1[16] = {2, 3, 8, 9, 14, 15, 4, 5, 10, 11, 0, 1, 6, 7, 12, 13};
  alignas(16) static const int8_t mask16_2[16] = {4, 5, 10, 11, 0, 1, 6, 7, 12, 13, 2, 3, 8, 9, 14, 15};
  alignas(16) static const int8_t count8[2] = {8, 8};
  __m128i v0 = _mm_loadu_si128((__m128i *)(src));       // a0,a1,a2,a3,...a7,
  __m128i v1 = _mm_loadu_si128((__m128i *)(src + 8));   // b0,b1,b2,b3...b7
  __m128i v2 = _mm_loadu_si128((__m128i *)(src + 16));  // c0,c1,c2,c3,...c7

  tmp0 = _mm_shuffle_epi8(v0, *(__m128i *)mask16_0);  // a0,a3,a6,a1,a4,a7,a2,a5,
  tmp1 = _mm_shuffle_epi8(v1, *(__m128i *)mask16_1);  // b1,b4,b7,b2,b5,b0,b3,b6
  tmp2 = _mm_shuffle_epi8(v2, *(__m128i *)mask16_2);  // c2,c5, c0,c3,c6, c1,c4,c7

  tmp3 = _mm_slli_si128(tmp0, 10);         // 0,0,0,0,0,a0,a3,a6,
  tmp3 = _mm_alignr_epi8(tmp1, tmp3, 10);  // a0,a3,a6,b1,b4,b7,x,x
  tmp3 = _mm_slli_si128(tmp3, 4);          // 0,0, a0,a3,a6,b1,b4,b7
  tmp3 = _mm_srli_si128(tmp3, 4);          // a0,a3,a6,b1,b4,b7,0,0
  v0 = _mm_slli_si128(tmp2, 12);           // 0,0,0,0,0,0, c2,c5,
  v0 = _mm_or_si128(v0, tmp3);             // a0,a3,a6,b1,b4,b7,c2,c5
  __m128i v0a = _mm_slli_epi16(v0, 8);
  __m128i v0b = _mm_srli_epi16(v0, 8);
  v0 = _mm_or_si128(v0a, v0b);
  _mm256_stream_si256((__m256i *)R, _mm256_cvtepu16_epi32(v0));

  tmp3 = _mm_slli_si128(tmp0, 4);   // 0,0,a0,a3,a6,a1,a4,a7
  tmp3 = _mm_srli_si128(tmp3, 10);  // a1,a4,a7, 0,0,0,0,0
  v1 = _mm_srli_si128(tmp1, 6);     // b2,b5,b0,b3,b6,0,0
  v1 = _mm_slli_si128(v1, 6);       // 0,0,0,b2,b5,b0,b3,b6,
  v1 = _mm_or_si128(v1, tmp3);      // a1,a4,a7,b2,b5,b0,b3,b6,
  v1 = _mm_slli_si128(v1, 6);       // 0,0,0,a1,a4,a7,b2,b5,
  v1 = _mm_srli_si128(v1, 6);       // a1,a4,a7,b2,b5,0,0,0,
  tmp3 = _mm_srli_si128(tmp2, 4);   // c0,c3,c6, c1,c4,c7,0,0
  tmp3 = _mm_slli_si128(tmp3, 10);  // 0,0,0,0,0,c0,c3,c6,
  v1 = _mm_or_si128(v1, tmp3);      // a1,a4,a7,b2,b5,c0,c3,c6,
  __m128i v1a = _mm_slli_epi16(v1, 8);
  __m128i v1b = _mm_srli_epi16(v1, 8);
  v1 = _mm_or_si128(v1a, v1b);
  _mm256_stream_si256((__m256i *)G, _mm256_cvtepu16_epi32(v1));

  tmp3 = _mm_srli_si128(tmp2, 10);  // c1,c4,c7, 0,0,0,0,0
  tmp3 = _mm_slli_si128(tmp3, 10);  // 0,0,0,0,0, c1,c4,c7,
  v2 = _mm_srli_si128(tmp1, 10);    // b0,b3,b6,0,0, 0,0,0
  v2 = _mm_slli_si128(v2, 4);       // 0,0, b0,b3,b6,0,0,0
  v2 = _mm_or_si128(v2, tmp3);      // 0,0, b0,b3,b6,c1,c4,c7,
  tmp0 = _mm_srli_si128(tmp0, 12);  // a2,a5,0,0,0,0,0,0
  v2 = _mm_or_si128(v2, tmp0);      // a2,a5,b0,b3,b6,c1,c4,c7,
  __m128i v2a = _mm_slli_epi16(v2, 8);
  __m128i v2b = _mm_srli_epi16(v2, 8);
  v2 = _mm_or_si128(v2a, v2b);
  _mm256_stream_si256((__m256i *)B, _mm256_cvtepu16_epi32(v2));
};
#endif