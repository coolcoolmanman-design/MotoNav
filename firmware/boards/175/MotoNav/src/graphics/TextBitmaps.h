#pragma once
// 手机文字位图绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 文字图片 =================

// 把 4 位灰度图画到 (中心 x, 顶部 y)，tint = 最亮处的灰度 0~255
// v1.7.1：如果图片超出半径 TEXT_R 的圆，就整体等比缩小（按面积取平均，缩小后依然平滑）
static inline uint8_t imgNib(const TextImg &im, int bpr, int x, int y) {
  uint8_t b = im.show[y * bpr + (x >> 1)];
  return (x & 1) ? (b & 0x0F) : (b >> 4);
}

static inline uint16_t greyColor(uint8_t a, uint8_t tint) {      // a = 0~15
  uint8_t g = (uint16_t)(a * 17) * tint / 255;
  return ((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3);
}

// zelda40.2：按亮度映射到指定 RGB565 颜色（白字位图改金色用），col=0 退化为灰阶
static inline uint16_t mapColor(uint8_t a, uint8_t tint, uint16_t col) {
  if (!col) return greyColor(a, tint);
  uint8_t g = (uint16_t)(a * 17) * tint / 255;
  uint8_t r = (uint16_t)g * (col >> 11) / 31;
  uint8_t gr = (uint16_t)g * ((col >> 5) & 0x3F) / 63;
  uint8_t b = (uint16_t)g * (col & 0x1F) / 31;
  return ((r >> 3) << 11) | ((gr >> 2) << 5) | (b >> 3);
}

static void textImgBlit(int slot, int y0, uint8_t tint, uint16_t col, int ox, int oy) {
  TextImg &im = imgs[slot];
  if (!im.show || im.showW == 0 || im.showH == 0) return;
  int bpr = (im.showW + 1) / 2;

  // 算缩放比例：图片四个角都要落在半径 TEXT_R 的圆里
  float s = 1.0f;
  for (int it = 0; it < 4; it++) {
    float h = im.showH * s;
    float dy = fmaxf(fabsf(y0 - CX), fabsf(y0 + h - CX));
    float half = dy < TEXT_R ? sqrtf((float)TEXT_R * TEXT_R - dy * dy) : 0;
    if (im.showW * s / 2 <= half) break;
    s = 2 * half / im.showW;
  }
  if (s < 0.5f) s = 0.5f;

  if (s >= 0.999f) {                       // 不用缩小：原样画
    int x0 = CX - im.showW / 2 + ox;
    for (int y = 0; y < im.showH; y++)
      for (int x = 0; x < im.showW; x++) {
        uint8_t a = imgNib(im, bpr, x, y);
        if (a) gfx->drawPixel(x0 + x, y0 + oy + y, mapColor(a, tint, col));
      }
    return;
  }

  int dw = (int)(im.showW * s), dh = (int)(im.showH * s);
  if (dw < 1 || dh < 1) return;
  int x0 = CX - dw / 2 + ox;
  float inv = 1.0f / s;
  for (int dy = 0; dy < dh; dy++) {
    int sy0 = (int)(dy * inv), sy1 = (int)((dy + 1) * inv);
    if (sy1 <= sy0) sy1 = sy0 + 1;
    if (sy1 > im.showH) sy1 = im.showH;
    for (int dx = 0; dx < dw; dx++) {
      int sx0 = (int)(dx * inv), sx1 = (int)((dx + 1) * inv);
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sx1 > im.showW) sx1 = im.showW;
      int sum = 0, cnt = 0;
      for (int y = sy0; y < sy1; y++)
        for (int x = sx0; x < sx1; x++) { sum += imgNib(im, bpr, x, y); cnt++; }
      uint8_t a = cnt ? (sum + cnt / 2) / cnt : 0;
      if (a) gfx->drawPixel(x0 + dx, y0 + oy + dy, mapColor(a, tint, col));
    }
  }
}

void drawTextImgC(int slot, int y0, uint8_t tint, uint16_t col) { textImgBlit(slot, y0, tint, col, 0, 0); }

// musicA.147c：中文路名描边——先四周偏移画深色，再压白字（参考图路名在亮色路网上仍清晰）
void drawTextImgHaloC(int slot, int y0, uint8_t tint, uint16_t col, uint16_t halo, int r) {
  if (r < 1) r = 1;
  textImgBlit(slot, y0, 0xD0, halo, -r, 0);
  textImgBlit(slot, y0, 0xD0, halo,  r, 0);
  textImgBlit(slot, y0, 0xD0, halo, 0, -r);
  textImgBlit(slot, y0, 0xD0, halo, 0,  r);
  textImgBlit(slot, y0, tint, col, 0, 0);
}

void drawTextImg(int slot, int y0, uint8_t tint) { drawTextImgC(slot, y0, tint, 0); }

// musicA.150：样式16 路名——「白色粗体 + 右下软投影」，且文字必须与背景做真 alpha 混合。
// musicA.149 的 drawTextImgHaloC() 是四向偏移描边 + 直写「亮度×颜色」（不混背景），
// 在青蓝地面上字心发灰、边缘是一圈硬黑边，和参考图的白色粗体 + 柔影不一致。
// y0 语义与 textImgBlit 相同（顶部 y）；需要缩小以落在 TEXT_R 圆内时按同样规则等比缩。
void drawTextImgGlass(int slot, int y0, uint16_t col, int sdx, int sdy, int dark, int tcx = CX) {
  TextImg &im = imgs[slot];
  if (!im.show || im.showW == 0 || im.showH == 0) return;
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int bpr = (im.showW + 1) / 2;
  float s = 1.0f;
  for (int it = 0; it < 4; it++) {
    float h = im.showH * s;
    float dy = fmaxf(fabsf(y0 - CX), fabsf(y0 + h - CX));
    float half = dy < TEXT_R ? sqrtf((float)TEXT_R * TEXT_R - dy * dy) : 0;
    if (im.showW * s / 2 <= half) break;
    s = 2 * half / im.showW;
  }
  if (s < 0.5f) s = 0.5f;
  int dw = (int)(im.showW * s), dh = (int)(im.showH * s);
  if (dw < 1 || dh < 1 || dw > SM_W || dh > SM_H) return;
  int x0 = tcx - dw / 2;
  const float inv = 1.0f / s;
  const int r = 3;
  static uint32_t tiGlassHash = 0;
  // ---- 掩膜：文字 alpha → 掩膜（四周留模糊余量），按内容哈希缓存 ----
  uint32_t hsh = 2166136261u ^ (uint32_t)im.showW * 131u ^ (uint32_t)im.showH * 31u ^ (uint32_t)y0 * 977u ^ (uint32_t)(s * 1000);
  const int nb = min((int)(((size_t)bpr * im.showH + 3) / 4), 2048);
  const uint8_t *bytes = im.show;
  for (int i = 0; i < nb; i++) { hsh ^= bytes[i]; hsh *= 16777619u; }
  if (hsh != tiGlassHash || !smBuf(SM_SLOT_TEXTIMG)) {
    tiGlassHash = hsh;
    int w2 = dw + 2 * r * 2, h2 = dh + 2 * r * 2;
    if (w2 > SM_W || h2 > SM_H) return;
    smBegin(SM_SLOT_TEXTIMG, w2, h2, x0 - 2 * r, y0 - 2 * r);
    uint8_t *m = smBuf(SM_SLOT_TEXTIMG);
    for (int dy = 0; dy < dh; dy++) {
      int sy0 = (int)(dy * inv), sy1 = (int)((dy + 1) * inv);
      if (sy1 <= sy0) sy1 = sy0 + 1;
      if (sy1 > im.showH) sy1 = im.showH;
      uint8_t *out = m + (size_t)(dy + 2 * r) * SM_W + 2 * r;
      for (int dx = 0; dx < dw; dx++) {
        int sx0 = (int)(dx * inv), sx1 = (int)((dx + 1) * inv);
        if (sx1 <= sx0) sx1 = sx0 + 1;
        if (sx1 > im.showW) sx1 = im.showW;
        if (s >= 0.999f) { out[dx] = imgNib(im, bpr, dx, dy) * 17; continue; }
        int sum = 0, cnt = 0;
        for (int y = sy0; y < sy1; y++)
          for (int x = sx0; x < sx1; x++) { sum += imgNib(im, bpr, x, y); cnt++; }
        out[dx] = cnt ? (uint8_t)(((sum + cnt / 2) / cnt) * 17) : 0;
      }
    }
    smBlur(SM_SLOT_TEXTIMG, r);
    smBlur(SM_SLOT_TEXTIMG, r);
  }
  smShadow(SM_SLOT_TEXTIMG, sdx, sdy, dark);
  // ---- 本体：按 alpha 与背景混合 ----
  for (int dy = 0; dy < dh; dy++) {
    const int py = y0 + dy;
    if (py < 0 || py >= LCD_HEIGHT) continue;
    uint16_t *row = fb + (size_t)py * LCD_WIDTH;
    for (int dx = 0; dx < dw; dx++) {
      const int px = x0 + dx;
      if (px < 0 || px >= LCD_WIDTH) continue;
      int a;
      if (s >= 0.999f) a = imgNib(im, bpr, dx, dy) * 17;
      else {
        int sx0 = (int)(dx * inv), sx1 = (int)((dx + 1) * inv), sy0 = (int)(dy * inv), sy1 = (int)((dy + 1) * inv);
        if (sx1 <= sx0) sx1 = sx0 + 1;
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sx1 > im.showW) sx1 = im.showW;
        if (sy1 > im.showH) sy1 = im.showH;
        int sum = 0, cnt = 0;
        for (int y = sy0; y < sy1; y++)
          for (int x = sx0; x < sx1; x++) { sum += imgNib(im, bpr, x, y); cnt++; }
        a = cnt ? ((sum + cnt / 2) / cnt) * 17 : 0;
      }
      if (!a) continue;
      row[px] = a >= 255 ? col : smMix(row[px], col, a);
    }
  }
}

// 左对齐画文字位图（歌词主导等布局：小封面旁的歌名/歌手）。
void drawTextImgAt(int slot, int x0, int y0, uint8_t tint, uint16_t col) {
  TextImg &im = imgs[slot];
  if (!im.show || im.showW == 0 || im.showH == 0) return;
  int bpr = (im.showW + 1) / 2;
  for (int y = 0; y < im.showH; y++)
    for (int x = 0; x < im.showW; x++) {
      uint8_t a = imgNib(im, bpr, x, y);
      if (a) gfx->drawPixel(x0 + x, y0 + y, mapColor(a, tint, col));
    }
}
