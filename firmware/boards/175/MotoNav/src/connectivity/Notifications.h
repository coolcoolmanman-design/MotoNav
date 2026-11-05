#pragma once
// 配对、地图确认、媒体等蓝牙通知。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
void sendPendingAck() {
  if (!txChar || !bleConnected || !bleAuthed) return;    // v6.5 没通过验证的手机什么都不发
  // v6.5 告诉手机“验证通过了”：连上后 0.3 / 1.5 / 4 秒各发一次（手机订阅通知可能比加密晚一点）
  static const uint16_t uAt[3] = {300, 1500, 4000};
  if (authNotifies < 3 && millis() - bleAuthAt > uAt[authNotifies]) {
    authNotifies++;
    txChar->setValue((uint8_t *)"U,1", 3);
    txChar->notify();
    txChar->setValue((uint8_t *)"CAP,HID,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,LYR,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,BDG,2", 9); txChar->notify();   // 吧唧多图；bulk type 7 / ACK K,7,id
    txChar->setValue((uint8_t *)"CAP,BGR,1", 9); txChar->notify();   // musicA.17 自定义开机背景；bulk type 8 / ACK K,8,id
    txChar->setValue((uint8_t *)"CAP,CXG,1", 9); txChar->notify();   // musicA.26 路口放大图；bulk type 9 / ACK K,9,id
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    appleMedia.resend();
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    pendingMediaStatus = String("AMC,") + appleMedia.controlMask();
  }
  // v7.12.9-ui.7 时间兜底：连上但一直没收到 #TIME（首连配对时 App 的对时消息
  // 可能被状态/图片突发推送挤掉）→ 每 10 秒主动问手机要一次；
  // 旧版 App 收到 "TIME?" 会无害忽略，新版 App 立即回发 #TIME
  static unsigned long timeReqAt = 0;
  if (!clkSyncMs && bleAuthAt && millis() - bleAuthAt > 3000 && millis() - timeReqAt > 10000) {
    timeReqAt = millis();
    txChar->setValue((uint8_t *)"TIME?", 5); txChar->notify();
  }
  if (pendingLyricsStatus.length()) { txChar->setValue((uint8_t *)pendingLyricsStatus.c_str(), pendingLyricsStatus.length()); txChar->notify(); pendingLyricsStatus = ""; }
  if (pendingMediaStatus.length()) { txChar->setValue((uint8_t *)pendingMediaStatus.c_str(), pendingMediaStatus.length()); txChar->notify(); pendingMediaStatus = ""; }
  uint8_t amsFrame[139]; size_t amsSize = 0;
  if (appleMedia.nextFrame(amsFrame, amsSize, millis())) { txChar->setValue(amsFrame, amsSize); txChar->notify(); }
  if (pendingHidStatus.length()) {
    txChar->setValue((uint8_t *)pendingHidStatus.c_str(), pendingHidStatus.length());
    txChar->notify(); pendingHidStatus = "";
  }
  if (pendingBootAck.length()) {
    txChar->setValue((uint8_t *)pendingBootAck.c_str(), pendingBootAck.length());
    txChar->notify(); pendingBootAck = "";
  }
  if (pendingFaceCfgAck.length()) {                          // musicA.8 组件槽配置 ACK
    txChar->setValue((uint8_t *)pendingFaceCfgAck.c_str(), pendingFaceCfgAck.length());
    txChar->notify(); pendingFaceCfgAck = "";
  }
  if (pendingVehAck.length()) {                              // musicA.17 载具形态 ACK
    txChar->setValue((uint8_t *)pendingVehAck.c_str(), pendingVehAck.length());
    txChar->notify(); pendingVehAck = "";
  }
  if (pendingBgCfgAck.length()) {                            // musicA.17 背景清除 ACK
    txChar->setValue((uint8_t *)pendingBgCfgAck.c_str(), pendingBgCfgAck.length());
    txChar->notify(); pendingBgCfgAck = "";
  }
  if (pendingFaceAck.length()) {
    txChar->setValue((uint8_t *)pendingFaceAck.c_str(), pendingFaceAck.length());
    txChar->notify(); pendingFaceAck = "";
  }
  if (pendingCoverSpinAck.length()) {                       // v7.12.10 音乐封面旋转开关回执
    txChar->setValue((uint8_t *)pendingCoverSpinAck.c_str(), pendingCoverSpinAck.length());
    txChar->notify(); pendingCoverSpinAck = "";
  }
  if (pendingBuildingCapability) {
    txChar->setValue((uint8_t *)"CAP,BLD,1", 9); txChar->notify(); pendingBuildingCapability=false;
  }
  if (pendingBadgeCapability) {
    txChar->setValue((uint8_t *)"CAP,BDG,2", 9); txChar->notify(); pendingBadgeCapability=false;
  }
  if (pendingAck.length()) {
    txChar->setValue((uint8_t *)pendingAck.c_str(), pendingAck.length());
    txChar->notify();
    pendingAck = "";
  }
  if (pendingStat2.length()) {
    txChar->setValue((uint8_t *)pendingStat2.c_str(), pendingStat2.length());
    txChar->notify();
    pendingStat2 = "";
  }
  if (pendingProf.length()) {                         // v5.8 分步用时
    txChar->setValue((uint8_t *)pendingProf.c_str(), pendingProf.length());
    txChar->notify();
    pendingProf = "";
  }
  if (pendingBat.length()) {                          // v6.8 电量
    txChar->setValue((uint8_t *)pendingBat.c_str(), pendingBat.length());
    txChar->notify();
    pendingBat = "";
  }
  if (pendingEvt.length()) {                          // v5.2 触摸事件
    txChar->setValue((uint8_t *)pendingEvt.c_str(), pendingEvt.length());
    txChar->notify();
    pendingEvt = "";
  }
}
