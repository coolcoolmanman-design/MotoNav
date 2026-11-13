# MotoNav 固件 musicA.78 · FO-225f 封面流按钮切歌不滚修复

> 圆屏音乐「封面流」主题（style 7，Cover Flow）的体验修正。**仅固件改动**，iOS 仍为
> 0.16.2.70，无需重新侧载 IPA。所有 Release 一律 draft（published_at=None）。

## 现象（实机反馈）
圆屏进入封面流后，点底栏「上一首 / 下一首」按钮，手机正常切歌，但**卡组（Cover Flow）
完全不跟着滚动**——只有拖动卡带时才动。HID/AMS 第三方播放器不会主动重发 type 12 队列，
于是视觉上封面流像卡死。

## 根因
点按钮走的是 `musicTap()` → `cfSendButton()` 直接发切歌命令并 `return`，**从未进入吸附
动画分支**。而拖动路径是实时改 `cfPhase`、松手由 `cfSnapTick()` 收尾滚动一格。两者行为
不对称：拖动有本地滚动反馈，点按钮没有，只能干等手机重发队列（往往不来）。

## 修复
- 新增 `cfStartSnap(dir)`（`MusicThemes.h`，紧挨 `cfSnapTick` 之前）：设置
  `cfSnapFrom=cfPhase; cfSnapTo=±1; cfSnapAt/Snap=true`，与拖动松手共用收尾逻辑。
- `musicTap()`（PageNavigation.h）在 `musicStyle==7 && (button==0||2)` 时**先起吸附动画再
  return**，让卡组立刻本地滚动一格作为即时反馈；真正的切歌命令仍在 `cfSnapTick()` 收尾时
  由 `cfSendButton()` 发出（HID 脉冲 / App `T,M,P|N` 两条路径都覆盖，行为与原按钮一致）。
- 播放/暂停（button==1）与未命中不改，仍走原逻辑。
- `cfStartSnap` 有 `!cfSnap && !cfGesture` 守卫，动画进行中再次点按被忽略，天然防连击。

## 行为对照
| 操作 | 修复前 | 修复后 |
|---|---|---|
| 拖动卡带 | 跟手 + 吸附滚动 | 不变 |
| 点 prev/next 按钮（App 模式） | 只发命令，卡组不动 | 立即本地滚一格，吸附收尾发 `T,M,P/N` |
| 点 prev/next 按钮（HID/AMS） | 只发脉冲，卡组不动 | 立即本地滚一格，吸附收尾发 HID 脉冲 |
| 点播放/暂停 | 不变 | 不变 |

> 注：本地滚动一格只是“预览”——若手机重发队列（App 模式通常会），`cfShift` 归零且换成
> 新当前封；若 HID/AMS 不重发，卡组停在本地缓存的相邻槽，仍是可见的滚动反馈，不再是死图。

## 发布与刷机
- CI workflow：固件 `release-firmware.yml`（iOS 未触发，App/协议未改）。
- 编译体积：本地 `python firmware/build.py --cli ... --jobs 8`：**Flash 2,951,027 B（93%）/
  RAM 145,468 B（44%）**，相对 musicA.77（2,950,871 B / 145,468 B）仅增加 **156 B**，RAM 零
  增长，Flash 余量仍约 195 KB。
- 交付：固件 draft（untagged），官方 `app.bin` 校验 SHA256 一致后写 COM5 0x10000，不擦 NVS。
- 开机日志 `logs/boot-musicA78.log` 全绿（版本 78 / I2C 五设备 / QMI8658 / touch）。

## 已知边界
- 装车若把圆屏旋转过角度，`fastPushRect()` 会退化成整屏约 12fps，吸附动画会顿挫，需实机确认。
- 真播放队列（下一首封面）仍不做（沿用 FO-225b 结论，MusicKit 只有 `ApplicationMusicPlayer`
  能读 up-next，会废掉 HID/AMS 主用法）。
