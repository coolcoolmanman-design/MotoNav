#pragma once
// Map-tile Deflate / XOR-delta envelope helpers (PR2). Host-testable; no Arduino types.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// After the standard raster header (16 or 48 bytes), MD payloads replace RLE with:
//   DFL v1: 'D''F''L' 0x01  u32 rawLen  zlib(raw RLE bytes)
//   DXR v1: 'D''X''R' 0x01  i16 shiftU  i16 shiftV  u32 rawLen  zlib(packed-nibble XOR plane)
// Legacy phones keep sending plain RLE; firmware still accepts that path.

struct RasterMdHeader {
  uint16_t frame, width, height, mm;
  int32_t x0, y0;
  uint32_t dataStart;
  int blocksWide;
  size_t bytes;
  bool palette;
  bool buildings;
  enum Kind : uint8_t { KIND_NONE = 0, KIND_DFL = 1, KIND_DXR = 2 } kind;
  uint8_t version;
  int16_t shiftU, shiftV;
  uint32_t payloadOff;
  uint32_t rawLen;
};

static inline uint16_t rasterMdU16(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline int16_t rasterMdI16(const uint8_t *p) {
  return (int16_t)rasterMdU16(p);
}
static inline int32_t rasterMdI32(const uint8_t *p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}
static inline uint32_t rasterMdU32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Parse geometry header. Returns false if the fixed header is corrupt.
// kind==KIND_NONE means "not an MD envelope" (caller should use classic RLE).
// kind!=NONE means MD envelope; returns false if the envelope fields are corrupt.
static inline bool rasterMdPeek(const uint8_t *p, uint32_t len, RasterMdHeader &h,
                                uint32_t maxPixels, size_t capacity, uint32_t maxRaw) {
  memset(&h, 0, sizeof(h));
  if (!p || len < 16) return false;
  h.frame = rasterMdU16(p);
  h.x0 = rasterMdI32(p + 2);
  h.y0 = rasterMdI32(p + 6);
  h.width = rasterMdU16(p + 10);
  h.height = rasterMdU16(p + 12);
  uint16_t flags = rasterMdU16(p + 14);
  h.palette = (flags & 0x8000) != 0;
  h.buildings = (flags & 0x4000) != 0;
  h.mm = flags & 0x3fff;
  if (h.palette && h.buildings) return false;
  uint32_t n = (uint32_t)h.width * h.height;
  if (!h.width || !h.height || !h.mm || n > maxPixels) return false;
  h.blocksWide = (h.width + 15) / 16;
  h.bytes = (size_t)h.blocksWide * ((h.height + 15) / 16) * 128;
  h.dataStart = h.palette ? 48 : 16;
  if (h.bytes > capacity || len < h.dataStart) return false;
  if (len < h.dataStart + 4) { h.kind = RasterMdHeader::KIND_NONE; return true; }
  const uint8_t *m = p + h.dataStart;
  if (m[0] == 'D' && m[1] == 'F' && m[2] == 'L') {
    if (len < h.dataStart + 8) return false;
    h.kind = RasterMdHeader::KIND_DFL;
    h.version = m[3];
    h.rawLen = rasterMdU32(m + 4);
    h.payloadOff = h.dataStart + 8;
    if (h.version != 1 || h.rawLen == 0 || h.rawLen > maxRaw || h.payloadOff >= len) return false;
    return true;
  }
  if (m[0] == 'D' && m[1] == 'X' && m[2] == 'R') {
    if (len < h.dataStart + 12) return false;
    h.kind = RasterMdHeader::KIND_DXR;
    h.version = m[3];
    h.shiftU = rasterMdI16(m + 4);
    h.shiftV = rasterMdI16(m + 6);
    h.rawLen = rasterMdU32(m + 8);
    h.payloadOff = h.dataStart + 12;
    if (h.version != 1 || h.rawLen != h.bytes || h.rawLen > maxRaw || h.payloadOff >= len) return false;
    // Reject shifts with <~67% overlap (width/3). Mid-speed (≥60) large-shift DXR
    // was still accepted under the old 50% gate, NACK-stormed after XOR desync, and
    // left blank tiled blocks while the phone retried. Prefer keep last dense tile
    // (parseRasterDxr returns false without clearing rasterValid) and force DFL/RLE.
    if (h.shiftU <= -(int)h.width / 3 || h.shiftU >= (int)h.width / 3) return false;
    if (h.shiftV <= -(int)h.height / 3 || h.shiftV >= (int)h.height / 3) return false;
    return true;
  }
  h.kind = RasterMdHeader::KIND_NONE;
  return true;
}

static inline void rasterMdWriteNibble(uint8_t *out, int blocksWide, int w, int u, int v, uint8_t color) {
  if (!color) return;
  uint32_t a = ((uint32_t)((v >> 4) * blocksWide + (u >> 4)) << 8) | ((v & 15) << 4) | (u & 15);
  out[a >> 1] |= (a & 1) ? (uint8_t)(color << 4) : color;
}

static inline uint8_t rasterMdReadNibble(const uint8_t *pix, int blocksWide, int w, int h, int u, int v) {
  if ((uint32_t)u >= (uint32_t)w || (uint32_t)v >= (uint32_t)h) return 0;
  uint32_t a = ((uint32_t)((v >> 4) * blocksWide + (u >> 4)) << 8) | ((v & 15) << 4) | (u & 15);
  uint8_t b = pix[a >> 1];
  return (a & 1) ? (uint8_t)(b >> 4) : (uint8_t)(b & 15);
}

// Place old tile pixels into a new buffer with pixel shift (new[u,v] = old[u-shiftU, v-shiftV]).
static inline void rasterMdShiftCopy(const uint8_t *oldPix, int oldW, int oldH, int oldBW,
                                     uint8_t *out, size_t outBytes, int newW, int newH, int newBW,
                                     int shiftU, int shiftV) {
  memset(out, 0, outBytes);
  if (!oldPix || !out || oldW <= 0 || oldH <= 0 || newW <= 0 || newH <= 0) return;
  for (int v = 0; v < newH; v++) {
    int ov = v - shiftV;
    if ((uint32_t)ov >= (uint32_t)oldH) continue;
    for (int u = 0; u < newW; u++) {
      int ou = u - shiftU;
      if ((uint32_t)ou >= (uint32_t)oldW) continue;
      uint8_t c = rasterMdReadNibble(oldPix, oldBW, oldW, oldH, ou, ov);
      rasterMdWriteNibble(out, newBW, newW, u, v, c);
    }
  }
}

static inline void rasterMdXorBytes(uint8_t *dst, const uint8_t *xorPlane, size_t n) {
  for (size_t i = 0; i < n; i++) dst[i] ^= xorPlane[i];
}

// Pixel-space shift matching firmware renderer axes:
//   u = (worldX - x0) / mpp,  v = (y0 - worldY) / mpp
static inline void rasterMdShifts(int32_t oldX0, int32_t oldY0, int32_t newX0, int32_t newY0,
                                  float mpp, int16_t &shiftU, int16_t &shiftV) {
  shiftU = (int16_t)lroundf((oldX0 - newX0) / mpp);
  shiftV = (int16_t)lroundf((newY0 - oldY0) / mpp);
}
