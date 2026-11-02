import SwiftUI
import SceneKit

/// 连接圆盘：喷砂阳极氧化铝合金（对齐微雪 1.75 实物）+ R0.15 圆角 + 侧键/开孔 + 根节点软椭圆影。
///
/// 0.15.37 根因：`FrostedSilver`/`BrushedMetal` 对角条纹贴图 × torus UV 重复 → 编织/麻花边框。
/// 0.15.38：纯色 + 各向同性微颗粒（无方向刷纹），双胶囊侧键贴近实物。
/// 0.16.2.4：塑料感根因不在几何而在材质——`metalness 0.68` 意味着混了 32% 漫反射（塑料特征），
/// 再叠加 6 面均匀灰立方体环境，金属反射没有对比就只会读成灰块。这次改为：
/// metalness 1.0 / roughness 0.42（喷砂哑光）+ 大面积柔光板环境 + 随机颗粒法线，
/// 圆角统一到 R0.15 并用 torus 与圆柱/端面相切，外沿连续不再有直角高光断线。
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
        // 半厚。必须 > 2×圆角，否则上下圆角自交
        private let halfThickness: CGFloat = 0.165
        private let fillet: CGFloat = 0.15
        private var faceRadius: CGFloat { puckRadius - fillet }

        func buildScene() -> SCNScene {
            let scene = SCNScene()
            scene.background.contents = UIColor.clear

            // 喷砂金属的反射 100% 来自环境；环境里必须是「大面积柔光板」，细灯条会在砂面拉出条状高光
            scene.lightingEnvironment.contents = Self.studioEnvironment()
            scene.lightingEnvironment.intensity = 1.15

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

        /// 喷砂阳极氧化银：metalness 拉满 + 较高粗糙度 + 随机颗粒法线。
        /// 金属色只靠 diffuse 定调（冷灰，比原亮银压暗一档），不再是「亮白 + 半金属」的塑料配方。
        private func beadBlastedAluminum() -> SCNMaterial {
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            alum.diffuse.contents = UIColor(red: 0.60, green: 0.63, blue: 0.68, alpha: 1)
            alum.metalness.contents = NSNumber(value: 1.0)
            alum.roughness.contents = NSNumber(value: 0.42)
            alum.fresnelExponent = 3.0
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

            let screen = SCNPlane(width: 1.66, height: 1.66)
            screen.cornerRadius = 0.83
            let face = SCNMaterial()
            face.lightingModel = .constant
            face.diffuse.contents = UIImage(named: "ConnectPuckScreen") ?? UIColor.darkGray
            face.isDoubleSided = true
            screen.materials = [face]
            let screenNode = SCNNode(geometry: screen)
            screenNode.position = SCNVector3(0, 0, Float(T) + 0.006)
            g.addChildNode(screenNode)

            // 玻璃盖板：constant 屏自身不反光，补一层真实环境反射（喷砂机身边 + 亮面屏，对比才像实物）
            let cover = SCNPlane(width: 1.66, height: 1.66)
            cover.cornerRadius = 0.83
            let coverMat = SCNMaterial()
            coverMat.lightingModel = .physicallyBased
            coverMat.diffuse.contents = UIColor(white: 0.02, alpha: 1)
            coverMat.metalness.contents = NSNumber(value: 0.0)
            coverMat.roughness.contents = NSNumber(value: 0.05)
            coverMat.transparency = 0.20
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

        /// 实物：上胶囊键 + 5 孔 + 下胶囊键（非圆球）。
        private func addSideControls(to g: SCNNode, radius: CGFloat, material: SCNMaterial) {
            // 键要贴边略凸；孔必须落在壳面内侧，否则黑球会读成「凸起的黑点」
            let keyX = Float(radius) - 0.004
            let holeX = Float(radius) - 0.022

            let topBtn = SCNCapsule(capRadius: 0.036, height: 0.16)
            topBtn.materials = [material]
            let topNode = SCNNode(geometry: topBtn)
            topNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            topNode.position = SCNVector3(keyX, 0.32, 0.03)
            g.addChildNode(topNode)

            let holeMat = SCNMaterial()
            holeMat.lightingModel = .constant
            holeMat.diffuse.contents = UIColor(white: 0.07, alpha: 1)
            for i in 0..<5 {
                let hole = SCNSphere(radius: 0.022)
                hole.segmentCount = 14
                hole.materials = [holeMat]
                let hn = SCNNode(geometry: hole)
                let y = 0.10 - Float(i) * 0.048
                hn.position = SCNVector3(holeX, y, 0.02)
                g.addChildNode(hn)
            }

            let botBtn = SCNCapsule(capRadius: 0.032, height: 0.12)
            botBtn.materials = [material]
            let botNode = SCNNode(geometry: botBtn)
            botNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            botNode.position = SCNVector3(keyX, -0.26, 0.025)
            g.addChildNode(botNode)
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
                    UIColor(red: 0.22, green: 0.24, blue: 0.27, alpha: 1).cgColor,
                    UIColor(red: 0.10, green: 0.11, blue: 0.13, alpha: 1).cgColor,
                    UIColor(red: 0.03, green: 0.035, blue: 0.04, alpha: 1).cgColor
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
