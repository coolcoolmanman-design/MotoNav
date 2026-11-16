# 固件 7.12.10-ui.2-zelda40.9-musicA.53

配套 iOS：`0.16.2.50` / `0.16.2.51`。基线 musicA.52（`0793af8`）。

## 一、相对 musicA.52 的差分（诚实）

- **仅版本串**：`FW_VERSION` / `firmware/RELEASE_VERSION` → `musicA.53`；协议与渲染逻辑 **无变更**。
- CSP 通路不变（bulk 11 / ffat sleep·chase / chroma 0）。
- 组合确认稿烤帧只在 App；固件仍只渲染收到的 CSP 帧。
- `#FV,4` = 组合；`#FV,2` 自行车仍隐藏。

> 因此刷 **musicA.52** 与刷 **musicA.53** 对组合预览/推送行为等价；53 仅为配套版本对齐与 Draft 标签。

## 二、其它

- musicA.51 矢量简图与既有功能不变。

## CI

- 源码构建 Draft；Arduino CLI 改从 GitHub release 安装，ESP32 core 带 cache + 退避重试（避免 downloads.arduino.cc / Espressif 504）。
