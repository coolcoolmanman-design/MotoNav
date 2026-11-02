import SwiftUI
import SceneKit

/// 重建版连接圆盘：磨砂银 PBR + 强边缘 fillet + 侧格栅/双键 + **根节点**软椭圆地面影。
///
/// 0.15.36 根因：
/// 1) `GroundShadow`/`ConnectGroundShadow` 挂在倾斜/自旋的 puck 子树上 → 地面平面跟着 tilt → 视口里成**平行四边形/方影**；
/// 2) PBR 无真实 IBL cubemap（仅 `UIColor` 环境）+ 偏高 ambient/metalness → 洗成**白塑料**。
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

            // 中性灰「假 cubemap」：用六面同色图像，避免 UIColor 环境被忽略导致塑料塌陷
            let env = Self.neutralEnvironmentCube()
            scene.lightingEnvironment.contents = env
            scene.lightingEnvironment.intensity = 0.85

            let cam = SCNNode()
            cam.camera = SCNCamera()
            cam.camera?.fieldOfView = 28
            cam.camera?.wantsHDR = true
            cam.camera?.wantsExposureAdaptation = false
            cam.position = SCNVector3(0, 0.55, 4.4)
            cam.look(at: SCNVector3(0, -0.05, 0))
            scene.rootNode.addChildNode(cam)

            // 克制环境；靠方向光塑形，避免洗白
            let ambient = SCNNode()
            ambient.light = SCNLight()
            ambient.light?.type = .ambient
            ambient.light?.intensity = 220
            ambient.light?.color = UIColor(red: 0.78, green: 0.80, blue: 0.84, alpha: 1)
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 780
            key.light?.castsShadow = false
            key.light?.color = UIColor(red: 0.96, green: 0.97, blue: 0.99, alpha: 1)
            key.eulerAngles = SCNVector3(-0.95, 0.55, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 280
            fill.light?.color = UIColor(red: 0.75, green: 0.78, blue: 0.84, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.2, -1.15, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 420
            rim.light?.color = UIColor(red: 0.88, green: 0.91, blue: 0.96, alpha: 1)
            rim.eulerAngles = SCNVector3(-0.25, 2.55, 0)
            scene.rootNode.addChildNode(rim)

            // —— 地面影：挂在 scene.root，永不随 puck tilt/yaw ——
            scene.rootNode.addChildNode(makeGroundShadow())

            // —— 可自旋圆盘（仅壳体旋转；阴影不动）——
            let spinRoot = SCNNode()
            spinRoot.position = SCNVector3(0, 0.02, 0)
            scene.rootNode.addChildNode(spinRoot)
            self.spinRoot = spinRoot

            let puck = makePuck()
            // 三分之四视角：略仰，让 fillet / 侧键可见
            puck.eulerAngles = SCNVector3(-0.42, 0.55, 0)
            spinRoot.addChildNode(puck)

            return scene
        }

        /// 磨砂银：中等金属度 + 高粗糙度；依赖颗粒贴图，不用高镜面塑料。
        private func frostedAluminum() -> SCNMaterial {
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            if let metal = UIImage(named: "FrostedSilver") ?? UIImage(named: "BrushedMetal") {
                alum.diffuse.contents = metal
                alum.diffuse.wrapS = .repeat
                alum.diffuse.wrapT = .repeat
                alum.diffuse.contentsTransform = SCNMatrix4MakeScale(2.8, 2.8, 1)
            } else {
                alum.diffuse.contents = UIColor(red: 0.62, green: 0.65, blue: 0.69, alpha: 1)
            }
            // 磨砂银：金属感在，但不靠亮塑料高光
            alum.metalness.contents = NSNumber(value: 0.48)
            alum.roughness.contents = NSNumber(value: 0.78)
            alum.normal.intensity = 0.35
            return alum
        }

        private func makePuck() -> SCNNode {
            let g = SCNNode()
            let R: CGFloat = 1.0
            let halfT: CGFloat = 0.20
            // 强边缘 fillet（贴近 Waveshare 1.75 实物弧边）
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

            // 前后 torus 圆角（粗 pipe → 三视角清晰可见弧边）
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

            // 侧壁外圈：略鼓，保证 fillet 与侧面连续
            let side = SCNCylinder(radius: R - fillet * 0.22, height: max(0.05, halfT * 2 - fillet * 1.5))
            side.radialSegmentCount = 128
            side.materials = [alum]
            let sideNode = SCNNode(geometry: side)
            sideNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(sideNode)

            // 黑边框 + 屏贴
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
                // 兜底：程序椭圆，绝不用方板纯黑
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
            // 水平铺在 XZ；不要挂到 tilt 的 puck 上
            node.eulerAngles = SCNVector3(-Float.pi / 2, 0, 0)
            node.position = SCNVector3(0, -0.92, 0.08)
            node.opacity = 0.92
            return node
        }

        private func addSideControls(to g: SCNNode, radius: CGFloat, material: SCNMaterial) {
            let sideX = Float(radius) * 1.04

            let topBtn = SCNCapsule(capRadius: 0.038, height: 0.15)
            topBtn.materials = [material]
            let topNode = SCNNode(geometry: topBtn)
            topNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            topNode.position = SCNVector3(sideX, 0.30, 0.03)
            g.addChildNode(topNode)

            let holeMat = SCNMaterial()
            holeMat.lightingModel = .constant
            holeMat.diffuse.contents = UIColor(white: 0.07, alpha: 1)
            for i in 0..<5 {
                let hole = SCNSphere(radius: 0.024)
                hole.segmentCount = 14
                hole.materials = [holeMat]
                let hn = SCNNode(geometry: hole)
                let y = 0.10 - Float(i) * 0.050
                hn.position = SCNVector3(sideX + 0.02, y, 0.02)
                g.addChildNode(hn)
            }

            let botBtn = SCNSphere(radius: 0.034)
            botBtn.segmentCount = 18
            botBtn.materials = [material]
            let botNode = SCNNode(geometry: botBtn)
            botNode.position = SCNVector3(sideX, -0.24, 0.025)
            g.addChildNode(botNode)
        }

        func setSpinning(_ on: Bool, duration: TimeInterval) {
            guard let spinRoot else { return }
            if on {
                guard !spinning else { return }
                spinning = true
                spinRoot.removeAllActions()
                // 只绕世界 Y 转壳体；阴影在 root 不动
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

        private static func neutralEnvironmentCube() -> [UIImage] {
            // 六面微渐变灰，给 PBR 一点环境反射而不洗白
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
            // +X -X +Y -Y +Z -Z
            return [
                face(0.72, 0.58), face(0.70, 0.56),
                face(0.82, 0.70), face(0.55, 0.48),
                face(0.74, 0.60), face(0.68, 0.54)
            ]
        }

        private static func proceduralEllipseShadow() -> UIImage {
            let s = CGSize(width: 512, height: 256)
            let r = UIGraphicsImageRenderer(size: s)
            return r.image { ctx in
                ctx.cgContext.clear(CGRect(origin: .zero, size: s))
                let rect = CGRect(x: 40, y: 40, width: 432, height: 176)
                ctx.cgContext.setShadow(offset: .zero, blur: 28, color: UIColor.black.withAlphaComponent(0.45).cgColor)
                UIColor.black.withAlphaComponent(0.35).setFill()
                ctx.cgContext.fillEllipse(in: rect.insetBy(dx: 20, dy: 30))
            }
        }
    }
}
