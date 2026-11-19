# musicA.115 — WiFi 地图加速：导航态真实腾内存

## .114 实测结论
- bringup 时堆仍是 largest=18420（与 .113 相同）：解压器/车道/临时数组均为懒分配，
  待机态从未分配，合并与 PSRAM 化在 bringup 时省为零。
- 真正瓶颈：BLE 连接态内部最大连续块 18.4KB，WiFi SoftAP init 需要更大块
  （.110 preinit 在 38.9KB 时成功过），门槛在两者之间。

## .115 修法（针对导航态真实占用）
1. **抑制缓存再分配**：WiFi 加速期间 wifiMapNoInternalCaches=true，
   reclaim 释放 ringFastColors(12.5K)/vigKeep(12.9K) 后雷达**不再重新分配**，
   走既有降级路径（无 fast colors / 软件 vignette）；关热点后自动恢复。
2. **共享解压器改 PSRAM**：开机动画启动即分配且常驻的 11KB 内部 RAM 移到 PSRAM，
   待机态与导航态都真实释放；解压速度略降可接受。

## 预期
- 导航态 bringup：reclaim 25KB + 解压器 11KB = 真实腾出 36KB 内部连续块候选，
  largest 有望越过 WiFi init 门槛。
- 待机态 bringup：解压器 11KB 释放（18420 → 约 30KB，能否过门槛待实测）。

## 代价
- WiFi 加速期间雷达进度环颜色缓存与 vignette 用降级绘制（视觉略简）。
- 关闭热点后恢复原状。
