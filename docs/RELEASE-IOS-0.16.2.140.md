# MotoNav iOS 0.16.2.140

- 屏幕直出 P3D（配套固件 musicA.167）：高德透视图按固件几何反推为 233×233 屏幕图，固件 2× 放大直接贴图，去掉 LUT 重投影。
- P3D 门控放宽：不再被瓦片 chunk / 蓝牙 bulk 队列拦截，P3D 以 prio 10 插队发送，最短 3 秒一帧，挪 20 m 或转 6° 换帧。
- 固件侧配套版本：7.12.10-ui.2-zelda40.9-musicA.167（P3D 屏幕直出见 firmware/docs/persp-3d-protocol.md 第 7 节）。

注意：需要配套固件 167 或更新版本，旧固件不识别屏幕直出帧（flags.bit1）。

