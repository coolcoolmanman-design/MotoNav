#pragma once
// ================= 字形图集拼字（musicA.147）=================
// 固件内置的 5x7 点阵字放大后是方块，做不出设计稿的观感。
// 这里改用 TTF 烘焙的 4bit 灰度字形（见 GlyphAtlas147.h），逐像素按 alpha 混色，
// 字形与设计稿一致，可任意拼接动态数值（距离/车速/时间）。
//
// 用法：
//   drawGlyphText(GA_X, "221", 210, 71, C_WHITE, GA_LEFT);
//   int w = gaWidth(GA_A, "58");            // 量宽度，用于并排排版
// 字符集见 gen_glyph_atlas.py；中点写作 '*'。

#include "GlyphAtlas147.h"

#define GA_LEFT    0
#define GA_CENTER  1
#define GA_RIGHT   2

// 4bit alpha(0~15) 与背景真混合：边缘过渡到背景色，而不是压暗成灰黑。
// 这是“抗锯齿观感”的关键——直接覆盖会把半透明边画成暗边。
static inline uint16_t gaBlend(uint8_t a, uint16_t col, uint16_t bg) {
  const uint16_t w  = (uint16_t)a * 17;
  const uint16_t iw = 255 - w;
  const uint16_t r = (((col >> 11) & 0x1F) * w + ((bg >> 11) & 0x1F) * iw) / 255;
  const uint16_t g = (((col >> 5) & 0x3F) * w + ((bg >> 5) & 0x3F) * iw) / 255;
  const uint16_t b = ((col & 0x1F) * w + (bg & 0x1F) * iw) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static inline const GlyphRec* gaFind(const GlyphTier &t, char c) {
  for (uint8_t i = 0; i < t.n; i++)
    if ((char)t.rec[i].ch == c) return &t.rec[i];
  return nullptr;
}

// 一行字的前进总宽（不含最后字符的右侧留白，排版够用）
int gaWidth(const GlyphTier &t, const String &s) {
  int w = 0;
  for (unsigned i = 0; i < s.length(); i++) {
    const GlyphRec* r = gaFind(t, s[i]);
    w += r ? r->adv : t.spaceAdv;
  }
  return w;
}

// 画一行字。yTop = 该档“数字墨迹顶部”的屏幕 y（与 printCentered 的 y 同为顶部锚点）。
// align：x 是左边界 / 中心 / 右边界。返回渲染宽度。
int drawGlyphText(const GlyphTier &t, const String &s, int x, int yTop,
                  uint16_t col, int align) {
  uint16_t* fb = gfx->getFramebuffer();
  if (!fb) return 0;
  int w = gaWidth(t, s);
  int pen = (align == GA_CENTER) ? x - w / 2 : (align == GA_RIGHT ? x - w : x);

  for (unsigned i = 0; i < s.length(); i++) {
    const GlyphRec* r = gaFind(t, s[i]);
    if (!r) { pen += t.spaceAdv; continue; }

    const int bpr = (r->w + 1) >> 1;
    const uint8_t* base = t.data + r->off;
    const int gx0 = pen + r->bx, gy0 = yTop + r->dy;

    for (uint8_t yy = 0; yy < r->h; yy++) {
      const int py = gy0 + yy;
      if (py < 0 || py >= LCD_HEIGHT) continue;
      const uint8_t* row = base + yy * bpr;
      const int line = py * LCD_WIDTH;
      for (uint8_t xx = 0; xx < r->w; xx++) {
        const uint8_t b = row[xx >> 1];
        const uint8_t a = (xx & 1) ? (b & 0x0F) : (b >> 4);
        if (!a) continue;                    // 全透明不画，保留地图
        const int px = gx0 + xx;
        if (px < 0 || px >= LCD_WIDTH) continue;
        uint16_t &p = fb[line + px];
        p = gaBlend(a, col, p);
      }
    }
    pen += r->adv;
  }
  return w;
}

// 便捷：把 UTF-8 中点替换成查表用的 '*'（圆点），避免 UTF-8 解码
static inline String gaAscii(const String &s) {
  String o = s;
  o.replace("\xC2\xB7", "*");
  return o;
}

// ================= musicA.150：字形软投影（整串一次成影，真羽化）================
// musicA.149 的实现有两个毛病：① 8bit 掩膜被钳到 15 再交给只吃 4bit alpha 的混合 →
// 只要掩膜 >10% 的像素全变成 100% 纯黑（实机上数字/箭头带一圈硬黑边）；
// ② 每个字形单独成影，相邻字形的影互相叠加越描越黑。
// 现在整串字铺进一块掩膜（VectorDrawing.h 的软掩膜），两遍盒式模糊后一次压暗背景；
// 掩膜按「内容 + 位置」缓存，数字不变就只做最后的叠画。
#define GA_SM_NUM  0     // 大距离
#define GA_SM_TIME 1     // 时间 / 其它小字
#define GA_SM_UNIT 6     // musicA.157：大距离的单位（m / km）专用槽；
                         // 原来和 GA_SM_TIME 共槽，每帧互相冲掉缓存 → 两次掩膜重建 + 四次模糊
#define GA_SM_BAT  7     // musicA.164：底部电量数字专用槽（同样不能和时间/单位共槽）
int drawGlyphShadowS(int slot, const GlyphTier &t, const String &s, int x, int yTop, int align,
                      int sdx, int sdy, int r, uint8_t dark) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || s.length() == 0 || !smBuf(slot)) return 0;
  if (r < 1) r = 1;
  if (r > 4) r = 4;
  int w = gaWidth(t, s);
  int pen = (align == GA_CENTER) ? x - w / 2 : (align == GA_RIGHT) ? x - w : x;
  char key[96];
  snprintf(key, sizeof(key), "%x|%d|%d|%d|%d|%d|%s", (unsigned int)(uintptr_t)t.data, x, yTop, align, r, (int)s.length(), s.c_str());
  if (String(key) != sm[slot].key) {
    int x0 = 100000, y0 = 100000, x1 = -100000, y1 = -100000, p = pen;
    for (unsigned i = 0; i < s.length(); i++) {
      const GlyphRec *g = gaFind(t, s[i]);
      if (!g) { p += t.spaceAdv; continue; }
      x0 = min(x0, p + g->bx - 2 * r); y0 = min(y0, yTop + g->dy - 2 * r);
      x1 = max(x1, p + g->bx + g->w + 2 * r); y1 = max(y1, yTop + g->dy + g->h + 2 * r);
      p += g->adv;
    }
    sm[slot].key = String(key);                            // 先记下，免得超尺寸时每帧重试
    if (x1 <= x0 || y1 <= y0 || x1 - x0 > SM_W || y1 - y0 > SM_H) return w;
    if (!smBegin(slot, x1 - x0, y1 - y0, x0, y0)) return w;
    uint8_t *m = smBuf(slot);
    p = pen;
    for (unsigned i = 0; i < s.length(); i++) {
      const GlyphRec *g = gaFind(t, s[i]);
      if (!g) { p += t.spaceAdv; continue; }
      const int bpr = (g->w + 1) >> 1;
      const uint8_t *base = t.data + g->off;
      const int gx0 = p + g->bx - x0, gy0 = yTop + g->dy - y0;
      for (uint8_t yy = 0; yy < g->h; yy++) {
        const uint8_t *row = base + yy * bpr;
        uint8_t *out = m + (size_t)(gy0 + yy) * SM_W + gx0;
        for (uint8_t xx = 0; xx < g->w; xx++) {
          const uint8_t b = row[xx >> 1];
          const uint8_t a = ((xx & 1) ? (b & 0x0F) : (b >> 4)) * 17;
          if (a > out[xx]) out[xx] = a;
        }
      }
      p += g->adv;
    }
    smBlur(slot, r);
    smBlur(slot, r);
  }
  smShadow(slot, sdx, sdy, dark);
  return w;
}

// 兼容旧签名：竖直偏移 dy、羽化 spread(1~3)、影心不透明度 dark；水平偏移取 dy 的一半（右下方向）
int drawGlyphShadow(const GlyphTier &t, const String &s, int x, int yTop, int align,
                    int dy, int spread, uint8_t dark) {
  return drawGlyphShadowS(GA_SM_TIME, t, s, x, yTop, align, (dy + 1) / 2, dy, spread, dark);
}
