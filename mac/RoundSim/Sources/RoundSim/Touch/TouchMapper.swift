import Foundation
import CoreGraphics

/// Waveshare 1.75 / boards/175: `TOUCH_FLIP_X=1`, `TOUCH_FLIP_Y=1` (musicA.13 / PageNavigation.h).
/// Mouse/trackpad already yields canvas coords; optional `simulateDeviceRaw` applies inverse flip so
/// the same `touchVectorToCanvas` path used on device can be exercised for OK / music hits.
enum TouchMapper {
    static let lcd: CGFloat = 466
    static let cx: CGFloat = 233
    static let touchFlipX = true
    static let touchFlipY = true

    /// Convert a view click in canvas space into the coordinate space `fxOkHit` / `musicButtonAt` expect
    /// after `touchVectorToCanvas` (i.e. canvas). When `simulateDeviceRaw` is on, treat the click as
    /// CST raw (pre-flip) by first mirroring like the panel, then flipping back via the FW path.
    static func canvasPoint(from viewPoint: CGPoint, simulateDeviceRaw: Bool) -> CGPoint {
        if !simulateDeviceRaw { return viewPoint }
        // viewPoint is visual/canvas; invent raw by inverse of touchVectorToCanvas (rot=0):
        // canvas = flip(raw - center) + center  ⇒  raw = flip(canvas - center) + center
        var dx = viewPoint.x - cx
        var dy = viewPoint.y - cx
        if touchFlipX { dx = -dx }
        if touchFlipY { dy = -dy }
        let raw = CGPoint(x: cx + dx, y: cx + dy)
        return touchVectorToCanvas(raw: raw)
    }

    static func touchVectorToCanvas(raw: CGPoint) -> CGPoint {
        var dx = raw.x - cx
        var dy = raw.y - cx
        if touchFlipX { dx = -dx }
        if touchFlipY { dy = -dy }
        return CGPoint(x: cx + dx, y: cx + dy)
    }

    // musicA.13 ArrivalAnimation.h
    static let fxOkX: CGFloat = cx - 168 / 2
    static let fxOkY: CGFloat = 372
    static let fxOkW: CGFloat = 168
    static let fxOkH: CGFloat = 52
    static let fxOkPad: CGFloat = 28

    static func fxOkHit(canvas: CGPoint) -> Bool {
        canvas.x >= fxOkX - fxOkPad && canvas.x <= fxOkX + fxOkW + fxOkPad
            && canvas.y >= fxOkY - fxOkPad && canvas.y <= fxOkY + fxOkH + fxOkPad
    }
}
