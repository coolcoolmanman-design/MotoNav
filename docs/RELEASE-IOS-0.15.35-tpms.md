# MotoNav iOS 0.15.35-tpms

通用圆屏固件配套 · 基于 `0.15.34-tpms`：**电子吧唧多图轮播** + **自选区域裁切** + 圆屏/连接壳 UI 修复 + 底栏液态玻璃点按反馈。

**本包为 Draft / 脱敏侧载包，供自测；未对 main 合入、不推上游。无公开 PR。**

## 本版内容

### A · 电子吧唧升级
- 多图（最多 3 张）：缩略条 + 圆预览点位；状态「已上传 N 张 · 圆屏轮播」
- 裁切：默认 **自动居中**；新增 **自选区域** 编辑器（拖选方区）；输出仍 **466×466**
- BLE bulk **type 7** `ver=2`（`slot` + `count`）；兼容设备 `7.12.11-opt.12` 多缓冲轮播
- iOS 15：`UIImagePickerController`（无 PhotosPicker / 无 iOS 17-only onChange）

### B · UI 修复
- 圆屏 Tab：软椭圆接触投影（替换方影板）+ 设备图略左移
- 连接动画：恢复 **磨砂银** + 侧格栅 5 孔 + 两实体键 + 软椭圆影（抑制白塑壳）

### C · 底栏
- 点按冷银青 **液态玻璃** 透镜反馈；切 Tab 微折射；无紫色光晕

### 固件

请配对 **`7.12.11-opt.12`**（多槽 type 7、左右滑切图、自动轮播、`CAP,BDG,2`）。

## 安装

- IPA：`MotoNav-0.15.35-tpms-unsigned.ipa`（unsigned 侧载）；同 Bundle ID 覆盖安装。
- 高德 Key **不在包内**。

## 脱敏说明

- 无内置高德 Key / 账号 / 序列号 / 私钥。
- 源码 zip 为 git archive，不含本机 UserDefaults、证书、mobileprovision。
