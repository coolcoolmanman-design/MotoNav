#pragma once
// 导航状态重置、文字命令和总消息入口。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
#include <time.h>
#include <string.h>
void resetNavState() {
  cancelPendingRaster();
  trafficLightFrame.clear();trafficLightLab.clear();
  if (idlePowerOffNavigationActive(true, inNav, cruiseOn, navDir == "A"))
    idlePowerOffReset(&idlePowerOffTimer, millis());
  inNav = false;
  // ui.5：方向 / 距离 / 行程信息一起清。原来只清 inNav，任何一次 navDirty 重绘
  // 都会把上一趟的 "A,0"（剩余 0）重新画回导航页。navDir 清空后新导航首帧
  // 不再触发 progReset()，这里补一次，避免进度条延续上一趟。
  progReset();
  navDir = ""; navDist = 0; navAngle = NAN;
  tripRemainM = 0; tripEta = 0; tripLights = 0;
  navDirty = false;
  tripOn = false; lightDist = -1;                       // v3.1
  mapCamN = 0;                                          // v3.5
  ghostN = 0;                                           // v4.4
  cruiseOn = false;                                     // v4.8
  trafN = 0; rerouteAt = 0; fasterAt = 0; gpsWeak = false;   // v5.0
  speedKmh = -1;
  hasCam = false;
  laneN = 0;
  ringActive = false; ringDir = "";
  for (int i = 0; i < 3; i++) { imgs[i].showW = 0; imgs[i].showH = 0; imgs[i].recvId = -1; }   // v5.3 天气栏 3、4 留着
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0xF;
}

// 处理文字类消息
void handleText(const String &m) {
  if(m.startsWith("#TLA,")) {
    const int previous=trafficLightFrame.key;
    const bool accepted=trafficLightFrame.receive(m.c_str(),millis());
    if(!accepted || !trafficLightFrame.valid(millis()) || previous!=trafficLightFrame.key)trafficLightLab.clear();
    return;
  }
  if(m=="#TLP,1") {
    trafficLightPreviewUntil=millis()+5000;
    Serial.println("TL LAYOUT PREVIEW placeholders-only duration=5000");
    return;
  }
  if (m.startsWith("#TL,") || m.startsWith("#TLS,")) {
    Serial.printf("TL WIRE at=%lu %s\n",millis(),m.c_str());
    bool accepted=trafficLightLab.receive(m.c_str(), millis());
    Serial.printf("TL JUNCTION RX accepted=%d source=%d count=%u left=%d straight=%d right=%d other=%d\n",
      accepted,trafficLightLab.source,trafficLightLab.count(millis()),trafficLightLab.phases[0].valid(millis()),
      trafficLightLab.phases[1].valid(millis()),trafficLightLab.phases[2].valid(millis()),trafficLightLab.phases[3].valid(millis()));
    return;
  }
  if (m.startsWith("#LY,")) {
    unsigned long gen = 0; unsigned title = 0, artist = 0;
    if (sscanf(m.c_str(), "#LY,%lu,%u,%u", &gen, &title, &artist) == 3 && gen == appleMedia.generation &&
        title == appleMedia.values[0].revision && artist == appleMedia.values[1].revision) {
      lyricImageGeneration = gen; lyricImageTitleRevision = title; lyricImageArtistRevision = artist;
    } else { lyricImageGeneration = 0; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; }
    return;
  }
  if (m == "#ML,0" || m == "#ML,1") {
    bool wasEnabled = musicLyrics;
    musicLyrics = m == "#ML,1";
    if (!musicLyrics) {
      lyricImageGeneration = 0; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; imgs[12].recvMask = 0; imgs[12].recvParts = 0;
      // Older companion renderers may have put lyrics in the artist bitmap.
      // Ask for a fresh title/artist once per connection or on an actual OFF transition.
      static uint32_t refreshedEpoch = 0xffffffff;
      if (wasEnabled || refreshedEpoch != mediaConnectionEpoch) {
        refreshedEpoch = mediaConnectionEpoch;
        imgs[6].showW = 0; imgs[6].showH = 0; imgs[6].recvId = -1; pendingEvt = "T,M,Q";
      }
    }
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    Serial.printf("MEDIA LYRICS enabled=%d\n", musicLyrics);
    musicDirty = true; return;
  }
  if (m == "#HM,0" || m == "#HM,1") {
    bool enabled = m == "#HM,1";
    if (enabled != hidMusicMode) {
      hidMusicMode = enabled; hidPulses.cancelQueued();
      Preferences pr; pr.begin("motonav", false); pr.putBool("mediahid", enabled); pr.end();
    }
    if (!hidMusicMode) { musicLyrics = false; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; }
    musicAck = 0; musicAckUntil = 0; musicDirty = true;
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    return;
  }
  // A dedicated command: no navigation state or screen invalidation.
  if (m.startsWith("#BOOT,")) {
    int id = BootAnimation::parse(m.c_str() + 6);
    if (id < 0) return;
    bool saved = true;
    if (id != bootBrand) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putInt("bootbrand", id) == sizeof(int32_t);
        pr.end();
      }
      if (saved) bootBrand = id;
    }
    pendingBootAck = "BOOT," + String(saved ? bootBrand : -1);
    return;
  }
  // musicA.19：表盘选择功能下线——#FACE 仍回 ACK 兼容旧 App，但恒定默认表盘（透明黑背景）。
  if (m.startsWith("#FACE,")) {
    pendingFaceAck = "FACE,0";
    return;
  }
  // 二期C 组件槽配置（musicA.8）：#FC,<时间色>,<电量色>,<连接态色>,<气压色>,<槽掩码>
  // 颜色 RGB565 十进制（0 = 自动默认）；掩码 bit0时间 bit1电量 bit2连接态 bit3气压。存 NVS，回 echo ACK。
  if (m.startsWith("#FC,")) {
    int tc = 0, bc = 0, cc = 0, pc = 0, mk = 0;
    if (sscanf(m.c_str(), "#FC,%d,%d,%d,%d,%d", &tc, &bc, &cc, &pc, &mk) != 5) return;
    tc = constrain(tc, 0, 65535); bc = constrain(bc, 0, 65535);
    cc = constrain(cc, 0, 65535); pc = constrain(pc, 0, 65535);
    mk &= 0x0F;
    bool saved = true;
    if (tc != slotTimeColor || bc != slotBattColor || cc != slotConnColor ||
        pc != slotTpmsColor || mk != slotMask) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putUShort("fc_tc", (uint16_t)tc) == 2
             && pr.putUShort("fc_bc", (uint16_t)bc) == 2
             && pr.putUShort("fc_cc", (uint16_t)cc) == 2
             && pr.putUShort("fc_pc", (uint16_t)pc) == 2
             && pr.putUChar("fc_mk", (uint8_t)mk) == 1;
        pr.end();
      }
      if (saved) {
        slotTimeColor = tc; slotBattColor = bc; slotConnColor = cc; slotTpmsColor = pc;
        slotMask = mk;
        faceDirty = true;       // 秒切：待机页整页重绘，槽颜色/显隐即刻生效
      }
    }
    pendingFaceCfgAck = "FC," + String(saved ? tc : -1) + "," + String(saved ? bc : -1) +
                       "," + String(saved ? cc : -1) + "," + String(saved ? pc : -1) +
                       "," + String(saved ? mk : -1);
    return;
  }
  // musicA.17 六槽表盘配置：#FC2,<时间色>,<电量色>,<连接态色>,<槽掩码>,<左上功能>,<右上功能>,<下方功能>,<左上色>,<右上色>,<下方色>,<载具色>
  // 掩码 bit0时间 bit1电量 bit2连接态 bit3中央；musicA.40 起 bit1 不再强制。功能 0速度 1G值 2胎压 3天气 4隐藏。
  // 全部存 NVS，回 echo ACK（首字段 tc，-1 = 保存失败）。
  if (m.startsWith("#FC2,")) {
    int tc=0,bc=0,cc=0,mk=0,f0=0,f1=0,f2=0,c0=0,c1=0,c2=0,cv=0;
    if (sscanf(m.c_str(), "#FC2,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
               &tc,&bc,&cc,&mk,&f0,&f1,&f2,&c0,&c1,&c2,&cv) != 11) return;
    tc=constrain(tc,0,65535); bc=constrain(bc,0,65535); cc=constrain(cc,0,65535);
    f0=constrain(f0,0,4); f1=constrain(f1,0,4); f2=constrain(f2,0,4);
    c0=constrain(c0,0,65535); c1=constrain(c1,0,65535); c2=constrain(c2,0,65535);
    cv=constrain(cv,0,65535);
    mk = mk & 0x0F;                                          // musicA.40：四位均可由 App 控制
    bool saved = true;
    if (tc != slotTimeColor || bc != slotBattColor || cc != slotConnColor || mk != slotMask ||
        f0 != faceSlotFunc[0] || f1 != faceSlotFunc[1] || f2 != faceSlotFunc[2] ||
        c0 != slotTLColor || c1 != slotTRColor || c2 != slotBTColor || cv != slotVehColor) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putUShort("fc_tc", (uint16_t)tc) == 2
             && pr.putUShort("fc_bc", (uint16_t)bc) == 2
             && pr.putUShort("fc_cc", (uint16_t)cc) == 2
             && pr.putUChar("fc2_mk", (uint8_t)mk) == 1
             && pr.putUChar("fc2_f0", (uint8_t)f0) == 1
             && pr.putUChar("fc2_f1", (uint8_t)f1) == 1
             && pr.putUChar("fc2_f2", (uint8_t)f2) == 1
             && pr.putUShort("fc2_c0", (uint16_t)c0) == 2
             && pr.putUShort("fc2_c1", (uint16_t)c1) == 2
             && pr.putUShort("fc2_c2", (uint16_t)c2) == 2
             && pr.putUShort("fc2_cv", (uint16_t)cv) == 2;
        pr.end();
      }
      if (saved) {
        slotTimeColor = tc; slotBattColor = bc; slotConnColor = cc;
        faceSlotFunc[0] = f0; faceSlotFunc[1] = f1; faceSlotFunc[2] = f2;
        slotTLColor = c0; slotTRColor = c1; slotBTColor = c2; slotVehColor = cv;
        slotMask = mk;
        faceDirty = true;
      }
    }
    pendingFaceCfgAck = "FC2," + String(saved ? tc : -1) + "," + String(saved ? bc : -1) +
                        "," + String(saved ? cc : -1) + "," + String(saved ? mk : -1) +
                        "," + String(saved ? f0 : -1) + "," + String(saved ? f1 : -1) +
                        "," + String(saved ? f2 : -1) + "," + String(saved ? c0 : -1) +
                        "," + String(saved ? c1 : -1) + "," + String(saved ? c2 : -1) +
                        "," + String(saved ? cv : -1);
    return;
  }
  // musicA.17/46/52 中央动画载具：#FV,<0塞尔达 | 1像素摩托 | 2自行车(App 已隐藏) | 3 GTA VI | 4 组合>（存 NVS，回 ACK）
  if (m.startsWith("#FV,")) {
    int id = constrain(m.substring(4).toInt(), 0, 4);
    bool saved = true;
    if (id != vehId) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putUChar("fc2_veh", (uint8_t)id) == 1; pr.end(); }
      if (saved) { vehId = (uint8_t)id; faceDirty = true; }
    }
    pendingVehAck = "FV," + String(saved ? id : -1);
    return;
  }
  // musicA.50 省电模式：#LP,<0|1>；插电自动退由固件 batPoll 执行；中央冻帧+降亮+关 dynBg
  if (m.startsWith("#LP,")) {
    int on = m.substring(4).toInt() != 0 ? 1 : 0;
    bool saved = true;
    if ((bool)on != lpModeOn) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putBool("lp_on", on != 0) == 1; pr.end(); }
      if (saved) {
        if (on) {
          if (lpBrightSaved < 0) lpBrightSaved = lastBrightness;
          int b = constrain(lpBrightSaved > 0 ? min(lpBrightSaved, (int)LP_BRIGHT_CAP) : LP_BRIGHT_CAP, 5, 255);
          noteBrightnessForWake(b);
        } else if (lpBrightSaved >= 0) {
          noteBrightnessForWake(lpBrightSaved);
          lpBrightSaved = -1;
        }
        lpModeOn = on != 0;
        faceDirty = true;
      }
    }
    pendingLpAck = String("LP,") + String(saved ? (lpModeOn ? 1 : 0) : -1);
    return;
  }
  // musicA.50 ≤10% 自动开省电：#LPA,<0|1>（默认开）
  if (m.startsWith("#LPA,")) {
    int on = m.substring(5).toInt() != 0 ? 1 : 0;
    bool saved = true;
    if ((bool)on != lpAutoOn) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putBool("lp_auto", on != 0) == 1; pr.end(); }
      if (saved) lpAutoOn = on != 0;
    }
    pendingLpAck = String("LPA,") + String(saved ? (lpAutoOn ? 1 : 0) : -1);
    return;
  }
  // musicA.50 电池供电短按息屏：#PBO,<0|1>（默认关；USB 短按始终可用）
  if (m.startsWith("#PBO,")) {
    int on = m.substring(5).toInt() != 0 ? 1 : 0;
    bool saved = true;
    if ((bool)on != pwrBatOffAllow) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putBool("pwr_bat_off", on != 0) == 1; pr.end(); }
      if (saved) pwrBatOffAllow = on != 0;
    }
    pendingLpAck = String("PBO,") + String(saved ? (pwrBatOffAllow ? 1 : 0) : -1);
    return;
  }
  // musicA.40 六槽改造：#FC3,<上方 0..5>,<中央 0..3>,<时间样式 0..8>,<指针A/B 0/1>
  // 上方 5=隐藏；中央 3=隐藏。旧 #FC2 一字不改。存 NVS，回 ACK FC3,<...>。
  if (m.startsWith("#FC3,")) {
    int tp=0, ct=0, st=0, ab=0;
    if (sscanf(m.c_str(), "#FC3,%d,%d,%d,%d", &tp,&ct,&st,&ab) != 4) return;
    tp=constrain(tp,0,5); ct=constrain(ct,0,3); st=constrain(st,0,8); ab=constrain(ab,0,1);
    bool saved = true;
    if (tp!=topFunc || ct!=centerFunc || st!=clockStyle || ab!=fullFaceAB) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putUChar("fc3_top",(uint8_t)tp)==1
             && pr.putUChar("fc3_ctr",(uint8_t)ct)==1
             && pr.putUChar("fc3_sty",(uint8_t)st)==1
             && pr.putUChar("fc3_ab", (uint8_t)ab)==1;
        pr.end();
      }
      if (saved) {
        topFunc=(uint8_t)tp; centerFunc=(uint8_t)ct;
        clockStyle=(uint8_t)st; fullFaceAB=(uint8_t)ab;
        // bit0←上方=时间；bit3←中央≠隐藏
        slotMask = (uint8_t)((slotMask & ~0x01) | (topFunc == 0 ? 0x01 : 0));
        slotMask = (uint8_t)((slotMask & ~0x08) | (centerFunc != 3 ? 0x08 : 0));
        faceDirty = true;
        Serial.printf("[FC3] top=%d center=%d style=%d ab=%d\n", tp, ct, st, ab);
      }
    }
    pendingVehAck = "FC3," + String(saved ? topFunc : -1) + "," + String(centerFunc) +
                    "," + String(clockStyle) + "," + String(fullFaceAB);
    return;
  }
  // musicA.37 预设动态背景：#BG,<on>,<style 0..5>,<speed 10..26>,<autoDayNight>
  // style 0 = 现有静态（零开销）；昼夜直接读 nightOn，auto 只影响 App 侧的说明与预览。
  // 上传自定义壁纸时 dynBgActive() 自动为假，不需要 App 额外下发暂停命令。
  if (m.startsWith("#BG,")) {
    int on=0, sty=0, spd=18, aut=1;
    if (sscanf(m.c_str(), "#BG,%d,%d,%d,%d", &on,&sty,&spd,&aut) != 4) return;
    sty=constrain(sty,0,5); spd=constrain(spd,10,26);
    bool en=on!=0, au=aut!=0;
    bool saved = true;
    if (en!=dynBgOn || sty!=dynBgStyle || spd!=dynBgSpeed || au!=dynBgAuto) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putBool("bg_on", en)==1
             && pr.putUChar("bg_sty",(uint8_t)sty)==1
             && pr.putUChar("bg_spd",(uint8_t)spd)==1
             && pr.putBool("bg_auto", au)==1;
        pr.end();
      }
      if (saved) {
        dynBgOn=en; dynBgStyle=(uint8_t)sty; dynBgSpeed=(uint8_t)spd; dynBgAuto=au;
        faceDirty = true;
        Serial.printf("[BG] on=%d style=%d speed=%d auto=%d\n", (int)en, sty, spd, (int)au);
      }
    }
    pendingVehAck = "BG," + String(saved ? (dynBgOn?1:0) : -1) + "," + String(dynBgStyle) +
                    "," + String(dynBgSpeed) + "," + String(dynBgAuto?1:0);
    return;
  }
  // musicA.25 屏保开关：#SV,<0 关 | 1 开>（存 NVS "saver"，回 ACK SV,<on>）
  // 关掉时如果正在显示屏保，立刻退回当前页面（不走渐变，避免停在黑屏半亮的状态）。
  if (m.startsWith("#SV,")) {
    bool on = m.substring(4).toInt() != 0;
    bool saved = true;
    if (on != saverEnabled) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putBool("saver", on) == 1; pr.end(); }
      if (saved) {
        saverEnabled = on;
        if (!on && saverActive) saverExit(false);   // App 关开关时没有触摸，别把下一次点击吃掉
        Serial.printf("[SAVER] enabled=%d\n", (int)on);
      }
    }
    pendingVehAck = "SV," + String(saved ? (on ? 1 : 0) : -1);
    return;
  }
  // musicA.26 路口放大图开关：#CX,<0 关 | 1 开>（存 NVS "cross"，回 ACK CX,<on>）
  if (m.startsWith("#CX,")) {
    bool on = m.substring(4).toInt() != 0;
    bool saved = true;
    if (on != crossEnabled) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putBool("cross", on) == 1; pr.end(); }
      if (saved) {
        crossEnabled = on;
        if (!on) { crossOn = false; crossPrevOn = false; }  // 关掉就立刻退回小地图，不清缓冲（下次还能复用）
        Serial.printf("[CROSS] enabled=%d\n", (int)on);
      }
    }
    pendingVehAck = "CX," + String(saved ? (on ? 1 : 0) : -1);
    return;
  }
  // musicA.26 高德隐藏路口图（通过路口）：只清当前图，不动开关
  if (m == "#CXH") {
    crossHiddenGen = lastCrossGen;                 // musicA.35：只静默「当前显示这一代」的迟到残包
    crossHideAt = millis();                        // 新 gen 到达会提前解禁（见 parseCross）
    if (crossOn || crossPrevOn) {
      crossOn = false;
      crossPrevOn = false;
      navDirty = true;
      Serial.println("[CROSS] hide");
    }
    pendingVehAck = "CXH,1";
    return;
  }
  // musicA.18 吧唧显示模式：#BT,<0 图片 | 1 文字横向滚动>（存 NVS，回 ACK BT,<mode>）
  if (m.startsWith("#BT,")) {
    int mode = constrain(m.substring(4).toInt(), 0, 1);
    if (mode != badgeMode) {
      Preferences pr;
      bool saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putUChar("bt_mode", (uint8_t)mode) == 1; pr.end(); }
      if (saved) { badgeMode = mode; if (page == 1) badgeDirty = true; }
      pendingVehAck = "BT," + String(saved ? mode : -1);
    } else {
      pendingVehAck = "BT," + String(mode);
    }
    return;
  }
  // musicA.17 恢复默认开机背景：#BGD（清掉自定义背景，回 faceId 表盘背景）
  if (m == "#BGD") {
    if (bgCustomOn || bgFlashCached) {
      bgCustomOn = false; bgFlashCached = false; bgClearReq = true; faceDirty = true;
      MediaFlash::clearBg();
    }
    pendingBgCfgAck = "BGD,1";
    return;
  }
  // v7.12.10 音乐封面旋转开关：#RC,1/0（持久化 NVS，回 ACK）
  if (m.startsWith("#RC,")) {
    int on = m.substring(4).toInt();
    bool en = on != 0;
    Preferences pr;
    bool saved = pr.begin("motonav", false);
    if (saved) { saved = pr.putBool("cvrspin", en) == 1; pr.end(); }
    if (saved) coverSpinEnabled = en;
    pendingCoverSpinAck = "RC," + String(saved ? (en ? 1 : 0) : -1);
    return;
  }
  // ui.6 音乐封面风格化背景：#MBG,<0 关闭|1 放大虚化|2 色块马赛克|3 双色海报>
  // 存 NVS "mbg"，回执 MBG,<mode>；换档位重建背景并重画音乐页。旧固件忽略未知命令。
  if (m.startsWith("#MBG,")) {
    int mode = constrain(m.substring(5).toInt(), 0, MBG_MODE_MAX);
    bool saved = true;
    if ((uint8_t)mode != musicBgMode) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putUChar("mbg", (uint8_t)mode) == 1; pr.end(); }
      if (saved) {
        musicBgMode = (uint8_t)mode;
        musicBgDirty = true;
        musicChromeReady = false;
        musicDirty = true;
        Serial.printf("[MBG] mode=%d\n", mode);
      }
    }
    pendingVehAck = "MBG," + String(saved ? (int)musicBgMode : -1);
    return;
  }
  // musicA.38 音乐样式：#MS,<0..4>（0 原版·冷雾银 / 1 封面轮播 / 2 柔光立体 / 3 歌词主导 / 4 极简仪表）
  // 存 NVS "mstyle"，回执 MS,<id>；切样式立刻重画左划音乐页。
  if (m.startsWith("#MS,")) {
    int id = constrain(m.substring(4).toInt(), 0, 4);
    bool saved = true;
    if ((uint8_t)id != musicStyle) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) { saved = pr.putUChar("mstyle", (uint8_t)id) == 1; pr.end(); }
      if (saved) {
        musicStyle = (uint8_t)id;
        musicChromeReady = false;
        musicDirty = true;
        Serial.printf("[MS] style=%d\n", id);
      }
    }
    pendingVehAck = "MS," + String(saved ? (int)musicStyle : -1);
    return;
  }
  if (m == "#AMS?") { appleMedia.resend(); return; }
  if (m == "#BC,1" || m == "#BC,0") {
    setRasterBuildingCarry(m == "#BC,1");
    Serial.printf("[BLD] carry=%d\n", (int)rasterBuildingCarryOn);   // musicA.29 排查建筑层用
    return;
  }
  if (m == "#GB?") {
    pendingBuildingCapability = true;
    pendingBuildingCapabilityN = 3;              // musicA.29 连发 3 次，抵掉单条通知丢失
    Serial.println("[BLD] #GB? → CAP,BLD,1");
    return;
  }
  if (m.startsWith("#ZACK,")) {
    if (hidMusicMode) return;
    musicAck = constrain(m.substring(6).toInt(), 0, 5);
    musicAckUntil = millis() + 2500; musicDirty = true; return;
  }
  if (m.startsWith("#Z,")) {
    int have = 0, play = 0, actions = 0;
    long durMs = 0, elapsedMs = 0;
    int n = sscanf(m.c_str(), "#Z,%d,%d,%d,%ld,%ld", &have, &play, &actions, &durMs, &elapsedMs);
    if (n >= 3) {
      if (!hidMusicMode) {
        musicHave = have != 0; musicPlaying = play != 0; musicActions = actions & 7;
        if (n >= 5 && durMs > 0) {
          musicDurMs = (uint32_t)durMs;
          musicElapsedMs = (uint32_t)(elapsedMs > durMs ? durMs : elapsedMs);
          musicProgAt = millis();
        } else if (n == 3) {
          // 旧三字段：保留已有进度；无额外时长字段时不强制清零
        } else {
          musicDurMs = 0; musicElapsedMs = 0; musicProgAt = 0;
        }
      }
      musicDirty = true;
    }
    return;
  }
  if (m.startsWith("#TIME,")) {
    // musicA.3：0.16.2.1+ 发 "#TIME,<utcSec>,<tzOffsetSec>"；旧 App 仍发 "yyyy-MM-dd HH:mm:ss"。
    // musicA.1/.2 只认旧格式 → clkSyncMs 永为 0 → 时间空白 + 秒环不画（现场根因）。
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    bool ok = false;
    long utc = 0; int off = 0;
    if (sscanf(m.c_str(), "#TIME,%ld,%d", &utc, &off) == 2 &&
        utc >= 1577836800L && utc < 4102444800L && abs(off) <= 50400) {
      time_t local = (time_t)(utc + off);
      struct tm t; memset(&t, 0, sizeof(t));
#if defined(ESP_PLATFORM)
      gmtime_r(&local, &t);
#else
      struct tm *p = gmtime(&local); if (p) t = *p;
#endif
      y = t.tm_year + 1900; mo = t.tm_mon + 1; d = t.tm_mday;
      h = t.tm_hour; mi = t.tm_min; s = t.tm_sec;
      ok = (y >= 2020 && mo >= 1 && mo <= 12 && d >= 1 && d <= 31);
    } else if (sscanf(m.c_str(), "#TIME,%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
      ok = true;
    }
    if (ok) {
      clkY = y; clkMo = mo; clkD = d;
      clkDaySec = h * 3600 + mi * 60 + s;
      clkSyncMs = millis() + 1;
      if (onWaitScreen && !sliding) { drawIdleClock(); catForce = true; }
      Serial.printf("TIME SYNC %04d-%02d-%02d %02d:%02d:%02d\n", y, mo, d, h, mi, s);
    }
    return;
  }
  if (m == "P") return;                                 // 心跳
  if (m == "#OFF") { appBye = true; return; }          // zelda40.2 App 主动断开的告别指令：强制离线（挡系统级 HID 重连）
  // musicA.13：恢复「C」清屏（0.16.2.13+ 到达不再自动发 C）。手机点 OK / 退出仍发 C 关庆祝。
  if (m == "C") { stopArriveFx(); progReset(); resetNavState(); drawWaiting(WAIT_READY); return; }

  if (m.startsWith("#v,")) { speedKmh = m.substring(3).toInt(); navDirty = true; return; }
  if (m.startsWith("#G,")) {                           // v7.12.9-ui.7 手机传感器 G 值 "#G,<总G>,<转速°/s>"
    float gv = 0, rv = 0;
    if (sscanf(m.c_str(), "#G,%f,%f", &gv, &rv) >= 1) {
      gCur = constrain(gv, 0.0f, 8.0f);
      gRot = constrain(rv, 0.0f, 2000.0f);
      gRxAt = millis();
      if (gCur > gPeak) gPeak = gCur;                  // 峰值在这里跟踪，页面不在前台也不漏
    }
    return;
  }
  if (m.startsWith("#TP,")) {                          // v7.12.10 手机透传蓝牙胎压气嘴：#TP,<前kPa>,<前°C>,<后kPa>,<后°C>[,<前电量%>,<后电量%>]
    int fk = 0, ft = 0, rk = 0, rt = 0, fb = -1, rb = -1;
    int n = sscanf(m.c_str(), "#TP,%d,%d,%d,%d,%d,%d", &fk, &ft, &rk, &rt, &fb, &rb);
    if (n >= 4) {
      if (fk == 0 && ft == 0 && rk == 0 && rt == 0) {   // App 侧数据过期清零帧：立即归零、熄 LIVE
        tpValid = false;
        tpFrontKpa = 0; tpFrontTemp = 0; tpRearKpa = 0; tpRearTemp = 0;
        tpFrontBatt = -1; tpRearBatt = -1;
        Serial.println("TPMS #TP clear");
        return;
      }
      tpFrontKpa  = constrain(fk, 0, 999);
      tpFrontTemp = constrain(ft, -40, 125);
      tpRearKpa   = constrain(rk, 0, 999);
      tpRearTemp  = constrain(rt, -40, 125);
      tpFrontBatt = (n >= 6) ? constrain(fb, -1, 100) : -1;   // -1 = 未知（不画电量）
      tpRearBatt  = (n >= 6) ? constrain(rb, -1, 100) : -1;
      tpValid = true; tpRxAt = millis();
      Serial.printf("TPMS #TP f=%dkPa/%dC/%d%% r=%dkPa/%dC/%d%%\n", fk, ft, tpFrontBatt, rk, rt, tpRearBatt);
    }
    return;
  }
  if (m.startsWith("#k")) {
    int c1 = m.indexOf(','), c2 = m.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      hasCam = true;
      camLimit = m.substring(c1 + 1, c2).toInt();
      camDist = m.substring(c2 + 1).toInt();
    } else {
      hasCam = false;
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#j")) {
    laneN = 0;
    if (m.length() > 3) {
      String s = m.substring(3);
      int n = s.length() / 2;
      if (n > 8) n = 8;
      for (int i = 0; i < n; i++) { laneBg[i] = hexVal(s[2 * i]); laneFr[i] = hexVal(s[2 * i + 1]); }
      laneN = n;
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#p,")) {                             // v2.0 位置
    int fr = 0, hd = 0, kmh = 0; long xd = 0, yd = 0;
    if (sscanf(m.c_str(), "#p,%d,%ld,%ld,%d,%d", &fr, &xd, &yd, &hd, &kmh) == 5) {
      // v2.6 记下“按旧数据推算的当前位置”和新位置的偏差，画面在 0.4 秒内平滑修正过去
      unsigned long nowMs = millis();
      // v6.7 转弯速度：和上一次定位的朝向差 / 时间（太慢、隔太久、变化太离谱就当没在转）
      float newRate = 0;
      if (fr == posFrame && posAt) {
        float fdt = (nowMs - posAt) / 1000.0f;
        float dhd = hd - posHdg;
        while (dhd > 180) dhd -= 360;
        while (dhd < -180) dhd += 360;
        if (fdt > 0.3f && fdt < 2.5f && kmh >= 6 && fabsf(dhd) < 120) newRate = constrain(dhd / fdt, -45.0f, 45.0f);
      }
      if (fr == posFrame && posAt) {
        float px, py, ph;
        predictPose(nowMs, 0, px, py, ph);              // v6.7 按旧数据（含转弯）推算的现在位置
        float ex = px - xd / 10.0f, ey = py - yd / 10.0f;
        float rem = corrAt ? fmaxf(0, 1 - (nowMs - corrAt) / 400.0f) : 0;   // 上一次还没修正完的部分
        ex += corrX * rem; ey += corrY * rem;
        if (ex * ex + ey * ey < 60 * 60) { corrX = ex; corrY = ey; corrAt = nowMs; }
        else { corrX = corrY = 0; corrAt = 0; }
      }
      posFrame = fr; posX = xd / 10.0f; posY = yd / 10.0f; posHdg = hd; posKmh = kmh; posAt = nowMs;
      hdgRate = hdgRate * 0.3f + newRate * 0.7f;          // v6.7 稍微平滑一下，GPS 朝向偶尔抖一下不至于乱转
      if (inNav) navDirty = true;
    }
    return;
  }
  if (m.startsWith("#q,")) {                             // v2.3 屏幕数据线速度：保存并重启
    int q = m.substring(3).toInt();
    if ((q == 40 || q == 60 || q == 80) && q != qspiMHz) {
      Preferences pr;
      pr.begin("motonav", false);
      pr.putInt("qspi", q);
      pr.end();
      delay(100);
      ESP.restart();
    }
    return;
  }
  if (m.startsWith("#f,")) {                             // v2.6 快速刷屏开关：保存并重启
    bool on = m.substring(3).toInt() != 0;
    if (on != fastFlushOn) {
      Preferences pr;
      pr.begin("motonav", false);
      pr.putBool("fastf", on);
      pr.end();
      delay(100);
      ESP.restart();
    }
    return;
  }
  if (m.startsWith("#t,")) {                             // v2.9 地图配色主题
    int t = m.substring(3).toInt();
    if (t >= 0 && t < THEME_N) {
      // App 8.3.23 adds a third field: each style now owns its map data.
      // Old Android/iOS #t,t commands keep their legacy recoloring behavior.
      if (t != themeIdx && m.indexOf(',',3)>=0) {
        cancelPendingRaster();
        rasterValid=false; rasterBackValid=false; mapValid=false;
        for(int kind : {1,3}) {
          Bulk &b=bulks[kind];b.id=-1;b.total=0;b.got=0;b.done=false;
          memset(b.mask,0,sizeof(b.mask));
        }
      }
      themeIdx = t; navDirty = true;
    }   // v7.8.11 0~11
    return;
  }
  if (m.startsWith("#Q,")) { curveOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v6.2 弯道上色
  if (m.startsWith("#R,") || m.startsWith("#D")) {
    // iOS sends #R,nearestQuarter,exactDegrees; Android sends #R then #Ddegrees.
    // Preserve legacy #R,quarter while applying the exact angle when supplied.
    int d;
    if (ScreenRotationCommand::parse(m.c_str(), d) && d != screenDeg) {
      setScreenDeg(d, true); navDirty = true; needFlush = true;
    }
    return;
  }
  if (m.startsWith("#L")) {                              // v6.7 预判提前量："#L300"
    leadMs = constrain(m.substring(2).toInt(), 0, 800);
    return;
  }
  if (m == "#UNPAIR") {                                  // v6.5 忘掉配对过的手机；v6.6 只忘“别的”手机，正在用的这台保留
#if defined(CONFIG_NIMBLE_ENABLED)
    ble_addr_t keep;
    bool haveKeep = false;
    ble_gap_conn_desc d;
    if (bleConnHandle != 0xFFFF && ble_gap_conn_find(bleConnHandle, &d) == 0) { keep = d.peer_id_addr; haveKeep = true; }
    ble_addr_t peers[8];
    int n = 0;
    if (ble_store_util_bonded_peers(peers, &n, 8) == 0) {
      for (int i = 0; i < n; i++) {
        if (haveKeep && ble_addr_cmp(&peers[i], &keep) == 0) continue;
        ble_gap_unpair(&peers[i]);
      }
    }
#endif
    return;
  }
  if (m.startsWith("#e")) {                              // v3.1 行程信息：剩余米,到达时间HHMM,红绿灯数
    int rm = 0, eta = 0, lt = 0;
    if (sscanf(m.c_str(), "#e,%d,%d,%d", &rm, &eta, &lt) == 3) {
      // v7.3 路程进度：剩余变少就算走掉了（一次少太多的当成重新规划，不算）
      unsigned long nowMs = millis();
      if (progLastRm >= 0) {
        long d = (long)progLastRm - rm;
        long cap = 200 + (long)((nowMs - progLastAt) / 1000) * 60;     // 最快按 60 米/秒算
        if (d > 0 && d <= cap) progDone += d;
      }
      progLastRm = rm; progLastAt = nowMs;
      tripRemainM = rm; tripEta = eta; tripLights = lt; tripOn = true;
      long total=progDone+tripRemainM;
      if(total>0)tripProgress.sample(progDone/(float)total,nowMs);
    }
    else tripOn = false;
    navDirty = true;
    return;
  }
  if (m.startsWith("#c")) {                              // v3.5 小地图上的电子眼位置；只有 #c 表示清空
    int oldN = mapCamN;
    int32_t oldX[MCAM_MAX], oldY[MCAM_MAX]; int oldT[MCAM_MAX]; int8_t oldP[MCAM_MAX], oldK[MCAM_MAX]; float oldH[MCAM_MAX], oldA[MCAM_MAX];
    for (int k = 0; k < oldN; k++) { oldX[k] = mapCamX[k]; oldY[k] = mapCamY[k]; oldT[k] = mapCamTyp[k]; oldP[k] = mapCamPref[k]; oldH[k] = mapCamLockH[k]; oldA[k] = mapCamPrefA[k]; oldK[k] = mapCamPrefK[k]; }
    mapCamN = 0;
    const char *q = m.c_str() + 2;
    if (*q == ',') {
      char *e;
      mapCamFrame = strtol(q + 1, &e, 10); q = e;
      while (*q == ',' && mapCamN < MCAM_MAX) {
        long v[4]; bool ok = true;
        for (int k = 0; k < 4; k++) {
          if (*q != ',') { ok = false; break; }
          v[k] = strtol(q + 1, &e, 10); q = e;
        }
        if (!ok) break;
        mapCamX[mapCamN] = v[0]; mapCamY[mapCamN] = v[1]; mapCamSpd[mapCamN] = v[2]; mapCamTyp[mapCamN] = v[3];
        // v3.8 同一个电子眼（坐标一样）沿用上次的图标位置和锁定朝向（v4.0）
        int8_t pref = -1;
        float lockH = 0, prefA = 0; int8_t prefK = 0;
        for (int k = 0; k < oldN; k++)
          if (oldX[k] == v[0] && oldY[k] == v[1] && oldT[k] == v[3]) { pref = oldP[k]; lockH = oldH[k]; prefA = oldA[k]; prefK = oldK[k]; break; }
        mapCamPref[mapCamN] = pref; mapCamLockH[mapCamN] = lockH; mapCamPrefA[mapCamN] = prefA; mapCamPrefK[mapCamN] = prefK;
        mapCamN++;
      }
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#g")) {                              // v3.1 前方红绿灯距离；只有 #g 表示隐藏
    int d = -1;
    lightDist = (sscanf(m.c_str(), "#g,%d", &d) == 1) ? d : -1;
    navDirty = true;
    return;
  }
  if (m.startsWith("#n,")) { nightOn = m.substring(3).toInt() != 0; navDirty = true; faceDirty = true; return; }   // v3.0 夜间配色（musicA.37：动态背景/时间样式也跟着换配色）
  if (m == "#o") { ghostN = 0; navDirty = true; return; }   // v4.4 不显示原路线
  if (m == "#WXRESET") { wxHave = false; wxExtra = false; wxDetail = 0; wxDirty = true; return; }
  if (m.startsWith("#WX,")) { parseWeatherExtra(m); return; }
  if (m.startsWith("#W,")) { parseWeather(m); return; }   // v5.3 天气
  if (m.startsWith("#i,")) {                             // v5.1 来电："#i,1,号码" / "#i,0"
    bool on = m.charAt(3) == '1';
    if (on) {
      if (!callOn) { callAt = millis(); callAnswerSent = false; touchHoldAt = millis(); touchHold = 0; }
      callSeenAt = millis();
      int c2 = m.indexOf(',', 4);
      callNum = c2 > 0 ? m.substring(c2 + 1) : "";
      if (callNum.length() > 16) callNum = callNum.substring(0, 16);
    }
    callOn = on;
    if (!on) navDirty = true;
    return;
  }
  if (m.startsWith("#y,")) {                             // v5.0 重新规划中
    rerouteAt = m.substring(3).toInt() ? max(1UL, millis()) : 0;
    if (rerouteAt) fasterAt = 0;
    navDirty = true; return;
  }
  if (m.startsWith("#h")) {                              // v5.0 更快路线：省几分钟；只有 #h 表示收起
    if (m.length() > 3) { fasterMin = m.substring(3).toInt(); fasterAt = max(1UL, millis()); } else fasterAt = 0;
    navDirty = true; return;
  }
  if (m.startsWith("#x,")) { gpsWeak = m.substring(3).toInt() != 0; navDirty = true; return; }   // v5.0 GPS 信号弱
  if (m.startsWith("#u")) {                              // v5.0 路况：起,止,状态,...；只有 #u 表示清空
    trafN = 0;
    const char *q = m.c_str() + 2;
    while (*q == ',' && trafN < TRAF_MAX) {
      char *e; long a = strtol(q + 1, &e, 10); q = e; if (*q != ',') break;
      long b = strtol(q + 1, &e, 10); q = e; if (*q != ',') break;
      long st = strtol(q + 1, &e, 10); q = e;
      trafA[trafN] = a; trafB[trafN] = b; trafS[trafN] = (uint8_t)st; trafN++;
    }
    navDirty = true; return;
  }
  if (m.startsWith("#w,")) { cleanOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v4.9 清爽模式
  if (m.startsWith("#z,")) {                             // v4.8 巡航模式开关
    cruiseOn = m.substring(3).toInt() != 0;
    if (cruiseOn) { inNav = true; navDir = "S"; navDist = 0; ringActive = false; tripOn = false; laneN = 0; forceExitBadge(true); }
    navDirty = true;
    return;
  }
  if (m.startsWith("#BM,")) {                            // 电子吧唧模式 0/1（bulk type 7；右滑进页）
    int on = m.substring(4).toInt();
    if (on) enterBadgeMode(); else forceExitBadge(true);
    return;
  }
  if (m.startsWith("#r,")) {                             // v3.0 走过的路线：1 变灰，2 擦除
    int r = m.substring(3).toInt();
    if (r == 1 || r == 2) { trailMode = r; navDirty = true; }
    return;
  }
  if (m.startsWith("#m,")) { mapOn = m.substring(3).toInt() != 0; navDirty = true; return; }
  if (m.startsWith("#b,")) {
    int b = constrain(m.substring(3).toInt(), 5, 255);
    if (flushDone) {                                    // v2.2 刷屏任务在用总线时先等它推完
      xSemaphoreTake(flushDone, portMAX_DELAY);
      noteBrightnessForWake(b);                         // v7.12.9-ui.3 记录亮度供 PWR 亮屏恢复；息屏中保持黑屏
      xSemaphoreGive(flushDone);
    } else {
      noteBrightnessForWake(b);
    }
    return;
  }

  if (m.startsWith("#O")) {                              // v7.1 行程统计 "#O米/分钟/最高车速"
    long a = -1; int b = -1, c = -1;
    if (sscanf(m.c_str() + 2, "%ld/%d/%d", &a, &b, &c) >= 2) { tripM = a; tripMin = b; tripMaxKmh = c; tripAt = millis(); }
    return;
  }
  if (m == "#M1" || m == "#M0") { progOn = m[2] == '1'; navDirty = true; return; }   // v7.3 路程进度开关
  if (m == "#J") { if (page == 0) startArriveFx(true); return; }   // v7.1 到达途经点
  if (m.startsWith("#")) return;                        // v6.7 不认识的 "#..." 指令：忽略（别当成导航方向）
  int comma = m.indexOf(',');                           // 导航：方向,距离
  if (comma > 0) {
    String prevDir = navDir;
    String newDir = m.substring(0, comma);
    // ui.5：清屏 / 断连之后收到的孤立 "A,0" 是 App 重连补发或队列尾帧，不是新一次到达，
    // 直接丢弃，别把圆屏拉回「剩余 0」的导航页。真正到达时 prevDir 必是 L/R/S/... 方向；
    // 导航途中圆屏重启补发的是方向帧（newDir != "A"），两种情况都不受影响。
    if (newDir == "A" && !inNav && !prevDir.length()) return;
    navDir = newDir;
    // v7.1 导航中 → 到达：放特效（重连补发的 "A,0"、圆屏刚开机收到的都不放）
    if (navDir == "A" && prevDir.length() && prevDir != "A" && page == 0) startArriveFx(false);
    if (prevDir == "A" && navDir != "A") progReset();       // v7.3 到达后又开始新的导航：进度从头算
    else if (navDir != "A" && fxOn && !fxManual) stopArriveFx();  // zelda40.6：目的地到达(#A 触发)只点 OK 关，新导航不抢占
    int comma2 = m.indexOf(',', comma + 1);
    if (comma2 > 0) {
      navDist = m.substring(comma + 1, comma2).toInt();
      navAngle = m.substring(comma2 + 1).toFloat();
    } else {
      navDist = m.substring(comma + 1).toInt();
      navAngle = NAN;
    }
    inNav = true;
    forceExitBadge(true);                                // 导航开始强制退出吧唧
    if (navDir != "A") idlePowerOffReset(&idlePowerOffTimer, millis());
    updateRingTarget();
    navDirty = true;
  }
}

void handleMessage(const uint8_t *d, int n) {
  lastRxMs = millis();
  appBye = false;                                      // zelda40.2 收到任何 App 消息 = App 真回来了，解除 #OFF 离线
  if (staleShown) { staleShown = false; navDirty = true; }   // 收到任何消息，清掉数据中断提示
  if (d[0] == 'B') { handleBitmap(d, n); return; }
  if (d[0] == 'M') { handleBulk(d, n); return; }     // v2.0 地图块 / 路线
  char buf[512];                                      // v4.8 64 → 512：电子眼列表长了会被截断
  int len = n < 511 ? n : 511;
  memcpy(buf, d, len);
  buf[len] = 0;
  String m = String(buf);
  m.trim();
#if DEBUG_SERIAL
  Serial.println("recv: " + m);
#endif
  handleText(m);
}
