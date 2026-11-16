# iOS 0.16.2.79 · musicA.85

基线 **0.16.2.78** / **musicA.84**（`2f06540`）。Draft 不 publish。

## 紧急

**musicA.84 不安全**：实机 app 镜像 **3145920** > 分区 **3145728**（超 192 B）→ bootloader 拒写 / 无限重启。  
恢复请先刷 **musicA.83**：https://github.com/coolcoolmanman-design/MotoNav/releases/tag/untagged-3d4e84f98f8c549f1078

## 本包

- Flash 超限紧急修复：再瘦 ST_DRAGON（去网格/ping/光点表；保留极简扫线 + 四星球）。
- 本地校验 `app.bin 3144160`（余量 1568 B ≥ 1.5 KiB）。
- 保留申花 large、开机 AA、等红灯。

## 版本

- iOS `0.16.2.79` · build `88`
- FW `7.12.10-ui.2-zelda40.9-musicA.85`
