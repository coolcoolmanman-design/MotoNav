#pragma once
// 双缓冲、旋转和 QSPI 推屏。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
#include "BandPush.h"          // musicA.143：脏行检测 + 圆形分带（纯 C++，电脑上有单测）
#ifndef BAND_PUSH
#define BAND_PUSH 1              // 1 = 地图帧只推「圆内、相对上一帧有变化的那一段」；0 = 一律整屏推
#endif
#define BAND_ROWS 48             // musicA.144 实测：每带有固定开销，20 带开销占了推屏一半时间，改 10 带
// ---- v2.2 双缓冲：核心 0 上的刷屏任务 ----
uint16_t *fbBuf[2] = {nullptr, nullptr};
int fbCur = 0;                    // 画布当前用的是哪一块
SemaphoreHandle_t flushReq = nullptr, flushDone = nullptr;
uint16_t *volatile flushSrc = nullptr;
volatile uint32_t flushMsAcc = 0, flushCnt = 0;
volatile int flushBandY = 0, flushBandH = 0;      // musicA.143：>0 = 本帧按圆形分带只推 [Y, Y+H) 这些行；0 = 整屏（旧路径）
static bool bandPrevOk = false;                   // musicA.143：fbBuf[fbCur^1] 的内容 == 屏幕上现在显示的内容（只有连续的地图帧才成立）
static uint8_t bandCnt = 0;

// ---- v2.6 快速刷屏：自己往屏幕数据线上排队发送，两块缓冲轮流用（一块在传、一块在准备）----
#include "../../QuarterTurnBlit.h"
#define FAST_PIX 4096                     // 每段像素数（8KB）
spi_device_handle_t fastDev = nullptr;
uint8_t *fastBuf[2] = {nullptr, nullptr};

int screenRot = 0;                        // v6.2 圆屏方向：0 / 1 / 2 / 3 = 0° / 90° / 180° / 270°
int screenDeg = 0;                        // v6.4 圆屏方向（度，0~359）
bool rotFree = false;                     // v6.4 不是 90° 的整数倍：推屏时逐像素按角度取样
int32_t rotCos = 65536, rotSin = 0;       // v6.4 角度的 cos / sin（×65536）
float rotCosF = 1, rotSinF = 0;
unsigned long rotSaveAt = 0;              // v6.4 角度停下后过一会儿再存进板子
int16_t rotSpan[LCD_HEIGHT];              // v6.4 每一行圆里面的半宽（圆外面的像素直接涂黑）

// v6.4 设定圆屏角度。save = true：稍后存进板子
void setScreenDeg(int d, bool save) {
  d = ((d % 360) + 360) % 360;
  screenDeg = d;
  rotFree = (d % 90) != 0;
  screenRot = rotFree ? 0 : d / 90;
  float a = d * (float)M_PI / 180.0f;
  rotCosF = cosf(a); rotSinF = sinf(a);
  rotCos = (int32_t)lroundf(rotCosF * 65536.0f);
  rotSin = (int32_t)lroundf(rotSinF * 65536.0f);
  if (save) rotSaveAt = millis() + 1500;
}

void initRotSpan() {
  const int c = LCD_WIDTH / 2;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int dy = y - c;
    int r2 = c * c - dy * dy;
    rotSpan[y] = r2 > 0 ? (int16_t)sqrtf((float)r2) : 0;
  }
}

// v6.4 停下 1.5 秒后把角度存起来（loop 里调用）
void saveRotIfDue() {
  if (!rotSaveAt || (long)(millis() - rotSaveAt) < 0) return;
  rotSaveAt = 0;
  Preferences pr;
  pr.begin("motonav", false);
  pr.putInt("rotd", screenDeg);
  pr.putInt("rot", screenRot);
  pr.end();
}

bool fastFlushInit() {
  spi_device_interface_config_t dc;
  memset(&dc, 0, sizeof(dc));
  dc.command_bits = 8;
  dc.address_bits = 24;
  dc.mode = 0;
  dc.clock_source = SPI_CLK_SRC_DEFAULT;
  dc.clock_speed_hz = qspiMHz * 1000000;
  dc.spics_io_num = -1;                   // 片选自己控制（和屏幕库一样）
  dc.flags = SPI_DEVICE_HALFDUPLEX;
  dc.queue_size = 2;
  esp_err_t e = spi_bus_add_device(SPI2_HOST, &dc, &fastDev);
  if (e != ESP_OK) { fastDev = nullptr; Serial.printf("[FAST] spi_bus_add_device fail err=%d (%s)\n", (int)e, esp_err_to_name(e)); return false; }
  for (int i = 0; i < 2; i++) {
    fastBuf[i] = (uint8_t *)heap_caps_aligned_alloc(16, FAST_PIX * 2, MALLOC_CAP_DMA);
    if (!fastBuf[i]) {
      Serial.printf("[FAST] buf%d alloc fail, internal free=%u largest=%u\n", i,
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
      fastDev = nullptr; return false;
    }
  }
  return true;
}

// v7.7 RGB565 拆成 0x0GGRRBB 形式（绿、红、蓝之间留空），一次乘法就能三个颜色一起按权重混合
static inline uint32_t rgbSpread(uint16_t c) { return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }

void __attribute__((optimize("O3"))) fastPush(const uint16_t *fb) {
  panel->startWrite();
  panel->writeAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);   // 设置写入区域（整屏）+ 写显存命令
  panel->endWrite();
  spi_device_acquire_bus(fastDev, portMAX_DELAY);
  digitalWrite(LCD_CS, LOW);
  static spi_transaction_ext_t tr[2];
  const uint32_t total = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
  uint32_t sent = 0;
  int bi = 0, inflight = 0;
  bool first = true;
  while (sent < total) {
    uint32_t l = total - sent;
    const uint32_t block=(!rotFree && (screenRot==1 || screenRot==3))?LCD_WIDTH*8:FAST_PIX;
    if (l > block) l = block;
    if (inflight == 2) {                                  // 两块都在传：等最早那块传完再复用
      spi_transaction_t *r;
      spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
      inflight--;
    }
    // 拷贝并交换高低字节（屏幕要高字节在前）
    if (rotFree) {
      // v6.4 任意角度：屏幕上的点 (px,py) 取画布上转过去的点；圆外面涂黑
      uint16_t *d16 = (uint16_t *)fastBuf[bi];
      const int W = LCD_WIDTH, C = LCD_WIDTH / 2;
      int py = sent / W, px = sent - py * W;
      int32_t fx = 0, fy = 0;
      int xs = 0, xe = -1;
      bool rowInit = false;
      for (uint32_t i = 0; i < l; i++) {
        if (!rowInit) {
          int hw = rotSpan[py];
          xs = C - hw; xe = C + hw;
          // 以像素中心算（屏幕宽 466 是双数，正中心在 232.5）：dx2、dy2 是离中心距离的 2 倍
          int dx2 = 2 * px - (W - 1), dy2 = 2 * py - (W - 1);
          // 源点 = (dx·cos + dy·sin, −dx·sin + dy·cos) + 中心；+0.5 四舍五入
          fx = ((dx2 * rotCos + dy2 * rotSin) >> 1) + ((W - 1) << 15) + 32768;
          fy = ((-dx2 * rotSin + dy2 * rotCos) >> 1) + ((W - 1) << 15) + 32768;
          rowInit = true;
        }
        uint16_t v = 0;
        if (px >= xs && px <= xe) {
          // v7.7 双线性插值：取周围 4 个点按距离混合（以前只取最近的一个点，斜着转时字的笔画粗细不均、发糊）
          int32_t bx = fx - 32768, by = fy - 32768;          // 去掉“四舍五入”的 0.5，换成小数部分当权重
          int sx = bx >> 16, sy = by >> 16;
          if ((unsigned)sx < (unsigned)(W - 1) && (unsigned)sy < (unsigned)(W - 1)) {
            const uint16_t *r0 = fb + sy * W + sx, *r1 = r0 + W;
            uint32_t ax = (bx >> 11) & 31, ay = (by >> 11) & 31;   // 0~31 的权重
            uint32_t a = rgbSpread(r0[0]), b = rgbSpread(r0[1]), c = rgbSpread(r1[0]), d = rgbSpread(r1[1]);
            uint32_t top = ((a * (32 - ax) + b * ax) >> 5) & 0x07E0F81Fu;
            uint32_t bot = ((c * (32 - ax) + d * ax) >> 5) & 0x07E0F81Fu;
            uint32_t m = ((top * (32 - ay) + bot * ay) >> 5) & 0x07E0F81Fu;
            v = (uint16_t)(m | (m >> 16));
          } else {
            int nx = fx >> 16, ny = fy >> 16;                 // 画布最边上一圈：还按最近的点取
            if ((unsigned)nx < (unsigned)W && (unsigned)ny < (unsigned)W) v = fb[ny * W + nx];
          }
        }
        d16[i] = (uint16_t)((v >> 8) | (v << 8));
        fx += rotCos; fy -= rotSin;
        if (++px == W) { px = 0; py++; rowInit = false; }
      }
    } else if (screenRot == 0) {
      const uint32_t *src = (const uint32_t *)(fb + sent);
      uint32_t *dst = (uint32_t *)fastBuf[bi];
      for (uint32_t i = 0; i < l / 2; i++) {
        uint32_t v = src[i];
        dst[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
      }
    } else if((screenRot==1 || screenRot==3) && sent%LCD_WIDTH==0 && l%LCD_WIDTH==0) {
      QuarterTurnBlit::pack((uint16_t*)fastBuf[bi],fb,LCD_WIDTH,sent/LCD_WIDTH,l/LCD_WIDTH,screenRot);
    } else {
      // v6.2 转向：屏幕上第 j 个像素取画布上转过去的那个位置
      uint16_t *d16 = (uint16_t *)fastBuf[bi];
      const uint32_t W = LCD_WIDTH, N = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
      uint32_t py = sent / W, px = sent - py * W;
      for (uint32_t i = 0; i < l; i++) {
        uint32_t idx;
        if (screenRot == 2) idx = N - 1 - (sent + i);
        else if (screenRot == 1) idx = (W - 1 - px) * W + py;
        else idx = px * W + (W - 1 - py);
        uint16_t v = fb[idx];
        d16[i] = (uint16_t)((v >> 8) | (v << 8));
        if (++px == W) { px = 0; py++; }
      }
    }
    spi_transaction_ext_t &t = tr[bi];
    memset(&t, 0, sizeof(t));
    if (first) {
      t.base.flags = SPI_TRANS_MODE_QIO;
      t.base.cmd = 0x32;
      t.base.addr = 0x003C00;
      first = false;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    t.base.tx_buffer = fastBuf[bi];
    t.base.length = l * 16;
    spi_device_queue_trans(fastDev, (spi_transaction_t *)&t, portMAX_DELAY);
    inflight++;
    bi ^= 1;
    sent += l;
  }
  while (inflight) {
    spi_transaction_t *r;
    spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
    inflight--;
  }
  digitalWrite(LCD_CS, HIGH);
  spi_device_release_bus(fastDev);
}

// musicA.23 局部推送：只把画布上的一块矩形推到屏幕。
// 屏保动画整屏是 434KB/帧（≈12fps），只推眼睛那块 100KB 就能到 40fps。
// 旋转状态（非 0° 或是任意角度）下屏幕像素在画布上不连续，取不出矩形，直接返回 false 让调用方退回整屏。
bool fastPushRectSrc(const uint16_t *src, int x, int y, int w, int h) {   // musicA.143：src 由调用方指定（异步推屏时画布已经换到另一块）
  if (rotFree || screenRot != 0) return false;
  if (!src) return false;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > LCD_WIDTH)  w = LCD_WIDTH - x;
  if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
  w &= ~1;                                         // 下面按 32 位成对拷贝，宽度取偶
  if (w <= 0 || h <= 0) return false;

  panel->startWrite();
  panel->writeAddrWindow(x, y, w, h);              // 设写入区域（+ 显存写命令）
  panel->endWrite();

  if (!fastDev) {                                  // 没启用快速刷屏：逐行交给屏幕库
    for (int r = 0; r < h; r++)
      panel->draw16bitRGBBitmap(x, y + r, (uint16_t *)src + (y + r) * LCD_WIDTH + x, w, 1);
    return true;
  }

  spi_device_acquire_bus(fastDev, portMAX_DELAY);
  digitalWrite(LCD_CS, LOW);
  static spi_transaction_ext_t rtr[2];
  int rowsPer = FAST_PIX / w; if (rowsPer < 1) rowsPer = 1;
  int done = 0, bi = 0, inflight = 0;
  bool first = true;
  while (done < h) {
    int rows = h - done; if (rows > rowsPer) rows = rowsPer;
    const int halfW = w >> 1;
    if (inflight == 2) {                           // 两块都在传：等最早那块传完再复用
      spi_transaction_t *r;
      spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
      inflight--;
    }
    uint32_t *dst = (uint32_t *)fastBuf[bi];
    for (int r = 0; r < rows; r++) {               // 逐行拷贝并交换高低字节（屏幕要高字节在前）
      const uint32_t *s = (const uint32_t *)(src + (y + done + r) * LCD_WIDTH + x);
      uint32_t *d = dst + r * halfW;
      for (int i = 0; i < halfW; i++) {
        uint32_t v = s[i];
        d[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
      }
    }
    spi_transaction_ext_t &t = rtr[bi];
    memset(&t, 0, sizeof(t));
    if (first) {                                   // 第一段带写显存命令，后面只发数据
      t.base.flags = SPI_TRANS_MODE_QIO;
      t.base.cmd = 0x32;
      t.base.addr = 0x003C00;
      first = false;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    t.base.tx_buffer = fastBuf[bi];
    t.base.length = (uint32_t)rows * w * 16;       // bit 数
    spi_device_queue_trans(fastDev, (spi_transaction_t *)&t, portMAX_DELAY);
    inflight++; bi ^= 1; done += rows;
  }
  while (inflight) {
    spi_transaction_t *r;
    spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
    inflight--;
  }
  digitalWrite(LCD_CS, HIGH);
  spi_device_release_bus(fastDev);
  return true;
}

bool fastPushRect(int x, int y, int w, int h) { return fastPushRectSrc(gfx->getFramebuffer(), x, y, w, h); }

// musicA.143：把 [y0, y1) 行范围按圆形分带推到屏幕（每带一个矩形，宽度取该带圆内最宽处）。任何一带失败返回 false，调用方退回整屏。
static bool fastPushCircleBands(const uint16_t *src, int y0, int y1) {
  for (int ya = y0; ya < y1; ya += BAND_ROWS) {
    const int yb = (ya + BAND_ROWS < y1) ? ya + BAND_ROWS : y1;
    int xa, w;
    circleBandX(rotSpan, LCD_WIDTH, ya, yb, xa, w);
    if (w <= 0) continue;
    if (!fastPushRectSrc(src, xa, ya, w, yb - ya)) return false;
  }
  return true;
}

// 局部刷屏对外入口：旋转状态下退回整屏（画布内容仍然是对的，只是慢一些）
void flushSync();                                  // 定义在本文件后面
void flushRect(int x, int y, int w, int h) {
  bandPrevOk = false;                              // musicA.143：屏幕内容不再等于上一块缓冲
  if (!fbBuf[1]) { gfx->flush(); return; }
  if (!fastPushRect(x, y, w, h)) flushSync();
}

void flushTask(void *arg) {
  for (;;) {
    xSemaphoreTake(flushReq, portMAX_DELAY);
    // musicA.144：initDoubleBuffer 里总线可能还没起来，导致快刷没装成。首次真正推屏时再试一次，
    // 拿到就用（flush 117ms → 明显下降），仍失败就一直是 nullptr，退回慢路径但只试一次。
    static bool fastRetryDone = false;
    if (!fastDev && fastFlushOn && !fastRetryDone) { fastRetryDone = true; fastFlushInit(); }
    crumb(100);
    uint32_t t0 = millis();
    const int bandH = flushBandH, bandY = flushBandY;
    flushBandH = 0; flushBandY = 0;               // musicA.148：带用完即清，绝不允许粘到下一帧
    if (fastDev && bandH > 0) { if (!fastPushCircleBands(flushSrc, bandY, bandY + bandH)) fastPush(flushSrc); }   // musicA.143：圆内 + 只推变化的行
    else if (fastDev) fastPush(flushSrc);
    else panel->draw16bitRGBBitmap(0, 0, flushSrc, LCD_WIDTH, LCD_HEIGHT);
    flushMsAcc += millis() - t0; flushCnt++;
    crumb(101);
    xSemaphoreGive(flushDone);
    vTaskDelay(1);        // musicA.143：每帧让出 1 个 tick —— 核心 0 上第二个渲染任务（优先级 3）+ 推屏（2）连续满载时，空闲任务饿死会触发看门狗重启
  }
}

// v2.6 等刷屏完成；超过 3 秒还没完成，说明快速刷屏在这块板子上不工作：关掉它并重启
void flushWait() {
  if (xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE) return;
  // musicA.143：只有「一次都没推成功过」才判定快速刷屏在这块板子上不可用并重启关掉它。
  // 运行中偶发卡住 > 3 秒（核心 0 被高优先级任务占满、蓝牙 / WiFi 突发）只记录，继续等 —— 否则会无故重启，
  // 并且把快速刷屏永久关掉（之后每帧都慢一大截）。
  if (fastFlushOn && flushCnt == 0) { crumbWhoSet(CRUMB_WHO_FASTFLUSH); saveAndRestart("fastf", 0, true); }
  Serial.printf("[FLUSH] stall >3s (cnt=%u), still waiting\n", (unsigned)flushCnt);
  xSemaphoreTake(flushDone, portMAX_DELAY);
}

// 异步刷屏：把刚画好的这一块交给核心 0 推到屏幕，画布换到另一块继续画下一帧
void flushAsync() {
  if (!fbBuf[1]) { gfx->flush(); return; }
  flushWait();                                    // 等上一帧推完
  flushSrc = fbBuf[fbCur];
  flushBandY = 0; flushBandH = 0;                 // 默认整屏（旧路径）
  bool doPush = true;
#if BAND_PUSH
  // musicA.143：整屏推送是显示链路的上限（≈12fps）。
  //   ① 圆屏四角（约 16%）看不见，只推圆内的横带；
  //   ② 上一帧也是地图帧（bandPrevOk）时，和它逐行比较，只推变化的那一段（3D 顶部天空 / 雾几乎不变）；两帧完全相同就一帧都不推；
  //   ③ 每 32 帧强制推一次完整的圆，万一有残影也能自愈。
  const bool bandOk = fastDev && screenRot == 0 && !rotFree;
  if (bandOk) {
    int y0 = 0, h = LCD_HEIGHT;
    if (bandPrevOk && ++bandCnt < 32) {
      bandDetect(fbBuf[fbCur], fbBuf[fbCur ^ 1], LCD_WIDTH, LCD_HEIGHT, y0, h);
      if (h == 0) doPush = false;
    } else {
      bandCnt = 0;
    }
    if (doPush) { flushBandY = y0; flushBandH = h; }
  }
  bandPrevOk = bandOk;
#endif
  if (doPush) xSemaphoreGive(flushReq);
  else xSemaphoreGive(flushDone);                 // 没推：把刚拿走的「推完」信号还回去
  fbCur ^= 1;
  gfx->setFramebuffer(fbBuf[fbCur]);
}

// 同步刷屏（普通画面用，等推完再返回）
void flushSync() {
  bandPrevOk = false;                             // musicA.143
  if (!fbBuf[1]) { gfx->flush(); return; }
  flushWait();
  flushSrc = fbBuf[fbCur];
  // musicA.148 修「导航结束后上半部冻屏 + 划页看起来没反应」：
  // flushBandY/H 只有 flushAsync 会写，flushTask 却无条件照着它推。上一帧导航（异步）
  // 留下的局部带如果不在这里清掉，紧接着的整屏重画（drawWaiting / 划页提交）就只推那一段，
  // 圆屏其余部分停在旧画面上，而且这个带会一直粘着，之后每次同步推屏都只推它。
  flushBandY = 0; flushBandH = 0;                 // 本帧整屏
  xSemaphoreGive(flushReq);
  flushWait();                                    // 等这一帧推完
  xSemaphoreGive(flushDone);
}

void initDoubleBuffer() {
  fbBuf[0] = gfx->getFramebuffer();
  fbBuf[1] = (uint16_t *)heap_caps_aligned_alloc(16, LCD_WIDTH * LCD_HEIGHT * 2, MALLOC_CAP_SPIRAM);
  if (!fbBuf[1]) { Serial.println("[FAST] fbBuf[1] alloc fail -> no double buffer"); return; }
  memcpy(fbBuf[1], fbBuf[0], LCD_WIDTH * LCD_HEIGHT * 2);
  if (fastFlushOn) fastFlushInit();                // v2.6 快速刷屏
  Serial.printf("[FAST] on=%d fb1=%p dev=%p int_free=%u largest=%u\n", fastFlushOn ? 1 : 0, fbBuf[1], fastDev,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  flushReq = xSemaphoreCreateBinary();
  flushDone = xSemaphoreCreateBinary();
  xSemaphoreGive(flushDone);
  xTaskCreatePinnedToCore(flushTask, "flush", 4096, nullptr, 2, nullptr, 0);
}
