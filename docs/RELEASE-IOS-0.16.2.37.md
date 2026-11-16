# MotoNav iOS 0.16.2.37（build 47）

配套固件：**musicA.39**（本版不改固件）

## 修复 · Path A 触底恢复四键底栏

0.16.2.36 使用 `.tabBarMinimizeBehavior(.onScrollDown)` 后，下滚会收成单图标；滚回顶部会展开，但**滚到内容底部仍保持收起**（与 Apple Music 触底恢复完整底栏不一致）。

根因：系统 `onScrollDown` 文档/HIG 只保证「回顶或点 Tab」退出收起态，**不保证触底展开**；我们又没有把主 `ScrollView` 的底部边沿反馈给 minimize API。

修复：

- 保留下滚收起（`.onScrollDown`）。
- 主 Tab `ScrollView` 用 `onScrollGeometryChange`：触底（含 rubber-band）/ 上滑 / 回顶时切到 `.never` 强制展开四键；中部下滚再切回 `.onScrollDown`。
- `.scrollBounceBehavior(.always)` 方便触底回弹被识别。
- 连接动画、Duo `sidebarAdaptable`、Liquid Glass TabView 不变。
