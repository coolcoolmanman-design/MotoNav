#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// Enough of miniz tinfl API for RasterDecoder / parseCover to compile.
// Decompress always fails (TINFL_STATUS_FAILED) — parent may vendor real miniz later.

enum tinfl_status {
  TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS = -4,
  TINFL_STATUS_BAD_PARAM = -3,
  TINFL_STATUS_ADLER32_MISMATCH = -2,
  TINFL_STATUS_FAILED = -1,
  TINFL_STATUS_DONE = 0,
  TINFL_STATUS_NEEDS_MORE_INPUT = 1,
  TINFL_STATUS_HAS_MORE_OUTPUT = 2
};

enum {
  TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
  TINFL_FLAG_HAS_MORE_INPUT = 2,
  TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
  TINFL_FLAG_COMPUTE_ADLER32 = 8
};

struct tinfl_decompressor {
  uint32_t state;
  uint32_t bit_buf;
  int num_bits;
  uint32_t z_adler32;
  // Opaque padding so sizeof matches rough miniz expectation for malloc.
  uint8_t opaque[32768];
};

inline void tinfl_init(tinfl_decompressor *d) {
  if (!d) return;
  std::memset(d, 0, sizeof(*d));
}

#define tinfl_decompressor_size() (sizeof(tinfl_decompressor))

inline tinfl_status tinfl_decompress(tinfl_decompressor *r,
                                     const uint8_t *pIn_buf_next, size_t *pIn_buf_size,
                                     uint8_t *pOut_buf_start, uint8_t *pOut_buf_next,
                                     size_t *pOut_buf_size, uint32_t decomp_flags) {
  (void)r;
  (void)pIn_buf_next;
  (void)pOut_buf_start;
  (void)pOut_buf_next;
  (void)decomp_flags;
  if (pIn_buf_size) *pIn_buf_size = 0;
  if (pOut_buf_size) *pOut_buf_size = 0;
  return TINFL_STATUS_FAILED;
}
