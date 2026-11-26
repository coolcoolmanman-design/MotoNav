#pragma once
// 地图帧组合及帧率统计。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v5.8 分步计时（微秒累计，每 2 秒发一次）
#define PROF_N 11
uint32_t profAcc[PROF_N];
uint32_t profT = 0;
static inline void profMark(int i) { uint32_t n = micros(); profAcc[i] += n - profT; profT = n; crumb((uint32_t)i); }   // musicA.143：同时记重启面包屑

void drawMapFrame() {
  crumb(200); crumbFrames++; crumbHeap();                // musicA.143：重启面包屑
  unsigned long t = millis();
  lastMapDraw = t;
  frameIsMap = true;
  // musicA.28 B 方案：有路口图时不再整页接管，地图照常画，最后把浮窗叠上去（见函数末尾）
  // 位置：按车速和转弯速度往前推算（v6.7 沿弧线，再加一点提前量），让画面连续移动、少“慢半拍”
  float cx, cy, hPred;
  predictPose(t, leadMs / 1000.0f, cx, cy, hPred);
  viewTargetHdg = hPred;
  if (corrAt) {                                         // v2.6 平滑修正
    float f = fmaxf(0, 1 - (t - corrAt) / 400.0f);
    cx += corrX * f; cy += corrY * f;
  }
  // 朝向平滑转动，缩放跟车速（越快看得越远）
  if (!viewInit) { viewHdg = hPred; viewInit = true; }
  float dh = hPred - viewHdg;
  while (dh > 180) dh -= 360;
  while (dh < -180) dh += 360;
  // v3.7 按时间平滑转动（和帧率无关）：时间常数 0.35 秒，最快每秒转 70 度。
  //      每一帧之间画面变化更小、更均匀，转弯时的撕裂感会轻很多（这块板子没接屏幕的刷新同步信号，
  //      没法从硬件上完全消除撕裂，只能让相邻两帧尽量接近）
  static unsigned long lastViewAt = 0;
  float vdt = lastViewAt ? (t - lastViewAt) / 1000.0f : 0.05f;
  lastViewAt = t;
  if (vdt > 0.25f) vdt = 0.25f;
  float hstep = dh * (1.0f - expf(-vdt / 0.22f));       // v6.7 0.35 → 0.22 秒：追得更紧
  float hmax = fminf(70.0f * vdt, 3.5f);                // v6.0 每帧最多转 3.5°（防撕裂）
  if (hstep > hmax) hstep = hmax;
  if (hstep < -hmax) hstep = -hmax;
  viewHdg += hstep;
  const bool dragon = styleIdx() == ST_DRAGON;
  // musicA.10 sticky + musicA.11 视口覆盖门闸：帧对齐优先；错帧仅当瓦片仍盖住视口。
  float scGuess = viewScale > 0.05f ? viewScale : 0.55f;
  bool vecOK = !dragon && mapValid && (mapFrame == posFrame);
  bool rasFrameOK = !dragon && rasterValid && rasterFrame == posFrame;
  bool rasStickyOK = !dragon && rasterValid && !rasFrameOK && rasterCoversView(cx, cy, scGuess);
  bool rasOK = rasFrameOK || rasStickyOK;
  bool useRaster = rasOK && (!vecOK || rasterAt >= vectorAt);   // v2.5 两种都有时用最新收到的
  {                                                     // musicA.134 3D 鸟瞰：带网格/特效的风格仍是平面，不开透视
    const int st3 = styleIdx();
    // style 15 自动开；也接受 "#3D,1"（其它可用透视的风格）
    // musicA.147c：样式16「昼夜通透」必须是透视视角（参考图/视频都是斜俯视，楼块有厚度），不能再走 90° 正俯视
    const bool want3d = t3dOn || themeIdx == ST_VECTOR_3D_BIRDSEYE || st3 == ST_DAYNIGHT_GLASS;
    t3dActive = want3d && st3 != ST_DRAGON && st3 != ST_MINIMAL && st3 != ST_BLUEPRINT &&
                st3 != ST_RADAR && st3 != ST_COMPASS && st3 != ST_PIXEL;
    if (t3dActive) t3dSyncBg();
  }
  // 路线/幽灵仍要求 frame 对齐，避免串进上一次导航的线。
  const bool routeOK = !cruiseOn && routeN > 1 && routeFrame == posFrame;
  // 图片地图每像素 2 米，放太大会显得粗糙，所以最大放大倍数小一些
  float speedS = constrain(0.85f - posKmh * 0.006f, 0.38f, useRaster ? 0.6f : 0.85f);
  // musicA.80 Option A · 微远推：导航/巡航会话 + 开关开 + 低速延时进入；
  // viewScale 在近端(停车跟车) ↔ 微远端之间 3–20s 呼吸；起步立刻交回车速公式；等待态藏网格。
  static bool rlWait = false;
  static unsigned long rlStillSince = 0;
  static unsigned long rlPhaseAt = 0;
  const bool rlSession = rlZoomOn && inNav;          // 巡航也会置 inNav
  const bool rlStill = posKmh < 2.5f;
  const bool rlMoving = posKmh > 4.0f;
  if (!rlSession || rlMoving) {
    rlWait = false;
    rlStillSince = 0;
  } else if (rlStill) {
    if (!rlStillSince) rlStillSince = t;
    if (!rlWait && (t - rlStillSince) >= 3000ul) {
      rlWait = true;
      rlPhaseAt = t;
    }
  } else {
    rlStillSince = 0;                                // 蠕行未达退出阈值：重置进入计时
  }
  float targetS = speedS;
  if (rlWait) {
    const float nearS = constrain(0.85f, 0.38f, useRaster ? 0.6f : 0.85f);
    const float farS = fmaxf(0.38f, nearS - 0.08f);  // 微远推振幅
    const float period = 10.0f;                      // 呼吸周期 10s（落在 3–20s）
    float phase = (t - rlPhaseAt) / 1000.0f;
    float wave = 0.5f + 0.5f * sinf(phase * (2.0f * PI / period));
    targetS = nearS + (farS - nearS) * wave;
  }
  // musicA.139 等红灯平视化：样式15/3D 鸟瞰下，停稳 5 秒把倾斜系数平滑降到 0.02（≈平面），
  // 同时放大 1.25x 看清路口；起步（>4km/h）立即平滑恢复到全 3D。植被/楼块/路线共用 t3dBlend。
  static unsigned long birdStillSince = 0;
  const bool birdFlatAvail = t3dActive && inNav;        // 只在导航中的 3D 鸟瞰才平视化（断连/待机不触发）
  const bool birdStill = birdFlatAvail && posKmh >= 0 && posKmh < 2.0f;
  if (!birdStill) birdStillSince = 0;
  else if (!birdStillSince) birdStillSince = t;
  const bool birdShouldFlat = birdStillSince && (t - birdStillSince) >= 5000ul;
  {
    const float tgt = birdShouldFlat ? T3D_BLEND_MIN : 1.0f;
    const float tau = birdShouldFlat ? 0.9f : 0.55f;    // 切平视稍慢、起步回 3D 稍快，都无跳变
    t3dBlend += (tgt - t3dBlend) * (1.0f - expf(-vdt / tau));
    if (t3dBlend < T3D_BLEND_MIN) t3dBlend = T3D_BLEND_MIN;
    if (t3dBlend > 1.0f) t3dBlend = 1.0f;
  }
  if (t3dActive) targetS *= 1.0f + 0.25f * (1.0f - t3dBlend);   // 平视态放大 1.25x
  {                                                     // musicA.142：高架悬浮高度平滑（上桥略缓、下桥略快）
    const float tgtE = elevTgtPx();
    const float tauE = tgtE > elevCur ? 0.55f : 0.40f;
    elevCur += (tgtE - elevCur) * (1.0f - expf(-vdt / tauE));
    if (elevCur < 0.05f) elevCur = 0.0f;
  }
  // 等待态用稍慢平滑（~2.0s）减轻眩晕；退出交回车速公式仍用 1.2s
  float zoomTau = rlWait ? 2.0f : 1.2f;
  viewScale += (targetS - viewScale) * (1.0f - expf(-vdt / zoomTau));
  float sc = viewScale, hs = viewHdg * PI / 180, cs = cosf(hs), sn = sinf(hs);
  float viewR = (CX + (MAP_PY - CX) + 30) / sc;
  if (t3dActive) viewR *= 1.9f;                         // musicA.134 透视能看到更远，路线/路网剔除半径放大

  float wk = sc / 0.6f;
  profT = micros();
  // 先铺主题陆地，避免瓦片外/未写像素残留纯黑弧（平切上方发黑）。
  // musicA.138：3D 鸟瞰且有位图时，背景由 rasterRows3D 在逐行遍历里一次写完（同为 T3D_FILL），
  // 跳过这次整屏填充，省掉第二遍 466² 写入；没有位图时仍按原路径铺底。
  if (!t3dActive || !useRaster) gfx->fillScreen(t3dActive ? T3D_FILL : C_BLOCK);
  if (useRaster) drawRasterMap(cx, cy, sc, cs, sn);     // v2.5 天地图图片地图
  profMark(0);
  // 地图块里的要素（文件里已按：水面、绿地、细路 → 粗路 排好序）
  float lx = cx - mapOx, ly = cy - mapOy;
  const float roadW[6] = {0, 15, 12, 10, 7, 5};
  for (int i = 0; i < ((dragon || useRaster) ? 0 : mapFeatN); i++) {
    const int16_t *bx = featBox + 4 * i;
    if (bx[2] < lx - viewR || bx[0] > lx + viewR || bx[3] < ly - viewR || bx[1] > ly + viewR) continue;
    int np = featNp[i];
    if (np > POLY_PTS) np = POLY_PTS;
    const uint8_t *pp = mapShow + featOff[i];
    for (int k = 0; k < np; k++) {
      float dx = rd16(pp + 4 * k) - lx, dy = rd16(pp + 4 * k + 2) - ly;
      tmpX[k] = CX + sc * (dx * cs - dy * sn);
      tmpY[k] = MAP_PY - sc * (dx * sn + dy * cs);
    }
    t3dArray(tmpX, tmpY, np);                           // musicA.134 3D 鸟瞰（矢量地图）
    uint8_t cls = featCls[i];
    if (cls & 0x80) {
      mapFillPoly(tmpX, tmpY, np, (cls & 0x7F) == 11 ? C_WATER : C_PARK);
    } else {
      int c = cls & 0x7F;
      if (c < 1 || c > 5) c = 5;
      mapPolyline(tmpX, tmpY, np, fmaxf(2.0f, roadW[c] * wk), C_ROADM, false);
    }
  }
  {                                                     // v6.1 风格特效（画在路线下面）
    int st = styleIdx();
    const bool hideGrid = rlWait;                       // 等待态藏主题网格
    if (st == ST_DRAGON && !hideGrid) {
      DragonRadar::drawGrid(gfx, LCD_WIDTH, LCD_HEIGHT, nightOn);  // v3 方格十字
      drawDragonRadarSweep();                                      // v3 软束扇扫
    } else if (st == ST_BLUEPRINT && !hideGrid) drawBlueprintGrid(cx, cy, sc, cs, sn, viewR);
    else if (st == ST_RADAR) drawRadarSweep();
    else if (st == ST_COMPASS && !hideGrid) drawHexGrid();
  }
  profMark(1);
  // 导航路线（带深色描边）；v3.0 车已经走过的部分变灰或不画
  // v7.6 巡航没有路线：上一次导航留下的路线、原路线、路况、红绿灯、终点都不画，道路保持原色
  if (!dragon && !cruiseOn) drawGhost(cx, cy, sc, cs, sn, viewR, fmaxf(3, 5 * wk));   // v4.4 原路线（导入的分享路线）
  if (routeOK) {
    updateRouteProgress(cx, cy);
    float split = routeProg >= 0 ? routeProg : 0;
    float w0 = fmaxf(6, 13 * wk), w1 = fmaxf(4, 9 * wk);
    if (split > 0 && trailMode == 1)
      drawRouteRange(0, split, cx, cy, sc, cs, sn, viewR, w0 * 0.8f, w1 * 0.75f, TH.done2, TH.done);
    if (styleIdx() == ST_GTA6) {
      // musicA.131：粉虚线块 trail（MapFrame 早于 VectorTilt，就地画）
      float phase = 0;
      for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
        int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
        if (e <= split || s >= (float)(routeN - 1)) continue;
        const int32_t *bb = rbBox + 4 * bi;
        if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
        float a0 = fmaxf(split, (float)s), a1 = fminf((float)(routeN - 1), (float)e);
        int m = 0; float x, y;
        routePt(a0, x, y);
        tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
        for (int k = (int)floorf(a0) + 1; k < a1; k++) {
          float dx = routeX[k] - cx, dy = routeY[k] - cy;
          tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
        }
        routePt(a1, x, y);
        tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
        for (int i = 1; i < m; i++) {
          float x0 = tmpX[i - 1], y0 = tmpY[i - 1], x1 = tmpX[i], y1 = tmpY[i];
          float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
          if (len < 0.5f) continue;
          float ux = dx / len, uy = dy / len, t = 0;
          while (t < len - 0.01f) {
            float cycle = 30.0f, mod = fmodf(phase, cycle); if (mod < 0) mod += cycle;
            bool on = mod < 18.0f;
            float slot = on ? (18.0f - mod) : (cycle - mod);
            if (slot > len - t) slot = len - t;
            if (on && slot > 1.2f) {
              float ax = x0 + ux * t, ay = y0 + uy * t, bx = x0 + ux * (t + slot), by = y0 + uy * (t + slot);
              float px[2] = {ax, bx}, py[2] = {ay, by};
              mapPolyline(px, py, 2, w1, C_ROUTE, true);
            }
            t += slot; phase += slot;
          }
        }
      }
    } else {
      drawRouteRange(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR, w0, w1, C_ROUTE2, C_ROUTE);
    }
    if (!dragon && styleIdx()!=ST_MINIMAL && styleIdx()!=ST_GTA6 && curveOn && routeCurv) drawRouteCurved(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR, w1);
    if (!dragon && styleIdx()!=ST_MINIMAL && styleIdx()!=ST_GTA6) drawTraffic(cx, cy, sc, cs, sn, viewR, w0, w1, split);
  }
  profMark(2);
  obsN = 0;                                             // v3.8 占位框从头登记
  if (dragon) {
    bool hudCam = hasCam; hasCam = false; // This style hides the camera badge.
    addHudObstacles(); hasCam = hudCam;
  } else addHudObstacles();
  if (!dragon && !cleanOn && !cruiseOn) drawMapLights(cx, cy, sc, cs, sn);   // v3.2 路线上的红绿灯（v4.9 清爽模式不画；v7.6 巡航不画）
  if (!dragon) drawMapCams(cx, cy, sc, cs, sn);                      // v3.5 电子眼
  if (!cruiseOn) drawDestBlip(cx, cy, sc, cs, sn);      // v2.9 目的地标记（v7.6 巡航没有目的地）
  if (styleIdx() == ST_DAYNIGHT_GLASS) dnGlassPlayer(CX, MAP_CAR_Y);
  else drawPlayer(CX, MAP_CAR_Y);
  profMark(3);
  {
    int st = styleIdx();                                // v6.1 纸地图、蓝图、掌机像素不压暗边缘
    if (st != 0 && st != ST_MINIMAL && st != ST_DRAGON && st != ST_PAPER && st != ST_BLUEPRINT && st != ST_PIXEL) drawVignette(178, 211, 150);   // v2.9 雷达边缘渐暗
  }
  profMark(4);
  if (styleIdx() == ST_COMPASS) drawCompassRose(viewHdg);   // v6.1 罗盘刻度
  else if (!dragon) drawNorth(viewHdg);                              // v2.9 北向标记
  if (!dragon && !cleanOn) drawCamIcons(cx, cy, sc, cs, sn);       // v3.8 电子眼图标（v4.9 清爽模式不画）
  profMark(5);
  unsigned long tMapDone = millis();                   // v2.6 统计：画地图用时
  mapMsAcc += tMapDone - t;

  // 边框和信息
  // v4.9 清爽模式：测速牌、超速红圈、底部行程信息（到达时间/里程/红绿灯数）都不显示
  bool saveCam = hasCam, saveTrip = tripOn;
  if (cleanOn || dragon) { hasCam = false; tripOn = false; }
  bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  // musicA.147b：样式16「昼夜通透」的 HUD 全部由 drawDayNightGlassHud() 画，
  // 旧的外圈行程环 / 车速 / 车道 / 底部大箭头 / 电量必须整体屏蔽，否则两套叠在一起
  // （这是 .145/.146 实机「元素重叠」的根因）。红线外的告警信息 drawNavHints 仍保留。
  const bool dnGlass = (styleIdx() == ST_DAYNIGHT_GLASS);
  if (!dnGlass) {
    {                                                   // v5.9 外圈一次画完
      bool isNear = navDist <= STAGE_NEAR_M;
      // v7.4 平时外圈是行程环；出现转弯倒计时就换成倒计时环（盖掉行程环）
      float tp = ringActive ? -1 : tripFrac();
      drawRingLayer(navDir != "S" && tp < 0, ringActive ? ringShown : 0, isNear ? 14 : 10, isNear ? C_ORANGE : C_YELLOW, over, tp);
    }
    profMark(6);
    drawMapSpeed(over);
    profMark(7);
    drawMapLanes();                                     // v3.3 车道（车速下面）
    profMark(8);
    drawMapBadge();
    profMark(9);
  }
  hasCam = saveCam; tripOn = saveTrip;
  drawNavHints(300);                                    // v5.0 重新规划中 / 更快路线 / GPS 信号弱
  if (!dnGlass) drawBattery(CX - 16, 18, true);           // v6.8 电量（车速上方）；v7.2 带黑底
  if (dnGlass) dnGlassTopScrim();                         // musicA.147c 顶部渐变遮罩（压在路口浮窗之下）
  if (crossReady()) drawCrossOverlay();                   // musicA.28 路口图浮窗压在最上层
  profMark(10);
  // v2.9 小地图里不画顶部的蓝牙小圆点（会和 N 标记重叠；能收到地图就说明连着）
  needFlush = true;
  mapFrames++;
  mapDrawMsAcc += millis() - t;
  ovlMsAcc += millis() - tMapDone;
}

// v2.2 每 2 秒统计一次帧率，发给手机显示
void updateStats() {
  unsigned long now = millis();
  if (now - statAt < 2000) return;
  float secs = (now - statAt) / 1000.0f;
  if (statAt != 0 && mapFrames > 0) {
    uint32_t fc = flushCnt, fm = flushMsAcc;
    pendingStat = "S," + String(mapFrames / secs, 1) + "," + String(mapDrawMsAcc / mapFrames) + "," +
                  String(fc ? fm / fc : 0) + "," + String(qspiMHz) + "," +
                  String(mapMsAcc / mapFrames) + "," + String(ovlMsAcc / mapFrames) + "," + String(fastDev ? 1 : 0) + "," +
                  String(rasMsAcc / mapFrames) + "," + String(helperTask ? 2 : 1);   // v5.7 底图用时、几个核心在画
    String q = "Q";                                    // v5.8 分步用时（0.1 毫秒/帧）
    for (int i = 0; i < PROF_N; i++) q += "," + String(profAcc[i] / 100 / mapFrames);
    pendingProf = q;
#if FPS_SERIAL_DIAGNOSTICS
    Serial.printf("FPSSTAT %s %s %d\n",pendingStat.c_str(),q.c_str(),screenDeg);
#endif
  }
  for (int i = 0; i < PROF_N; i++) profAcc[i] = 0;
  statAt = now; mapFrames = 0; mapDrawMsAcc = 0; mapMsAcc = 0; ovlMsAcc = 0; flushCnt = 0; flushMsAcc = 0; rasMsAcc = 0;
}
