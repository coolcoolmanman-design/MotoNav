#pragma once
// ================= 字形图集拼字（musicA.147）=================
// 固件内置的 5x7 点阵字放大后是方块，做不出设计稿的观感。
// 这里改用 TTF 烘焙的 4bit 灰度字形（见 GlyphAtlas147.h），逐像素按 alpha 混色，
// 字形与设计稿一致，可任意拼接动态数值（距离/车速/时间）。
//
// 用法：
//   drawGlyphText(GA_L, "1.2", CX + 66, 73, C_WHITE, GA_CENTER);
//   int w = gaWidth(GA_M, "58");            // 量宽度，用于并排排版
// 字符集见 gen_glyph_atlas.py；中点写作 '~'。

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

// ================= musicA.147c：字形软投影 =================
// 参考稿里时间与大数字都不是「偏移叠画 3 次」那种硬边阴影，而是一层向下偏 2px、
// 边缘羽化 2~3px 的暗影（压在地图上看得出「浮起来」，但边缘不脏）。
// 做法：逐个字形把 4bit alpha 铺成 8bit 掩膜 → 横竖各一遍 5 点盒式模糊 → 以纯黑按掩膜强度混合。
// 只用一个掩膜缓冲（<10KB，内部 RAM）+ 一条临时行，避免为整串字开大缓存。
#define GA_SD_W   88
#define GA_SD_H   112
static uint8_t gaSdMask[GA_SD_W * GA_SD_H];
static uint8_t gaSdLine[GA_SD_H];

// dy：向下偏移；spread：羽化半径（1~3，窗口 2*spread+1）；dark：影心不透明度 0..255
int drawGlyphShadow(const GlyphTier &t, const String &s, int x, int yTop, int align,
                    int dy, int spread, uint8_t dark) {
  uint16_t* fb = gfx->getFramebuffer();
  if (!fb) return 0;
  int w = gaWidth(t, s);
  int pen = (align == GA_CENTER) ? x - w / 2 : (align == GA_RIGHT ? x - w : x);
  if (spread < 1) spread = 1;
  if (spread > 3) spread = 3;
  const int win = 2 * spread + 1;

  for (unsigned i = 0; i < s.length(); i++) {
    const GlyphRec* r = gaFind(t, s[i]);
    if (!r) { pen += t.spaceAdv; continue; }
    const int mw = r->w + 2 * spread, mh = r->h + 2 * spread;
    if (mw > GA_SD_W || mh > GA_SD_H) { pen += r->adv; continue; }   // 超宽（不该发生）就跳过阴影
    const int bpr = (r->w + 1) >> 1;
    const uint8_t* base = t.data + r->off;

    // 1) 铺掩膜（四周留 spread 的透明边，模糊时才有淡出空间）
    memset(gaSdMask, 0, (size_t)mw * mh);
    for (uint8_t yy = 0; yy < r->h; yy++) {
      const uint8_t* row = base + yy * bpr;
      uint8_t* out = gaSdMask + (size_t)(yy + spread) * mw + spread;
      for (uint8_t xx = 0; xx < r->w; xx++) {
        const uint8_t b = row[xx >> 1];
        out[xx] = ((xx & 1) ? (b & 0x0F) : (b >> 4)) * 17;
      }
    }
    // 2) 横向盒式（原地，用临时行做源；边缘 clamp 复制，避免暗角）
    for (int y = 0; y < mh; y++) {
      uint8_t* row = gaSdMask + (size_t)y * mw;
      memcpy(gaSdLine, row, mw);
      int sum = 0;
      for (int k = -spread; k <= spread; k++) sum += gaSdLine[constrain(k, 0, mw - 1)];
      for (int px = 0; px < mw; px++) {
        row[px] = (uint8_t)(sum / win);
        sum -= gaSdLine[constrain(px - spread, 0, mw - 1)];
        sum += gaSdLine[constrain(px + spread + 1, 0, mw - 1)];
      }
    }
    // 3) 纵向盒式（列拷贝到临时行）
    for (int px = 0; px < mw; px++) {
      for (int y = 0; y < mh; y++) gaSdLine[y] = gaSdMask[(size_t)y * mw + px];
      int sum = 0;
      for (int k = -spread; k <= spread; k++) sum += gaSdLine[constrain(k, 0, mh - 1)];
      for (int y = 0; y < mh; y++) {
        gaSdMask[(size_t)y * mw + px] = (uint8_t)(sum / win);
        sum -= gaSdLine[constrain(y - spread, 0, mh - 1)];
        sum += gaSdLine[constrain(y + spread + 1, 0, mh - 1)];
      }
    }
    // 4) 按掩膜把背景压向纯黑
    const int mx = pen + r->bx - spread;
    const int my = yTop + r->dy + dy - spread;
    for (int yy = 0; yy < mh; yy++) {
      const int py = my + yy;
      if (py < 0 || py >= LCD_HEIGHT) continue;
      const int line = py * LCD_WIDTH;
      const uint8_t* row = gaSdMask + (size_t)yy * mw;
      for (int xx = 0; xx < mw; xx++) {
        const uint8_t v = row[xx];
        if (v < 6) continue;
        const int px2 = mx + xx;
        if (px2 < 0 || px2 >= LCD_WIDTH) continue;
        uint8_t a = (uint8_t)((uint16_t)v * dark / 255);
        if (a > 15) a = 15;
        if (!a) continue;
        uint16_t &q = fb[line + px2];
        q = gaBlend(a, 0x0000, q);
      }
    }
    pen += r->adv;
  }
  return w;
}
