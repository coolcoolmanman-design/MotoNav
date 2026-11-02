import SwiftUI
import SceneKit

/// 连接圆盘：喷砂阳极氧化铝合金（对齐微雪 1.75 实物）+ 壁面内嵌侧键/沉孔 + 根节点软椭圆影。
///
/// 0.15.37 根因：`FrostedSilver`/`BrushedMetal` 对角条纹贴图 × torus UV 重复 → 编织/麻花边框。
/// 0.15.38：纯色 + 各向同性微颗粒（无方向刷纹），双胶囊侧键贴近实物。
/// 0.16.2.4：metalness 1.0 / roughness 0.42 + 大面积柔光板环境 + 随机颗粒法线。
/// 0.16.2.7（v4 效果图定稿参数，Three.js 预览验证后移植）：
/// - 半厚 0.165→0.19、圆角 0.15→0.095（对照实物照片的厚度与圆润过渡）；
/// - 侧键改为**壁面内嵌**：暗底槽 + 银色胶囊沿轮缘切向贴壁（上长下短，方位角 ±0.7~0.8rad），
///   不再是悬在端面外的竖向凸钉；
/// - 5 麦克孔改为沉孔圆柱端面齐平壁面（修穿模）；
/// - 材质对齐 v4：diffuse #e0e4ea / metalness 0.78 / roughness 0.44；
/// - 屏幕贴图重绘为实物 UI：转向徽章 / 200m·即将左转 / 白芯蓝路线+导航箭头 / 28km/h 白圆。
struct ConnectPuckSceneView: UIViewRepresentable {
    var spinning: Bool
    var spinSeconds: TimeInterval

    func makeUIView(context: Context) -> SCNView {
        let view = SCNView()
        view.backgroundColor = .clear
        view.antialiasingMode = .multisampling4X
        view.autoenablesDefaultLighting = false
        view.allowsCameraControl = false
        view.isPlaying = true
        view.scene = context.coordinator.buildScene()
        context.coordinator.scnView = view
        return view
    }

    func updateUIView(_ uiView: SCNView, context: Context) {
        context.coordinator.setSpinning(spinning, duration: spinSeconds)
    }

    func makeCoordinator() -> Coordinator { Coordinator() }

    final class Coordinator {
        weak var scnView: SCNView?
        private var spinRoot: SCNNode?
        private var spinning = false

        // 外径半径（圆角过渡后最大半径仍是它）
        private let puckRadius: CGFloat = 1.0
        // 半厚（v4：对照实物增厚）。必须 > 2×圆角，否则上下圆角自交
        private let halfThickness: CGFloat = 0.19
        private let fillet: CGFloat = 0.095
        private var faceRadius: CGFloat { puckRadius - fillet }

        func buildScene() -> SCNScene {
            let scene = SCNScene()
            scene.background.contents = UIColor.clear

            // 喷砂金属的反射 100% 来自环境；环境里必须是「大面积柔光板」，细灯条会在砂面拉出条状高光
            scene.lightingEnvironment.contents = Self.studioEnvironment()
            scene.lightingEnvironment.intensity = 1.35

            let cam = SCNNode()
            cam.camera = SCNCamera()
            // 略广角 + 偏正视：对齐连接弹层三分之四正面（非极端侧视）
            cam.camera?.fieldOfView = 32
            cam.camera?.wantsHDR = true
            cam.camera?.wantsExposureAdaptation = false
            cam.camera?.bloomIntensity = 0.06
            cam.camera?.bloomThreshold = 0.94
            cam.position = SCNVector3(0.15, 0.35, 4.1)
            cam.look(at: SCNVector3(0, -0.02, 0))
            scene.rootNode.addChildNode(cam)

            // 有 IBL 后直射灯只做塑形，整体压暗，否则过曝成灰白
            let ambient = SCNNode()
            ambient.light = SCNLight()
            ambient.light?.type = .ambient
            ambient.light?.intensity = 95
            ambient.light?.color = UIColor(red: 0.76, green: 0.78, blue: 0.82, alpha: 1)
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 500
            key.light?.castsShadow = false
            key.light?.color = UIColor(red: 0.96, green: 0.97, blue: 0.99, alpha: 1)
            key.eulerAngles = SCNVector3(-0.95, 0.55, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 200
            fill.light?.color = UIColor(red: 0.74, green: 0.77, blue: 0.82, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.2, -1.15, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 300
            rim.light?.color = UIColor(red: 0.86, green: 0.89, blue: 0.94, alpha: 1)
            rim.eulerAngles = SCNVector3(-0.25, 2.55, 0)
            scene.rootNode.addChildNode(rim)

            // 地面影：挂在 scene.root，永不随 puck tilt/yaw
            scene.rootNode.addChildNode(makeGroundShadow())

            let spinRoot = SCNNode()
            spinRoot.position = SCNVector3(0, 0.02, 0)
            scene.rootNode.addChildNode(spinRoot)
            self.spinRoot = spinRoot

            let puck = makePuck()
            // 连接弹层同款三分之四正面：屏朝向观众，右侧见键/开孔
            puck.eulerAngles = SCNVector3(-0.22, 0.38, 0)
            spinRoot.addChildNode(puck)

            return scene
        }

        /// 喷砂阳极氧化银（v4 定稿：diffuse #e0e4ea / metalness 0.78 / roughness 0.44）。
        /// 0.78 意味着保留 22% 漫反射参与塑形——v4 预览验证过：纯 1.0 在柔光环境下容易读成灰塑料。
        private func beadBlastedAluminum() -> SCNMaterial {
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            alum.diffuse.contents = UIColor(red: 0.878, green: 0.894, blue: 0.918, alpha: 1)
            alum.metalness.contents = NSNumber(value: 0.78)
            alum.roughness.contents = NSNumber(value: 0.44)
            alum.fresnelExponent = 2.0
            if let grain = Self.beadBlastNormal() {
                alum.normal.contents = grain
                alum.normal.intensity = 0.65
                alum.normal.wrapS = .repeat
                alum.normal.wrapT = .repeat
                // 小贴图平铺多次，砂粒才够细
                alum.normal.contentsTransform = SCNMatrix4MakeScale(7, 7, 1)
            }
            return alum
        }

        private func makePuck() -> SCNNode {
            let g = SCNNode()
            let R = puckRadius
            let T = halfThickness
            let r = fillet
            let faceR = faceRadius
            let alum = beadBlastedAluminum()

            // 侧面直段：只保留上下圆角之间的那一段
            let body = SCNCylinder(radius: R, height: max(0.01, (T - r) * 2))
            body.radialSegmentCount = 160
            body.heightSegmentCount = 4
            body.materials = [alum]
            let bodyNode = SCNNode(geometry: body)
            bodyNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(bodyNode)

            // 上下 R0.15 圆角环：与侧面、端面都相切，外沿因此连续圆润（没有直角高光断线）
            for sign: Float in [-1, 1] {
                let torus = SCNTorus(ringRadius: faceR, pipeRadius: r)
                torus.ringSegmentCount = 128
                torus.pipeSegmentCount = 36
                torus.materials = [alum]
                let rim = SCNNode(geometry: torus)
                rim.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
                rim.position = SCNVector3(0, 0, sign * Float(T - r))
                g.addChildNode(rim)
            }

            // 端面封盘：圆角内圈直径处，避免从缝隙看进空腔
            for sign: Float in [-1, 1] {
                let cap = SCNCylinder(radius: faceR + 0.03, height: 0.02)
                cap.radialSegmentCount = 128
                cap.materials = [alum]
                let capNode = SCNNode(geometry: cap)
                capNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
                capNode.position = SCNVector3(0, 0, sign * Float(T - 0.012))
                g.addChildNode(capNode)
            }

            // 正面黑边 + 屏贴图：用圆角平面朝 +Z，避免圆柱端面 UV 扭向
            let bezelSide = (faceR - 0.005) * 2
            let bezel = SCNPlane(width: bezelSide, height: bezelSide)
            bezel.cornerRadius = faceR - 0.005
            let bezelMat = SCNMaterial()
            bezelMat.diffuse.contents = UIColor(white: 0.04, alpha: 1)
            bezelMat.lightingModel = .constant
            bezelMat.isDoubleSided = true
            bezel.materials = [bezelMat]
            let bezelNode = SCNNode(geometry: bezel)
            bezelNode.position = SCNVector3(0, 0, Float(T) + 0.002)
            g.addChildNode(bezelNode)

            let screen = SCNPlane(width: 1.72, height: 1.72)
            screen.cornerRadius = 0.86
            let face = SCNMaterial()
            face.lightingModel = .constant
            face.diffuse.contents = UIImage(named: "ConnectPuckScreen") ?? Self.navigationScreenTexture()
            face.isDoubleSided = true
            screen.materials = [face]
            let screenNode = SCNNode(geometry: screen)
            screenNode.position = SCNVector3(0, 0, Float(T) + 0.006)
            g.addChildNode(screenNode)

            // 玻璃盖板：constant 屏自身不反光，补一层真实环境反射（喷砂机身边 + 亮面屏，对比才像实物）
            let cover = SCNPlane(width: 1.72, height: 1.72)
            cover.cornerRadius = 0.86
            let coverMat = SCNMaterial()
            coverMat.lightingModel = .physicallyBased
            coverMat.diffuse.contents = UIColor(white: 0.02, alpha: 1)
            coverMat.metalness.contents = NSNumber(value: 0.0)
            coverMat.roughness.contents = NSNumber(value: 0.05)
            coverMat.transparency = 0.06
            coverMat.isDoubleSided = false
            cover.materials = [coverMat]
            let coverNode = SCNNode(geometry: cover)
            coverNode.position = SCNVector3(0, 0, Float(T) + 0.008)
            g.addChildNode(coverNode)

            addSideControls(to: g, radius: R, material: alum)

            g.scale = SCNVector3(0.78, 0.78, 0.78)
            return g
        }

        /// 软椭圆接触影：固定在世界水平面，独立于 spinRoot。
        private func makeGroundShadow() -> SCNNode {
            let shadow = SCNPlane(width: 2.8, height: 1.15)
            let shadowMat = SCNMaterial()
            if let img = UIImage(named: "ConnectGroundShadow") {
                shadowMat.diffuse.contents = img
            } else {
                shadowMat.diffuse.contents = Self.proceduralEllipseShadow()
            }
            shadowMat.lightingModel = .constant
            shadowMat.writesToDepthBuffer = false
            shadowMat.readsFromDepthBuffer = false
            shadowMat.blendMode = .alpha
            shadowMat.isDoubleSided = true
            shadowMat.transparencyMode = .aOne
            shadow.materials = [shadowMat]

            let node = SCNNode(geometry: shadow)
            node.eulerAngles = SCNVector3(-Float.pi / 2, 0, 0)
            node.position = SCNVector3(0, -0.92, 0.08)
            node.opacity = 0.88
            return node
        }

        /// v4 壁面内嵌布局（与 Three.js 预览逐参数对应）：
        /// 键与孔都嵌在圆柱壁面上，方位角 α 从 +X 轴量起（本地系：屏朝 +Z、圆柱轴 = Z）。
        /// 径向 r̂=(cosα,sinα,0)、切向 t̂=(−sinα,cosα,0)；eulerAngles.z=α 恰好把
        /// 节点本地 Y 轴旋到 t̂、本地 X 轴旋到 r̂ —— 于是 scale.x 压扁的就是径向（键贴壁）。
        private func addSideControls(to g: SCNNode, radius: CGFloat, material: SCNMaterial) {
            let slotMat = SCNMaterial()
            slotMat.lightingModel = .physicallyBased
            slotMat.diffuse.contents = UIColor(red: 0.188, green: 0.208, blue: 0.235, alpha: 1) // #30353c
            slotMat.metalness.contents = NSNumber(value: 0.35)
            slotMat.roughness.contents = NSNumber(value: 0.70)

            let holeMat = SCNMaterial()
            holeMat.lightingModel = .physicallyBased
            holeMat.diffuse.contents = UIColor(red: 0.063, green: 0.075, blue: 0.094, alpha: 1) // #101318
            holeMat.metalness.contents = NSNumber(value: 0.20)
            holeMat.roughness.contents = NSNumber(value: 0.85)

            /// 内嵌键：暗底槽（大而扁）+ 银色胶囊（小一圈、内收 0.006），均沿切向贴壁。
            func wallKey(alpha: Float, slotLen: CGFloat, silverLen: CGFloat) {
                func embed(_ geo: SCNGeometry, radialInset: CGFloat, flatten: CGFloat) -> SCNNode {
                    let node = SCNNode(geometry: geo)
                    node.eulerAngles = SCNVector3(0, 0, alpha)
                    node.scale = SCNVector3(flatten, 1, 1)
                    node.position = SCNVector3(radius * cosf(alpha) - radialInset * cosf(alpha),
                                               radius * sinf(alpha) - radialInset * sinf(alpha), 0)
                    g.addChildNode(node)
                    return node
                }
                // 暗底槽：半径 0.060、径向压到 0.35（≡ v4 CapsuleGeometry + scale.x 0.35）
                let slot = SCNCapsule(capRadius: 0.060, height: slotLen)
                slot.radialSegmentCount = 24
                slot.materials = [slotMat]
                embed(slot, radialInset: 0.012, flatten: 0.35)
                // 银色键芯：半径 0.045、压 0.9，内收 0.006 露出暗缝
                let cap = SCNCapsule(capRadius: 0.045, height: silverLen)
                cap.radialSegmentCount = 24
                cap.materials = [material]
                embed(cap, radialInset: 0.006, flatten: 0.90)
            }

            // 注意方位角取反：HTML 预览经 rotation.x=π/2 映射后 y 翻转，SceneKit 本地系（屏朝 +Z、Y 向上）
            // 直接用 −α_HTML —— 长键在右上 (+0.82)、短键在右下 (−0.68)、孔列在两键之间 (+0.07)。
            wallKey(alpha: 0.82, slotLen: 0.34, silverLen: 0.30)  // 上长键
            wallKey(alpha: -0.68, slotLen: 0.18, silverLen: 0.14) // 下短键

            // 5 麦克沉孔：圆柱轴沿径向、外端面与壁面齐平（中心在 R−0.023，半高 0.025 → 端面 R+0.002）
            let holeAlpha: Float = 0.07
            let holeAxis = SCNVector3(0, 0, holeAlpha - Float.pi / 2) // 本地 Y → 径向
            for offset in [-0.15, -0.075, 0.0, 0.075, 0.15] {
                let hole = SCNCylinder(radius: 0.015, height: 0.05)
                hole.radialSegmentCount = 16
                hole.materials = [holeMat]
                let hn = SCNNode(geometry: hole)
                hn.eulerAngles = holeAxis
                let rr: Float = Float(radius) - 0.023
                let a = holeAlpha
                let tang = Float(offset)
                hn.position = SCNVector3(rr * cosf(a) - tang * sinf(a),
                                         rr * sinf(a) + tang * cosf(a), 0)
                g.addChildNode(hn)
            }
        }

        func setSpinning(_ on: Bool, duration: TimeInterval) {
            guard let spinRoot else { return }
            if on {
                guard !spinning else { return }
                spinning = true
                spinRoot.removeAllActions()
                let spin = SCNAction.rotateBy(x: 0, y: CGFloat.pi * 2, z: 0, duration: duration)
                spin.timingMode = .easeInEaseOut
                spinRoot.runAction(spin) { [weak self] in
                    self?.spinning = false
                }
            } else if spinning {
                // 自然结束
            } else {
                let yaw = CGFloat(spinRoot.eulerAngles.y)
                let settle = SCNAction.rotateTo(x: 0, y: yaw, z: 0, duration: 0.45)
                settle.timingMode = .easeOut
                spinRoot.runAction(settle)
            }
        }

        // MARK: - Helpers

        /// 喷砂颗粒法线：随机高度场 → 盒式平滑（砂粒尺寸）→ 梯度转法线。
        /// 没有它，roughness 只会把环境糊成一片，缺「砂面」该有的细碎起伏。
        private static func beadBlastNormal(size: Int = 256) -> UIImage? {
            var heights = [Float](repeating: 0, count: size * size)
            var seed: UInt64 = 0x9E37_79B9_7F4A_7C15
            for i in 0..<(size * size) {
                seed = seed &* 6_364_136_223_846_793_005 &+ 1_442_695_040_888_963_407
                heights[i] = Float((seed >> 33) & 0xFF_FFFF) / Float(0xFF_FFFF)
            }

            var tmp = heights
            for _ in 0..<2 {
                for y in 0..<size {
                    for x in 0..<size {
                        var sum: Float = 0
                        for dy in -1...1 {
                            let yy = (y + dy + size) % size
                            for dx in -1...1 {
                                let xx = (x + dx + size) % size
                                sum += tmp[yy * size + xx]
                            }
                        }
                        heights[y * size + x] = sum / 9
                    }
                }
                tmp = heights
            }

            var pixels = [UInt8](repeating: 255, count: size * size * 4)
            let strength: Float = 3.0
            for y in 0..<size {
                for x in 0..<size {
                    let xm = (x - 1 + size) % size
                    let xp = (x + 1) % size
                    let ym = (y - 1 + size) % size
                    let yp = (y + 1) % size
                    let dx = (heights[y * size + xp] - heights[y * size + xm]) * strength
                    let dy = (heights[yp * size + x] - heights[ym * size + x]) * strength
                    let len = sqrt(dx * dx + dy * dy + 1)
                    let i = (y * size + x) * 4
                    pixels[i + 0] = UInt8(clamping: Int(((-dx / len) * 0.5 + 0.5) * 255))
                    pixels[i + 1] = UInt8(clamping: Int(((-dy / len) * 0.5 + 0.5) * 255))
                    pixels[i + 2] = UInt8(clamping: Int(((1 / len) * 0.5 + 0.5) * 255))
                    pixels[i + 3] = 255
                }
            }

            guard let provider = CGDataProvider(data: Data(pixels) as CFData),
                  let cgImage = CGImage(
                    width: size,
                    height: size,
                    bitsPerComponent: 8,
                    bitsPerPixel: 32,
                    bytesPerRow: size * 4,
                    space: CGColorSpaceCreateDeviceRGB(),
                    bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                    provider: provider,
                    decode: nil,
                    shouldInterpolate: true,
                    intent: .defaultIntent
                  ) else { return nil }
            return UIImage(cgImage: cgImage)
        }

        /// 棚拍环境：暗背景 + 大面积柔光板（喷砂要面光源，不能是细灯条）。
        private static func studioEnvironment() -> UIImage {
            let width: CGFloat = 1024
            let height: CGFloat = 512
            let format = UIGraphicsImageRendererFormat.default()
            format.scale = 1
            format.opaque = true
            return UIGraphicsImageRenderer(size: CGSize(width: width, height: height), format: format).image { ctx in
                let cg = ctx.cgContext
                let space = CGColorSpaceCreateDeviceRGB()

                let background = [
                    UIColor(red: 0.55, green: 0.57, blue: 0.60, alpha: 1).cgColor,
                    UIColor(red: 0.38, green: 0.40, blue: 0.43, alpha: 1).cgColor,
                    UIColor(red: 0.18, green: 0.19, blue: 0.21, alpha: 1).cgColor
                ] as CFArray
                if let grad = CGGradient(colorsSpace: space, colors: background, locations: [0, 0.5, 1]) {
                    cg.drawLinearGradient(grad, start: .zero, end: CGPoint(x: 0, y: height), options: [])
                }

                func softbox(_ center: CGPoint, rx: CGFloat, ry: CGFloat, alpha: CGFloat) {
                    let colors = [UIColor(white: 1, alpha: alpha).cgColor,
                                  UIColor(white: 1, alpha: 0).cgColor] as CFArray
                    guard let grad = CGGradient(colorsSpace: space, colors: colors, locations: [0, 1]) else { return }
                    cg.saveGState()
                    cg.translateBy(x: center.x, y: center.y)
                    cg.scaleBy(x: 1, y: ry / rx)
                    cg.drawRadialGradient(grad, startCenter: .zero, startRadius: 0,
                                          endCenter: .zero, endRadius: rx, options: [])
                    cg.restoreGState()
                }

                softbox(CGPoint(x: width * 0.50, y: height * 0.12), rx: width * 0.46, ry: height * 0.15, alpha: 1.00)
                softbox(CGPoint(x: width * 0.03, y: height * 0.40), rx: width * 0.16, ry: height * 0.34, alpha: 0.55)
                softbox(CGPoint(x: width * 0.97, y: height * 0.36), rx: width * 0.14, ry: height * 0.30, alpha: 0.40)
                softbox(CGPoint(x: width * 0.50, y: height * 0.84), rx: width * 0.42, ry: height * 0.10, alpha: 0.16)
            }
        }

        /// 内置导航屏纹理（0.16.2.7 按 v4 效果图/实物照片重绘，坐标即 1024² Canvas 原值）：
        /// 深色地图网格 + 白芯蓝路线（底部起弯向上）+ 白色导航箭头 + 顶部转向徽章
        /// + 居中 200 m·即将左转 + 左下白色 28 km/h 圆。工程没有 ConnectPuckScreen 图片资源，
        /// 不能再回退成一块黑/灰屏；连接动画旋转到正面时始终有内容。
        private static func navigationScreenTexture() -> UIImage {
            let side: CGFloat = 1024
            let format = UIGraphicsImageRendererFormat.default()
            format.scale = 1
            format.opaque = true
            return UIGraphicsImageRenderer(size: CGSize(width: side, height: side), format: format).image { ctx in
                let cg = ctx.cgContext
                cg.setLineCap(.round)

                // 底色 #0d1119
                UIColor(red: 0.051, green: 0.067, blue: 0.098, alpha: 1).setFill()
                cg.fill(CGRect(x: 0, y: 0, width: side, height: side))

                // 横向街线 #1c2534
                cg.setStrokeColor(UIColor(red: 0.110, green: 0.145, blue: 0.204, alpha: 1).cgColor)
                cg.setLineWidth(8)
                for l in [(60, 270, 980, 225), (40, 470, 990, 450), (55, 700, 980, 685), (80, 915, 960, 905)] {
                    cg.move(to: CGPoint(x: CGFloat(l.0), y: CGFloat(l.1)))
                    cg.addLine(to: CGPoint(x: CGFloat(l.2), y: CGFloat(l.3)))
                }
                cg.strokePath()

                // 纵向街线 #28324a
                cg.setStrokeColor(UIColor(red: 0.157, green: 0.196, blue: 0.290, alpha: 1).cgColor)
                cg.setLineWidth(6)
                for l in [(290, 80, 330, 980), (560, 70, 530, 990), (810, 110, 850, 940)] {
                    cg.move(to: CGPoint(x: CGFloat(l.0), y: CGFloat(l.1)))
                    cg.addLine(to: CGPoint(x: CGFloat(l.2), y: CGFloat(l.3)))
                }
                cg.strokePath()

                // 蓝路线（白芯）：底部 (430,1040) 弯向上至 (505,748)
                func routePath() -> UIBezierPath {
                    let p = UIBezierPath()
                    p.move(to: CGPoint(x: 430, y: 1040))
                    p.addCurve(to: CGPoint(x: 512, y: 792),
                               controlPoint1: CGPoint(x: 452, y: 940),
                               controlPoint2: CGPoint(x: 508, y: 866))
                    p.addCurve(to: CGPoint(x: 505, y: 748),
                               controlPoint1: CGPoint(x: 514, y: 766),
                               controlPoint2: CGPoint(x: 512, y: 754))
                    p.lineCapStyle = .round
                    return p
                }
                UIColor(red: 0.184, green: 0.435, blue: 0.878, alpha: 1).setStroke() // #2f6fe0
                routePath().lineWidth = 36
                routePath().stroke()
                UIColor(red: 0.949, green: 0.961, blue: 0.976, alpha: 1).setStroke() // #f2f5f9
                routePath().lineWidth = 13
                routePath().stroke()

                // 白色导航箭头（路线终点上方，微倾 −0.16rad）
                cg.saveGState()
                cg.translateBy(x: 503, y: 706)
                cg.rotate(by: -0.16)
                UIColor.white.setFill()
                let arrow = UIBezierPath()
                arrow.move(to: CGPoint(x: 0, y: -104))
                arrow.addLine(to: CGPoint(x: 74, y: 62))
                arrow.addLine(to: CGPoint(x: 0, y: 25))
                arrow.addLine(to: CGPoint(x: -74, y: 62))
                arrow.close()
                arrow.fill()
                cg.restoreGState()

                func centered(_ text: String, baselineY: CGFloat, size: CGFloat, weight: UIFont.Weight, color: UIColor) {
                    let attrs: [NSAttributedString.Key: Any] = [
                        .font: UIFont.systemFont(ofSize: size, weight: weight),
                        .foregroundColor: color
                    ]
                    let str = NSAttributedString(string: text, attributes: attrs)
                    let bounds = str.boundingRect(with: CGSize(width: side, height: 220), options: [.usesLineFragmentOrigin], context: nil)
                    // Canvas fillText 的 y 是基线；draw(at:) 是 top-left，按 ascent≈0.76em 换算
                    str.draw(at: CGPoint(x: (side - bounds.width) / 2, y: baselineY - size * 0.76))
                }

                // 顶部转向徽章：(512,268) r92，暗底 + 灰描边 + 白色左转箭头
                let badgeRect = CGRect(x: 420, y: 176, width: 184, height: 184)
                UIColor(red: 0.063, green: 0.086, blue: 0.122, alpha: 0.95).setFill()
                UIBezierPath(ovalIn: badgeRect).fill()
                UIColor(red: 0.224, green: 0.271, blue: 0.353, alpha: 1).setStroke() // #39455a
                let ring = UIBezierPath(ovalIn: badgeRect)
                ring.lineWidth = 4
                ring.stroke()
                cg.setStrokeColor(UIColor.white.cgColor)
                cg.setLineWidth(15)
                cg.move(to: CGPoint(x: 551, y: 322))
                cg.addLine(to: CGPoint(x: 551, y: 280))
                cg.addQuadCurve(to: CGPoint(x: 528, y: 258), controlPoint: CGPoint(x: 551, y: 258))
                cg.addLine(to: CGPoint(x: 470, y: 258))
                cg.strokePath()
                cg.move(to: CGPoint(x: 499, y: 228))
                cg.addLine(to: CGPoint(x: 463, y: 258))
                cg.addLine(to: CGPoint(x: 499, y: 288))
                cg.strokePath()

                centered("200 m", baselineY: 500, size: 118, weight: .bold, color: .white)
                centered("即将左转", baselineY: 572, size: 46, weight: .medium, color: UIColor(red: 0.784, green: 0.816, blue: 0.867, alpha: 1))

                // 左下白圆车速：(340,862) r124
                UIColor(red: 0.957, green: 0.965, blue: 0.976, alpha: 1).setFill()
                UIBezierPath(ovalIn: CGRect(x: 216, y: 738, width: 248, height: 248)).fill()
                centered("28", baselineY: 886, size: 100, weight: .bold, color: UIColor(red: 0.059, green: 0.078, blue: 0.110, alpha: 1))
                centered("km/h", baselineY: 946, size: 36, weight: .medium, color: UIColor(red: 0.400, green: 0.439, blue: 0.494, alpha: 1))
            }
        }

        private static func proceduralEllipseShadow() -> UIImage {
            let s = CGSize(width: 512, height: 256)
            let r = UIGraphicsImageRenderer(size: s)
            return r.image { ctx in
                ctx.cgContext.clear(CGRect(origin: .zero, size: s))
                let rect = CGRect(x: 40, y: 40, width: 432, height: 176)
                ctx.cgContext.setShadow(offset: .zero, blur: 32, color: UIColor.black.withAlphaComponent(0.40).cgColor)
                UIColor.black.withAlphaComponent(0.30).setFill()
                ctx.cgContext.fillEllipse(in: rect.insetBy(dx: 24, dy: 36))
            }
        }
    }
}
