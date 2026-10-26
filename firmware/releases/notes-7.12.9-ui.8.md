# v7.12.9-ui.8

1. 修复 App 端点「断开连接」后圆屏仍显示 ready：圆屏是已绑定的 HID 设备，iOS 会在 App 断开后由系统自动重连（链路通、已认证）。现在把「手机订阅了 TX 通知通道」作为 App 在场的判据（`bleConnected && bleAuthed && txSubscribed`）——系统级 HID 重连不会订阅自定义 TX 特征，此时待机页正确显示 no link；App 重新连接订阅后恢复 ready。
