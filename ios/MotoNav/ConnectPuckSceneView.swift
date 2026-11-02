import SwiftUI
import SceneKit

/// 连接圆盘：光滑磨砂银（对齐微雪 1.75 实物）+ fillet + 侧键/开孔 + 根节点软椭圆影。
///
/// 0.15.37 根因：`FrostedSilver`/`BrushedMetal` 对角条纹贴图 × torus UV 重复 → 编织/麻花边框。
/// 0.15.38：纯色 + 各向同性微颗粒（无方向刷纹），双胶囊侧键贴近实物。
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

        func buildScene() -> SCNScene {
            let scene = SCNScene()
            scene.background.contents = UIColor.clear

            let env = Self.neutralEnvironmentCube()
            scene.lightingEnvironment.contents = env
            scene.lightingEnvironment.intensity = 0.72

            let cam = SCNNode()
            cam.camera = SCNCamera()
            cam.camera?.fieldOfView = 28
            cam.camera?.wantsHDR = true
            cam.camera?.wantsExposureAdaptation = false
            cam.position = SCNVector3(0, 0.55, 4.4)
            cam.look(at: SCNVector3(0, -0.05, 0))
            scene.rootNode.addChildNode(cam)

            let ambient = SCNNode()
            ambient.light = SCNLight()
            ambient.light?.type = .ambient
            ambient.light?.intensity = 180
            ambient.light?.color = UIColor(red: 0.76, green: 0.78, blue: 0.82, alpha: 1)
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 720
            key.light?.castsShadow = false
            key.light?.color = UIColor(red: 0.96, green: 0.97, blue: 0.99, alpha: 1)
            key.eulerAngles = SCNVector3(-0.95, 0.55, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 260
            fill.light?.color = UIColor(red: 0.74, green: 0.77, blue: 0.82, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.2, -1.15, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 380
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
            // 三分之四：露出 fillet / 侧键 / 开孔
            puck.eulerAngles = SCNVector3(-0.42, 0.55, 0)
            spinRoot.addChildNode(puck)

            return scene
        }

        /// 光滑磨砂银：无方向刷纹、无编织贴图；各向同性微颗粒仅作漫反射起伏。
        private func frostedAluminum() -> SCNMaterial {
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            // 优先用程序生成的无方向微颗粒；不再用 FrostedSilver/BrushedMetal（对角条纹→麻花）
            alum.diffuse.contents = Self.smoothFrostedSilverTexture()
            alum.diffuse.wrapS = .clamp
            alum.diffuse.wrapT = .clamp
            alum.metalness.contents = NSNumber(value: 0.55)
            alum.roughness.contents = NSNumber(value: 0.72)
            alum.normal.intensity = 0.0
            return alum
        }

        private func makePuck() -> SCNNode {
            let g = SCNNode()
            let R: CGFloat = 1.0
            let halfT: CGFloat = 0.20
            let fillet: CGFloat = 0.13
            let alum = frostedAluminum()

            let bodyR = R - fillet * 0.65
            let body = SCNCylinder(radius: bodyR, height: max(0.06, halfT * 2 - fillet * 1.35))
            body.radialSegmentCount = 160
            body.heightSegmentCount = 4
            body.materials = [alum]
            let bodyNode = SCNNode(geometry: body)
            bodyNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(bodyNode)

            for sign: Float in [-1, 1] {
                let torus = SCNTorus(ringRadius: bodyR, pipeRadius: fillet)
                torus.ringSegmentCount = 128
                torus.pipeSegmentCount = 36
                torus.materials = [alum]
                let rim = SCNNode(geometry: torus)
                rim.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
                rim.position = SCNVector3(0, 0, sign * Float(halfT - fillet * 0.35))
                g.addChildNode(rim)
            }

            let side = SCNCylinder(radius: R - fillet * 0.22, height: max(0.05, halfT * 2 - fillet * 1.5))
            side.radialSegmentCount = 128
            side.materials = [alum]
            let sideNode = SCNNode(geometry: side)
            sideNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(sideNode)

            let bezel = SCNPlane(width: 1.78, height: 1.78)
            bezel.cornerRadius = 0.89
            let bezelMat = SCNMaterial()
            bezelMat.diffuse.contents = UIColor(white: 0.05, alpha: 1)
            bezelMat.lightingModel = .constant
            bezelMat.isDoubleSided = true
            bezel.materials = [bezelMat]
            let bezelNode = SCNNode(geometry: bezel)
            bezelNode.position = SCNVector3(0, 0, Float(halfT) + 0.002)
            g.addChildNode(bezelNode)

            let screen = SCNPlane(width: 1.62, height: 1.62)
            screen.cornerRadius = 0.81
            let face = SCNMaterial()
            face.lightingModel = .constant
            face.diffuse.contents = UIImage(named: "ConnectPuckScreen") ?? UIColor.darkGray
            face.isDoubleSided = true
            screen.materials = [face]
            let screenNode = SCNNode(geometry: screen)
            screenNode.position = SCNVector3(0, 0, Float(halfT) + 0.006)
            g.addChildNode(screenNode)

            addSideControls(to: g, radius: R - fillet * 0.15, material: alum)

            g.scale = SCNVector3(0.72, 0.72, 0.72)
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
            let sideX = Float(radius) * 1.04

            let topBtn = SCNCapsule(capRadius: 0.036, height: 0.16)
            topBtn.materials = [material]
            let topNode = SCNNode(geometry: topBtn)
            topNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            topNode.position = SCNVector3(sideX, 0.32, 0.03)
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
                hn.position = SCNVector3(sideX + 0.018, y, 0.02)
                g.addChildNode(hn)
            }

            let botBtn = SCNCapsule(capRadius: 0.032, height: 0.12)
            botBtn.materials = [material]
            let botNode = SCNNode(geometry: botBtn)
            botNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            botNode.position = SCNVector3(sideX, -0.26, 0.025)
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

        /// 各向同性微颗粒银：无对角条纹，避免 torus UV 显编织。
        private static func smoothFrostedSilverTexture() -> UIImage {
            let size = CGSize(width: 256, height: 256)
            let renderer = UIGraphicsImageRenderer(size: size)
            return renderer.image { ctx in
                let base = UIColor(red: 0.58, green: 0.61, blue: 0.65, alpha: 1)
                base.setFill()
                ctx.fill(CGRect(origin: .zero, size: size))

                // 极轻各向同性噪点（非刷纹）
                for _ in 0..<4200 {
                    let x = CGFloat.random(in: 0..<size.width)
                    let y = CGFloat.random(in: 0..<size.height)
                    let d = CGFloat.random(in: -0.035...0.035)
                    let c = UIColor(
                        red: max(0, min(1, 0.58 + d)),
                        green: max(0, min(1, 0.61 + d)),
                        blue: max(0, min(1, 0.65 + d * 0.8)),
                        alpha: 0.55
                    )
                    c.setFill()
                    ctx.cgContext.fillEllipse(in: CGRect(x: x, y: y, width: 1.2, height: 1.2))
                }
            }
        }

        private static func neutralEnvironmentCube() -> [UIImage] {
            func face(_ top: CGFloat, _ bot: CGFloat) -> UIImage {
                let s = CGSize(width: 64, height: 64)
                let r = UIGraphicsImageRenderer(size: s)
                return r.image { ctx in
                    let colors = [
                        UIColor(white: top, alpha: 1).cgColor,
                        UIColor(white: bot, alpha: 1).cgColor
                    ]
                    let space = CGColorSpaceCreateDeviceRGB()
                    if let g = CGGradient(colorsSpace: space, colors: colors as CFArray, locations: [0, 1]) {
                        ctx.cgContext.drawLinearGradient(
                            g,
                            start: CGPoint(x: 32, y: 0),
                            end: CGPoint(x: 32, y: 64),
                            options: []
                        )
                    }
                }
            }
            return [
                face(0.70, 0.56), face(0.68, 0.54),
                face(0.78, 0.66), face(0.52, 0.46),
                face(0.72, 0.58), face(0.66, 0.52)
            ]
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
