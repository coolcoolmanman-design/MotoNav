#pragma once
// 地图和路线大块数据分包接收。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// 'M' 类型 编号 总片数(2) 片序号(2) 总字节(4) 偏移(4) + 数据
// type 7 = 电子吧唧；type 8 = 自定义开机背景（musicA.17）；type 11 = 中央自定义精灵（musicA.50）；type 6 拒收（上游安卓 TYPE_COVER）。
void handleBulk(const uint8_t *d, int n) {
  if (n < 15) return;
  int type = d[1], id = d[2];
  int parts = d[3] | (d[4] << 8), seq = d[5] | (d[6] << 8);
  uint32_t total = (uint32_t)rd32(d + 7), off = (uint32_t)rd32(d + 11);
  if (type < 1 || type > 11 || type == 6) return;     // type 6 预留给上游安卓 TYPE_COVER；…；11=中央精灵
  Bulk &b = bulks[type];
  if (!b.buf || total == 0 || total > b.cap || parts == 0 || parts > 4096 || seq >= parts) {
    if (type == 9 || type == 10) Serial.printf("[CROSS] 拒收 type=%d buf=%p total=%u cap=%u parts=%d seq=%d\n",
                                               type, (void*)b.buf, (unsigned)total, (unsigned)b.cap, parts, seq);
    return;
  }
  if (id != b.id || total != b.total || parts != b.parts) {     // 新的一份：从头开始收
    if(type==3)cancelPendingRaster();
    if (type == 9 || type == 10) Serial.printf("[CROSS] rx start type=%d id=%d total=%u parts=%d\n",
                                               type, id, (unsigned)total, parts);
    b.id = id; b.total = total; b.parts = parts; b.got = 0; b.done = false;
    memset(b.mask, 0, sizeof(b.mask));
  }
  if (b.done) { pendingAck = "K," + String(type) + "," + String(id); return; }   // 重复的：再确认一次
  if(type==3 && pendingRasterWork.active)return; // A duplicate tail must not acknowledge unfinished work.
  uint32_t len = n - 15;
  if (off >= total) return;
  if (off + len > total) len = total - off;
  if (!(b.mask[seq >> 3] & (1 << (seq & 7)))) {
    memcpy(b.buf + off, d + 15, len);
    b.mask[seq >> 3] |= (1 << (seq & 7));
    b.got++;
  }
  if (b.got >= b.parts) {
    b.done = true;
    if (type == 1) {
      uint8_t *t = mapShow; mapShow = b.buf; b.buf = t;   // 交换：新图显示，旧缓冲留着收下一块
      parseMap(mapShow, total);
    } else if (type == 2) {
      parseRoute(b.buf, total);
    } else if (type == 4) {
      parseGhost(b.buf, total);                        // v4.4 原路线
      } else if (type == 5) {
      parseCover(b.buf, total);                        // 音乐封面（失败也确认，不重发）
    } else if (type == 7) {
      parseBadge(b.buf, total);                        // 电子吧唧（失败也确认）
    } else if (type == 8) {
      parseBg(b.buf, total);                           // 开机背景（失败也确认，App 侧兜底重发）
    } else if (type == 11) {
      parseCsp(b.buf, total);                          // 中央自定义精灵（失败也确认）
    } else if (type == 9 || type == 10) {
      parseCross(b.buf, total, id);                // 路口放大图（9 全图 / 10 预览，按包内 w/h 分流）
    } else if (type == 3) {
      b.done=false;
      if (!queueRasterWork(b.buf,total,id)) {
        // 没有成功解码就不回“已确认”，并允许相同编号完整重发。
        b.done=false; b.got=0; memset(b.mask,0,sizeof(b.mask));
        return;
      }
      if(pendingRasterWork.active)return;
      b.done=true;
    } else {
      return;
    }
    pendingAck = "K," + String(type) + "," + String(id);
    navDirty = true;
  }
}
