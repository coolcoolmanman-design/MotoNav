#pragma once
// 位图地图及罪恶都市轮廓解码。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v2.5 解析图片地图：u16 坐标系, i32 左上角 x, i32 左上角 y, u16 宽, u16 高, u16 每像素毫米, 然后是压缩的颜色编号
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t);   // v6.1 后面定义
bool rasHasPal = false;                    // v6.1 卫星图自带调色板
uint16_t rasPal[16];

// 罪恶都市局部边界几何；其它风格仍读取原来的 4 位材质图。
uint16_t *rasterVice = nullptr;
size_t rasterAllocatedBytes = RASTER_BUF_BYTES;
bool rasterViceValid = false;
tinfl_decompressor *viceInflater = nullptr;
uint16_t viceEdgePal[7][257];
uint16_t vicePalKey[4] = {0xffff, 0xffff, 0xffff, 0xffff};
// 覆盖率按像素方形与半平面的相交面积计算。利用镜像对称，只需保存九个角度的表。
int8_t viceNX[64], viceNY[64];
int32_t viceOffsets[2][32];
uint16_t viceCoverage[9][257];
bool rasterViceFine = false;
uint8_t viceBaseShift = 13;
uint8_t rasterViceVersion = 0;
const uint16_t *vicePatchData = nullptr;
uint16_t vicePatchCount = 0;

void initViceSampling() {
  static bool ready = false;
  if (ready) return;
  for (int i = 0; i < 64; i++) {
    float a = i * (2.0f * PI / 64.0f);
    viceNX[i] = (int8_t)lroundf(cosf(a) * 127);
    viceNY[i] = (int8_t)lroundf(sinf(a) * 127);
  }
  const float offsets[16] = {0.0f, 0.0625f, 0.125f, 0.1875f,
    0.25f, 0.3125f, 0.375f, 0.4375f, 0.5f, 0.5625f,
    0.625f, 0.6875f, 0.8125f, 1.0f, 1.25f, 1.625f};
  for (int i = 0; i < 32; i++) {
    viceOffsets[0][i] = (i < 16 ? i : i - 32) * 4064;
    viceOffsets[1][i] = (int32_t)lroundf(offsets[i & 15] * (i & 16 ? -32512.0f : 32512.0f));
  }
  for (int angle = 0; angle <= 8; angle++) {
    float a = angle * (PI / 32.0f);
    float major = cosf(a), minor = sinf(a);
    for (int i = 0; i <= 256; i++) {
      float distance = (i - 128) * (1.5f / 256.0f);
      float t = distance + (major + minor) * 0.5f;
      float coverage;
      if (t <= 0) coverage = 0;
      else if (t >= major + minor) coverage = 1;
      else if (minor < 0.00001f) coverage = t / major;
      else if (t < minor) coverage = t * t / (2 * major * minor);
      else if (t <= major) coverage = (t - minor * 0.5f) / major;
      else { float tail = major + minor - t; coverage = 1 - tail * tail / (2 * major * minor); }
      viceCoverage[angle][i] = (uint16_t)max(0, min(256, (int)lroundf(coverage * 256.0f)));
    }
  }
  ready = true;
}

static inline uint8_t viceCoverageAngle(float nx, float ny) {
  int angle = (int)lroundf(atan2f(fabsf(ny), fabsf(nx)) * (32.0f / PI));
  if (angle > 8) angle = 16 - angle;
  return (uint8_t)max(0, min(8, angle));
}


void parseViceGeometry(const uint8_t *p, uint32_t len, uint32_t pos, int bw, int bh) {
  rasterViceValid = false;
  vicePatchCount = 0; vicePatchData = nullptr;
  if (pos <= len && len - pos >= 8 && p[pos] == 'V' && p[pos + 1] == 'C' && p[pos + 2] == 'E' &&
      p[pos + 3] >= 1 && p[pos + 3] <= 3) {
    uint8_t version = p[pos + 3];
    uint32_t packed = (uint32_t)rd32(p + pos + 4);
    uint32_t header = version == 3 ? 12 : 8;
    if (len - pos < header) return;
    uint16_t patchCount = version == 3 ? rdu16(p + pos + 8) : 0;
    if (patchCount > 2047 || (version == 3 && rdu16(p + pos + 10) != 0)) return;
    size_t cellsBytes = (size_t)bw * bh * 512;
    size_t needed = cellsBytes + (size_t)patchCount * 8;
    if (packed == len - pos - header && needed <= RASTER_BUF_BYTES * 4 + 2047 * 8) {
      if (rasterAllocatedBytes != needed) {
        free(rasterBuf);
        rasterBuf = (uint8_t *)ps_malloc(needed);
        rasterAllocatedBytes = rasterBuf ? needed : 0;
      }
      rasterVice = (uint16_t *)rasterBuf;
      if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
      if (rasterVice && rasterAllocatedBytes == needed && viceInflater) {
        tinfl_init(viceInflater);
        size_t inBytes = packed, outBytes = needed;
        tinfl_status result = tinfl_decompress(viceInflater, p + pos + header, &inBytes,
            (uint8_t *)rasterVice, (uint8_t *)rasterVice, &outBytes,
            TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        rasterViceValid = result == TINFL_STATUS_DONE && inBytes == packed && outBytes == needed;
        const uint16_t *vertices = (const uint16_t *)(rasterBuf + cellsBytes);
        if (rasterViceValid) {
          for (uint32_t i = 0; i < (uint32_t)patchCount * 4; i++) {
            uint16_t v = vertices[i];
            if ((v & 15) + ((v >> 4) & 15) + ((v >> 8) & 15) + (v >> 12) != 15) {
              rasterViceValid = false; break;
            }
          }
        }
        if (rasterViceValid) {
          rasterViceVersion = version;
          rasterViceFine = version >= 2;
          viceBaseShift = rasterViceFine ? 14 : 13;
          vicePatchCount = patchCount; vicePatchData = patchCount ? vertices : nullptr;
        }
      }
    }
  }
  if (!rasterViceValid) rasterVice = nullptr;
}

void prepareVicePalette(const uint16_t *pal) {
  static bool initialized = false;
  initViceSampling();
  bool changed = !initialized;
  for (int i = 0; i < 4; i++) if (vicePalKey[i] != pal[i]) changed = true;
  if (!changed) return;
  const uint8_t pairA[7] = {0, 0, 0, 0, 1, 1, 2};
  const uint8_t pairB[7] = {0, 1, 2, 3, 2, 3, 3};
  for (int p = 1; p <= 6; p++) for (int a = 0; a <= 256; a++)
    viceEdgePal[p][a] = blendF(pal[pairA[p]], pal[pairB[p]], a);
  memcpy(vicePalKey, pal, sizeof(vicePalKey));
  initialized = true;
}

void parseRasterLegacy(const uint8_t *p, uint32_t len) {
  rasterValid = false;
  rasterViceValid = false;
  if (len < 16) return;
  int frame = rdu16(p);
  int32_t x0 = rd32(p + 2), y0 = rd32(p + 6);
  int w = rdu16(p + 10), h = rdu16(p + 12), mm = rdu16(p + 14);
  bool hasPal = (mm & 0x8000) != 0;
  mm &= 0x7FFF;
  uint16_t palNew[16];
  if (hasPal) {
    if (len < 48) return;
    for (int i = 0; i < 16; i++) palNew[i] = rdu16(p + 16 + 2 * i);
  }
  uint32_t n = (uint32_t)w * h;
  if (w == 0 || h == 0 || n > RASTER_MAX_PX || mm == 0) return;
  int bw = (w + 15) >> 4, bh = (h + 15) >> 4;
  size_t legacyBytes = (size_t)bw * bh * 128;
  if (legacyBytes > RASTER_BUF_BYTES) return;
  uint32_t dataStart = hasPal ? 48 : 16;
  // 先检查普通图的游程长度，找到扩展的位置；不另存一份解压后的普通图。
  uint32_t pos = dataStart, i = 0;
  while (pos < len && i < n) {
    uint8_t b = p[pos++], r = b >> 4;
    uint32_t run = r + 1;
    if (r == 15) { if (pos >= len) return; run = 16 + p[pos++]; }
    if (run > n - i) return;
    i += run;
  }
  if (i != n) return;
  if (!hasPal) parseViceGeometry(p, len, pos, bw, bh);
  if (!rasterViceValid) {
    // 不带扩展的地图、卫星图或扩展解码失败，回到原来的 4 位格式。
    if (!rasterBuf || rasterAllocatedBytes != RASTER_BUF_BYTES) {
      free(rasterBuf);
      rasterBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
      rasterAllocatedBytes = rasterBuf ? RASTER_BUF_BYTES : 0;
    }
    rasterVice = nullptr;
    if (!rasterBuf) return;
    memset(rasterBuf, 0, legacyBytes);
    pos = dataStart; i = 0;
    int x = 0, y = 0;
    while (i < n) {
      uint8_t b = p[pos++], c = b & 15, r = b >> 4;
      uint32_t run = r + 1;
      if (r == 15) run = 16 + p[pos++];
      if (c) {
        for (uint32_t k = 0; k < run; k++) {
          uint32_t a = ((uint32_t)((y >> 4) * bw + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
          rasterBuf[a >> 1] |= (a & 1) ? (c << 4) : c;
          if (++x == w) { x = 0; y++; }
        }
      } else {
        x += run;
        while (x >= w) { x -= w; y++; }
      }
      i += run;
    }
  }
  rasBW = bw;
  rasX0 = x0; rasY0 = y0; rasW = w; rasH = h; rasMpp = mm / 1000.0f; rasterFrame = frame;
  rasHasPal = hasPal;
  if (hasPal) memcpy(rasPal, palNew, sizeof(rasPal));
  rasterValid = true;
  rasterAt = millis();
}

#include "../../RasterDelta.h"

tinfl_decompressor *mapInflater = nullptr;
uint8_t *mapScratch = nullptr;
size_t mapScratchCap = 0;

static bool mapEnsureScratch(size_t need) {
  if (mapScratch && mapScratchCap >= need) return true;
  free(mapScratch);
  mapScratch = (uint8_t *)ps_malloc(need);
  mapScratchCap = mapScratch ? need : 0;
  return mapScratch != nullptr;
}

static bool mapInflateZlib(const uint8_t *in, size_t inLen, uint8_t *out, size_t expectOut) {
  if (!mapInflater) mapInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!mapInflater || !out || expectOut == 0) return false;
  tinfl_init(mapInflater);
  size_t inBytes = inLen, outBytes = expectOut;
  tinfl_status st = tinfl_decompress(mapInflater, in, &inBytes, out, out, &outBytes,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  return st == TINFL_STATUS_DONE && inBytes == inLen && outBytes == expectOut;
}

static void rasterCommitSwap(const RasterHeader &next, const uint8_t *p, bool keepOld) {
  RasterHeader old = {};
  old.frame = rasterFrame; old.x0 = rasX0; old.y0 = rasY0; old.width = rasW; old.height = rasH;
  old.mm = (uint16_t)lroundf(rasMpp * 1000); old.blocksWide = rasBW;
  bool keep = keepOld && rasterValid && !rasterViceValid && !rasHasPal && !next.palette &&
      rasterFrame == next.frame && old.mm == next.mm && rasterAllocatedBytes == RASTER_BUF_BYTES;
  uint8_t *oldPixels = rasterBuf;
  rasterBuf = rasterBackBuf;
  rasterBackBuf = oldPixels;
  if (rasterAllocatedBytes != RASTER_BUF_BYTES) { free(rasterBackBuf); rasterBackBuf = nullptr; keep = false; }
  rasterAllocatedBytes = RASTER_BUF_BYTES;
  rasterBackHeader = old; rasterBackValid = keep;
  rasterVice = nullptr; rasterViceValid = false; vicePatchData = nullptr; vicePatchCount = 0;
  rasBW = next.blocksWide; rasX0 = next.x0; rasY0 = next.y0; rasW = next.width; rasH = next.height;
  rasMpp = next.mm / 1000.0f; rasterFrame = next.frame; rasHasPal = next.palette;
  if (next.palette) for (int i = 0; i < 16; i++) rasPal[i] = rasterU16(p + 16 + 2 * i);
  rasterValid = true; rasterAt = millis();
}

// Gate empty material planes before swap. Does not clear rasterValid on reject.
static bool rasterAcceptDecoded(const RasterHeader &next, const uint8_t *decoded) {
  if (next.palette) return true;   // satellite / custom palette always has content intent
  size_t neu = rasterNonZeroNibbles(decoded, next.bytes);
  size_t oldBytes = rasterValid && rasterBuf && rasBW > 0 && rasH > 0
      ? (size_t)rasBW * ((size_t)(rasH + 15) / 16) * 128 : 0;
  size_t old = oldBytes ? rasterNonZeroNibbles(rasterBuf, oldBytes) : 0;
  if (rasterTooEmptyToReplace(neu, next.width, next.height, old, rasterValid && !rasterViceValid && !rasHasPal))
    return false;
  return true;
}

static bool parseRasterDfl(const uint8_t *p, uint32_t len, const RasterMdHeader &md) {
  size_t packetBytes = (size_t)md.dataStart + md.rawLen;
  if (!mapEnsureScratch(packetBytes)) return false;
  memcpy(mapScratch, p, md.dataStart);
  if (!mapInflateZlib(p + md.payloadOff, len - md.payloadOff, mapScratch + md.dataStart, md.rawLen)) return false;
  RasterHeader next;
  if (!rasterCheck(mapScratch, (uint32_t)packetBytes, next, RASTER_MAX_PX, RASTER_BUF_BYTES)) return false;
  if (next.dataEnd != packetBytes) return false;
  if (!rasterBackBuf) rasterBackBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  if (!rasterBackBuf) return false;
  rasterDecode(mapScratch, next, rasterBackBuf);
  if (!rasterAcceptDecoded(next, rasterBackBuf)) return false;
  rasterCommitSwap(next, p, true);
  return true;
}

static bool parseRasterDxr(const uint8_t *p, uint32_t len, const RasterMdHeader &md) {
  // Need a prior same-frame / same-mm / same-size tile to XOR against.
  // On any failure return false without clearing rasterValid — keep the last
  // good base tile on screen while the phone falls back to DFL/RLE.
  if (!rasterValid || rasterViceValid || rasHasPal || md.palette || md.buildings) return false;
  if (rasterFrame != md.frame || rasW != md.width || rasH != md.height) return false;
  if ((uint16_t)lroundf(rasMpp * 1000) != md.mm) return false;
  if (rasterAllocatedBytes != RASTER_BUF_BYTES || !rasterBuf) return false;
  // Mirror rasterMdPeek ≥50% overlap gate (defense in depth).
  if (md.shiftU <= -(int)md.width / 2 || md.shiftU >= (int)md.width / 2) return false;
  if (md.shiftV <= -(int)md.height / 2 || md.shiftV >= (int)md.height / 2) return false;
  if (!mapEnsureScratch(md.rawLen)) return false;
  if (!mapInflateZlib(p + md.payloadOff, len - md.payloadOff, mapScratch, md.rawLen)) return false;
  if (!rasterBackBuf) rasterBackBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  if (!rasterBackBuf) return false;
  rasterMdShiftCopy(rasterBuf, rasW, rasH, rasBW, rasterBackBuf, RASTER_BUF_BYTES,
                    md.width, md.height, md.blocksWide, md.shiftU, md.shiftV);
  rasterMdXorBytes(rasterBackBuf, mapScratch, md.bytes);
  RasterHeader next = {};
  next.frame = md.frame; next.x0 = md.x0; next.y0 = md.y0;
  next.width = md.width; next.height = md.height; next.mm = md.mm;
  next.blocksWide = md.blocksWide; next.bytes = md.bytes;
  next.palette = md.palette; next.buildings = md.buildings;
  next.dataStart = md.dataStart; next.dataEnd = len;
  if (!rasterAcceptDecoded(next, rasterBackBuf)) return false;
  rasterCommitSwap(next, p, true);
  return true;
}

// 普通地图先在备用缓冲解码，成功后交换；上一块保留，用来补齐新块的覆盖边缘。
// PR2：在 RLE 之前识别 DFL（二次 Deflate）与 DXR（XOR 增量）；旧手机仍走 RLE。
bool parseRaster(const uint8_t *p, uint32_t len) {
  RasterMdHeader md;
  if (!rasterMdPeek(p, len, md, RASTER_MAX_PX, RASTER_BUF_BYTES, RASTER_MAX_BYTES)) return false;
  if (md.kind == RasterMdHeader::KIND_DFL) return parseRasterDfl(p, len, md);
  if (md.kind == RasterMdHeader::KIND_DXR) return parseRasterDxr(p, len, md);

  RasterHeader next;
  if (!rasterCheck(p,len,next,RASTER_MAX_PX,RASTER_BUF_BYTES)) return false;
  if (next.dataEnd != len) {
    // 保留既有轮廓扩展的兼容路径；新 App 只发送无扩展的普通底图。
    if (len-next.dataEnd<8 || p[next.dataEnd]!='V' || p[next.dataEnd+1]!='C' || p[next.dataEnd+2]!='E') return false;
    rasterBackValid=false;
    parseRasterLegacy(p,len);
    return rasterValid;
  }
  if (!rasterBackBuf) rasterBackBuf=(uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  if (!rasterBackBuf) return false;
  rasterDecode(p,next,rasterBackBuf);
  if (!rasterAcceptDecoded(next, rasterBackBuf)) return false;
  rasterCommitSwap(next, p, true);
  return true;
}

// ===== v7.12.10-cover 音乐封面（bulk type 5）=====
// 载荷：'C''V''R' 版本(1)=1 宽(1) 高(1) + zlib 压缩的 RGB565（小端）。宽高 0 表示清除封面。
// v7.12.11-opt.6：收包后强制灰度。协议头不变。
// v7.12.11-opt.7：解码后下采样到 COVER_ATM_PX、3×3 box blur、16 阶 Bayer 抖动；绘制用软斑而非全屏上采样。
tinfl_decompressor *coverInflater = nullptr;
static inline uint16_t coverToGray565(uint16_t c) {
  uint32_t R = ((c >> 11) & 31); R = (R << 3) | (R >> 2);
  uint32_t G = ((c >> 5) & 63);  G = (G << 2) | (G >> 4);
  uint32_t B = (c & 31);         B = (B << 3) | (B >> 2);
  uint32_t Y = (77 * R + 150 * G + 29 * B) >> 8;
  uint16_t y5 = (uint16_t)(Y >> 3), y6 = (uint16_t)(Y >> 2);
  return (uint16_t)((y5 << 11) | (y6 << 5) | y5);
}
static inline uint8_t coverLuma8(uint16_t c) {
  // gray565 → 8-bit luma（R5≈G6≈B5）
  return (uint8_t)(((c >> 5) & 63) << 2);
}
static inline uint16_t coverGrayFrom8(uint8_t Y) {
  uint16_t y5 = (uint16_t)(Y >> 3), y6 = (uint16_t)(Y >> 2);
  return (uint16_t)((y5 << 11) | (y6 << 5) | y5);
}
// 解码灰度图 → 64² 模糊 + 16 阶有序抖动，写入 coverImg 前 COVER_ATM_PX²。
static void coverPrepareAtmosphere(int w, int h) {
  static uint8_t src[COVER_ATM_PX * COVER_ATM_PX];
  static uint8_t blur[COVER_ATM_PX * COVER_ATM_PX];
  const int A = COVER_ATM_PX;
  // 盒式下采样到 64²（兼容手机仍发 128）
  for (int y = 0; y < A; y++) {
    int y0 = y * h / A, y1 = (y + 1) * h / A;
    if (y1 <= y0) y1 = y0 + 1;
    if (y1 > h) y1 = h;
    for (int x = 0; x < A; x++) {
      int x0 = x * w / A, x1 = (x + 1) * w / A;
      if (x1 <= x0) x1 = x0 + 1;
      if (x1 > w) x1 = w;
      uint32_t sum = 0, cnt = 0;
      for (int yy = y0; yy < y1; yy++) {
        const uint16_t *row = coverImg + (uint32_t)yy * w;
        for (int xx = x0; xx < x1; xx++) { sum += coverLuma8(row[xx]); cnt++; }
      }
      src[y * A + x] = cnt ? (uint8_t)(sum / cnt) : 0;
    }
  }
  // 3×3 box blur（边界钳制）——抹平马赛克/色带源
  for (int y = 0; y < A; y++) {
    for (int x = 0; x < A; x++) {
      uint32_t sum = 0;
      for (int dy = -1; dy <= 1; dy++) {
        int yy = y + dy; if (yy < 0) yy = 0; else if (yy >= A) yy = A - 1;
        for (int dx = -1; dx <= 1; dx++) {
          int xx = x + dx; if (xx < 0) xx = 0; else if (xx >= A) xx = A - 1;
          sum += src[yy * A + xx];
        }
      }
      blur[y * A + x] = (uint8_t)(sum / 9);
    }
  }
  // 16 阶量化 + Bayer 4×4：低端圆屏上比连续灰度更少 banding
  static const uint8_t BAYER4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
  };
  for (int y = 0; y < A; y++) {
    for (int x = 0; x < A; x++) {
      int Y = blur[y * A + x];
      int thr = (int)BAYER4[y & 3][x & 3];           // 0..15
      int q = (Y * 16 + thr * 16) / 256;              // 0..16
      if (q > 15) q = 15;
      coverImg[y * A + x] = coverGrayFrom8((uint8_t)(q * 17));  // 0,17,...,255
    }
  }
  coverW = A;
  coverH = A;
}
void parseCover(const uint8_t *p, uint32_t len) {
  coverValid = false;
  musicDirty = true;                                    // 无论成败都让音乐页刷新一次（占位主题可立刻重画）
  if (len < 6 || p[0] != 'C' || p[1] != 'V' || p[2] != 'R' || p[3] != 1) return;
  int w = p[4], h = p[5];
  if (w == 0 || h == 0 || w > COVER_PX || h > COVER_PX) return;
  size_t need = (size_t)w * h * 2;
  // 真 Deflate 可远小于 raw RGB565；只要求 zlib 头 + 少量载荷，由 tinfl 校验解压结果。
  if (len < 6 + 8) return;
  if (!coverImg) coverImg = (uint16_t *)ps_malloc(COVER_PX * COVER_PX * 2);
  if (!coverInflater) coverInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!coverImg || !coverInflater) return;
  tinfl_init(coverInflater);
  size_t inBytes = len - 6, outBytes = need;
  tinfl_status st = tinfl_decompress(coverInflater, p + 6, &inBytes,
      (uint8_t *)coverImg, (uint8_t *)coverImg, &outBytes,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (st != TINFL_STATUS_DONE || inBytes != len - 6 || outBytes != need) return;
  size_t n = (size_t)w * (size_t)h;
  for (size_t i = 0; i < n; i++) coverImg[i] = coverToGray565(coverImg[i]);
  coverPrepareAtmosphere(w, h);                         // 一次预处理；绘制不再全屏采样原图
  coverValid = true;
}
