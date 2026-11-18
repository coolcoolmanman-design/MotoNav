# iOS 0.16.2.95 · musicA.104

全屏转动封面稳态帧率 P0 配套包。Draft-only。无协议变更。

## 本版

- **配套固件 musicA.104**：style 5 旋帧 ~16.7fps（60ms）稳态；进度局部刷新，禁 500ms 全量 dirty
- 基线继承 0.16.2.94 / musicA.103（chrome blit + COVER_HI 452）

## 版本

- iOS `0.16.2.95` · build `104`
- FW `7.12.10-ui.2-zelda40.9-musicA.104`

## 刷机

1. 分区表 `0x8000` → app `0x10000` 刷 musicA.104
2. 装本 Draft IPA
