#pragma once
// musicA.138：局部推屏的「脏行」检测。纯 C++（不依赖 ESP 头文件），电脑上有单元测试。
// 整屏 434KB 的 QSPI 推送只能跑到 ~12fps（见 DisplayPipeline.h 注释），3D 鸟瞰顶部 100 行左右的天空/雾几乎不变，
// 只推变化的那一段能直接省下推送时间；画面完全没变时一帧都不用推。
#include <stdint.h>
#include <string.h>

// 比较两块同尺寸画布，返回需要推送的行范围 [y0, y0+h)。h == 0 表示两帧完全相同。
// 只比较整行（W 像素），从上往下找第一行不同，再从下往上找最后一行不同。
static inline void bandDetect(const uint16_t *cur, const uint16_t *prev, int W, int H, int &y0, int &h) {
  int top = -1;
  for (int y = 0; y < H; y++) {
    if (memcmp(cur + (size_t)y * W, prev + (size_t)y * W, (size_t)W * 2) != 0) { top = y; break; }
  }
  if (top < 0) { y0 = 0; h = 0; return; }
  int bot = top;
  for (int y = H - 1; y > top; y--) {
    if (memcmp(cur + (size_t)y * W, prev + (size_t)y * W, (size_t)W * 2) != 0) { bot = y; break; }
  }
  y0 = top; h = bot - top + 1;
}

// 圆屏只有圆内可见，画布四角（约 21.5%）推不推都看不见。把圆按行切成若干横带，每条带推一个矩形（宽取该带最宽那一行 + 2 像素余量，
// 起点取偶、宽度取偶，满足屏幕控制器对窗口对齐的要求），这样整屏推送的数据量降到 ≈84%。
// half[y] = 第 y 行圆内的半宽（像素）；W = 画布宽。返回该带的 x 起点和宽度。
static inline void circleBandX(const int16_t *half, int W, int ya, int yb, int &xa, int &w) {
  int hw = 0;
  for (int y = ya; y < yb; y++) if (half[y] > hw) hw = half[y];
  if (hw < 16) hw = 16;                       // rotSpan 在最上 / 最下一行取整成 0 或偏小；真实圆在最顶行（像素中心）半宽约 15.3
  const int c = W / 2;
  int x0 = c - hw - 4, x1 = c + hw + 4;
  if (x0 < 0) x0 = 0;
  if (x1 > W - 1) x1 = W - 1;
  x0 &= ~1; x1 |= 1;
  if (x1 > W - 1) x1 = W - 1;
  xa = x0; w = (x1 - x0 + 1) & ~1;
}
