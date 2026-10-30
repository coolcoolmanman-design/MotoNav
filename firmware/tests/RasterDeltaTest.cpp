#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include "../boards/175/MotoNav/RasterDelta.h"

static int checks = 0;
static void check(bool p, const char *msg) {
  if (!p) { std::cerr << "FAIL: " << msg << "\n"; assert(p); }
  ++checks;
}

static void putU16(std::vector<uint8_t> &b, uint16_t v) {
  b.push_back((uint8_t)(v & 255)); b.push_back((uint8_t)(v >> 8));
}
static void putI32(std::vector<uint8_t> &b, int32_t v) {
  uint32_t u = (uint32_t)v;
  for (int i = 0; i < 4; i++) b.push_back((uint8_t)((u >> (8 * i)) & 255));
}
static void putU32(std::vector<uint8_t> &b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back((uint8_t)((v >> (8 * i)) & 255));
}

static std::vector<uint8_t> header(int frame, int x0, int y0, int w, int h, int mm) {
  std::vector<uint8_t> b;
  putU16(b, (uint16_t)frame); putI32(b, x0); putI32(b, y0); putU16(b, (uint16_t)w); putU16(b, (uint16_t)h); putU16(b, (uint16_t)mm);
  return b;
}

int main() {
  const uint32_t maxPx = 1300u * 1300u;
  const size_t cap = ((1300 + 15) / 16) * ((1300 + 15) / 16) * 128;
  const uint32_t maxRaw = 500u * 1024u;

  // Classic RLE (no magic) → KIND_NONE
  {
    auto b = header(1, -1200, 1200, 32, 32, 2000);
    // 1024 pixels of color 0 run — one extended run of 271 then leftovers; simple: 1024 single? use long runs
    int left = 32 * 32;
    while (left > 0) {
      int run = left > 271 ? 271 : left;
      if (run <= 15) b.push_back((uint8_t)(((run - 1) << 4) | 0));
      else { b.push_back((uint8_t)((15 << 4) | 0)); b.push_back((uint8_t)(run - 16)); }
      left -= run;
    }
    RasterMdHeader h;
    check(rasterMdPeek(b.data(), (uint32_t)b.size(), h, maxPx, cap, maxRaw), "RLE header ok");
    check(h.kind == RasterMdHeader::KIND_NONE, "RLE is KIND_NONE");
  }

  // DFL envelope
  {
    auto b = header(7, 0, 0, 16, 16, 2000);
    b.push_back('D'); b.push_back('F'); b.push_back('L'); b.push_back(1);
    putU32(b, 40);
    for (int i = 0; i < 20; i++) b.push_back(0x78); // fake zlib bytes
    RasterMdHeader h;
    check(rasterMdPeek(b.data(), (uint32_t)b.size(), h, maxPx, cap, maxRaw), "DFL peek ok");
    check(h.kind == RasterMdHeader::KIND_DFL && h.version == 1 && h.rawLen == 40, "DFL fields");
    check(h.payloadOff == 24, "DFL payload offset");
  }

  // DXR envelope + shift geometry
  {
    auto b = header(7, 100, 200, 64, 64, 2000);
    size_t bytes = ((64 + 15) / 16) * ((64 + 15) / 16) * 128;
    b.push_back('D'); b.push_back('X'); b.push_back('R'); b.push_back(1);
    putU16(b, (uint16_t)(int16_t)-3); // shiftU
    putU16(b, (uint16_t)(int16_t)5);  // shiftV
    putU32(b, (uint32_t)bytes);
    for (int i = 0; i < 8; i++) b.push_back(1);
    RasterMdHeader h;
    check(rasterMdPeek(b.data(), (uint32_t)b.size(), h, maxPx, cap, maxRaw), "DXR peek ok");
    check(h.kind == RasterMdHeader::KIND_DXR && h.shiftU == -3 && h.shiftV == 5, "DXR shifts");
    check(h.rawLen == bytes && h.payloadOff == 28, "DXR sizes");
  }

  // Shift-copy + XOR reconstructs new tile
  {
    const int W = 32, H = 32, BW = 2;
    size_t bytes = (size_t)BW * 2 * 128;
    std::vector<uint8_t> oldPix(bytes, 0), neu(bytes, 0), base(bytes, 0), xorPlane(bytes, 0);
    // Paint a 2x2 block of color 3 at (4,4) in old
    rasterMdWriteNibble(oldPix.data(), BW, W, 4, 4, 3);
    rasterMdWriteNibble(oldPix.data(), BW, W, 5, 4, 3);
    rasterMdWriteNibble(oldPix.data(), BW, W, 4, 5, 1);
    // New tile is old shifted by (+2,+1): content moves to (6,5)/(7,5)/(6,6)
    int16_t su = 2, sv = 1;
    rasterMdShiftCopy(oldPix.data(), W, H, BW, base.data(), bytes, W, H, BW, su, sv);
    check(rasterMdReadNibble(base.data(), BW, W, H, 6, 5) == 3, "shift copy color");
    check(rasterMdReadNibble(base.data(), BW, W, H, 4, 4) == 0, "old hole cleared");
    // Desired new also has an extra pixel at (0,0)=2
    memcpy(neu.data(), base.data(), bytes);
    rasterMdWriteNibble(neu.data(), BW, W, 0, 0, 2);
    for (size_t i = 0; i < bytes; i++) xorPlane[i] = (uint8_t)(neu[i] ^ base[i]);
    std::vector<uint8_t> out(bytes, 0);
    rasterMdShiftCopy(oldPix.data(), W, H, BW, out.data(), bytes, W, H, BW, su, sv);
    rasterMdXorBytes(out.data(), xorPlane.data(), bytes);
    check(memcmp(out.data(), neu.data(), bytes) == 0, "XOR rebuild matches");
  }

  // World-axis shift helper matches renderer convention
  {
    int16_t su = 0, sv = 0;
    rasterMdShifts(/*old*/0, 100, /*new*/-4, 108, /*mpp*/2.0f, su, sv);
    // shiftU = (0 - (-4))/2 = 2; shiftV = (108 - 100)/2 = 4
    check(su == 2 && sv == 4, "world shift axes");
  }

  // Corrupt DFL rejected
  {
    auto b = header(1, 0, 0, 16, 16, 2000);
    b.push_back('D'); b.push_back('F'); b.push_back('L'); b.push_back(2); // bad ver
    putU32(b, 10);
    RasterMdHeader h;
    check(!rasterMdPeek(b.data(), (uint32_t)b.size(), h, maxPx, cap, maxRaw), "bad DFL ver rejected");
  }

  std::cout << "RasterDelta: " << checks << " checks passed\n";
  return 0;
}
