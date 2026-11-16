# MotoNav iOS 0.15.40-tpms

Draft-only。配套固件建议：`7.12.10-ui.2-zelda40.7-mapstep.1`。无 PR。

## 相对 0.15.39

1. **息屏地图（cherry-pick 自上游 Build16）**  
   - `MapFeeder.setRenderingAvailable`：后台/息屏继续发已缓存底图/楼块；暂停 SDK 抓图，减轻 cancel/resend 抖动  
   - `MotoNavApp` scenePhase 接线

2. **偏航恢复**  
   - 新增 `MotorReroutePolicy`；`WebMotor` 确认门槛 + 超时/失败恢复；保留「非机动车」文案  
   - `MotorRouteTracker` 匹配优化

3. **底栏液态玻璃（动态滚动）**  
   - 对齐用户上传 Apple Music 风参考视频：玻璃胶囊罩住图标+文案，`matchedGeometry` 左右滚动  
   - 色散边 / Fresnel / 高光；选中系统蓝，未选中黑  
   - **连接动画未改**

## 版本

- `0.15.40-tpms` / `MARKETING_VERSION` 0.15.40  
- 分支：`cursor/ios-0.15.40-map-glass-7061`  
- IPA：`MotoNav-0.15.40-tpms-unsigned.ipa`

参考：`media/ios-0.15.40-liquid-glass/tab-liquid-glass-ref.mp4` · frame-001…017
