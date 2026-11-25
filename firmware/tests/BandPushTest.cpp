// musicA.143 主机端单测：脏行检测 + 圆形分带（CI 会跑 firmware/tests/*Test.cpp）
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include "../boards/175/MotoNav/src/display/BandPush.h"
#define W 466
#define H 466
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)
int main() {
  std::vector<uint16_t> a(W * H, 0x1234), b(W * H, 0x1234);
  int y0, h;
  bandDetect(b.data(), a.data(), W, H, y0, h); CHECK(h == 0, "identical → h==0");
  b[50 * W + 7] = 1; bandDetect(b.data(), a.data(), W, H, y0, h); CHECK(y0 == 50 && h == 1, "single row 50");
  b[300 * W + W - 1] = 2; bandDetect(b.data(), a.data(), W, H, y0, h); CHECK(y0 == 50 && h == 251, "rows 50..300");
  b[0 * W] = 3; b[H * W - 1] = 4; bandDetect(b.data(), a.data(), W, H, y0, h); CHECK(y0 == 0 && h == H, "first and last row");
  // 随机：把任意一组行改掉，检测出的范围必须覆盖全部被改的行，且恰好从第一行到最后一行
  srand(1);
  for (int t = 0; t < 500; t++) {
    a.assign(W * H, 0x0F0F); b = a; int n = rand() % 6; int lo = H, hi = -1;
    for (int i = 0; i < n; i++) { int r = rand() % H, c = rand() % W; b[r * W + c] ^= 0x1; if (r < lo) lo = r; if (r > hi) hi = r; }
    bandDetect(b.data(), a.data(), W, H, y0, h);
    if (n == 0) CHECK(h == 0, "random: none changed");
    else CHECK(y0 == lo && h == hi - lo + 1, "random: exact range");
  }
  // 圆形分带：用固件同款 rotSpan（initRotSpan 公式），检查每一行圆内每个像素都被所在带的矩形覆盖
  int16_t half[H]; const int c = W / 2;
  for (int y = 0; y < H; y++) { int dy = y - c; int r2 = c * c - dy * dy; half[y] = r2 > 0 ? (int16_t)sqrtf((float)r2) : 0; }
  const int BAND = 24; long pushed = 0; int bands = 0; bool covered = true; bool aligned = true;
  for (int ya = 0; ya < H; ya += BAND) {
    int yb = ya + BAND < H ? ya + BAND : H; int xa, w; circleBandX(half, W, ya, yb, xa, w);
    pushed += (long)w * (yb - ya); bands++;
    if ((xa & 1) || (w & 1) || xa < 0 || xa + w > W) aligned = false;
    for (int y = ya; y < yb; y++) {  // 真实圆（以像素中心算）里的像素范围
      double dy = y + 0.5 - 233.0, h2 = 233.0 * 233.0 - dy * dy; if (h2 <= 0) continue; double hw = std::sqrt(h2);
      int xl = (int)std::ceil(233.0 - hw), xr = (int)std::floor(233.0 + hw - 1);
      if (xl < xa || xr > xa + w - 1) covered = false;
    }
  }
  CHECK(covered, "每个圆内像素都在带矩形内"); CHECK(aligned, "起点偶、宽偶、不越界");
  printf("分带: %d 条, 推送像素 %.1f%% of full\n", bands, 100.0 * pushed / (W * H));
  // 带范围 [y0,y0+h) 的子集：任意起止行也满足覆盖
  bool ok2 = true;
  for (int s = 0; s < H; s += 37) for (int e = s + 1; e <= H; e += 53) {
    for (int ya = s; ya < e; ya += BAND) {
      int yb = ya + BAND < e ? ya + BAND : e; int xa, w; circleBandX(half, W, ya, yb, xa, w);
      for (int y = ya; y < yb; y++) { double dy = y + 0.5 - 233.0, h2 = 233.0 * 233.0 - dy * dy; if (h2 <= 0) continue; double hw = std::sqrt(h2); int xl = (int)std::ceil(233.0 - hw), xr = (int)std::floor(233.0 + hw - 1); if (xl < xa || xr > xa + w - 1) ok2 = false; }
    }
  }
  CHECK(ok2, "任意行范围的分带都覆盖圆");
  printf(fails ? "有失败\n" : "全部通过\n"); return fails;
}
