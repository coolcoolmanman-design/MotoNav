import SwiftUI
import SceneKit

/// SceneKit 磨砂银圆盘：侧扬声器孔 + 实体键 + 实物级边缘圆角 + 软椭圆地面阴影（无中心圆环伪影）。
struct ConnectPuckSceneView: UIViewRepresentable {
    var spinning: Bool
    var spinSeconds: TimeInterval

    func makeUIView(context: Context) -> SCNView {
        let view = SCNView()
        view.backgroundColor = .clear
        view.antialiasingMode = .multisampling4X
        view.autoenablesDefaultLighting = false
        view.allowsCameraControl = false
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
        private var puck: SCNNode?
        private var spinning = false

        func buildScene() -> SCNScene {
            let scene = SCNScene()
            scene.background.contents = UIColor.clear
            // PBR 金属需要环境光；无 IBL 时高金属度会塌成灰塑
            scene.lightingEnvironment.contents = UIColor(red: 0.82, green: 0.84, blue: 0.87, alpha: 1)
            scene.lightingEnvironment.intensity = 1.15

            let root = SCNNode()
            scene.rootNode.addChildNode(root)

            let puck = makePuck()
            puck.eulerAngles = SCNVector3(-0.12, 0, 0)
            root.addChildNode(puck)
            self.puck = puck

            let cam = SCNNode()
            cam.camera = SCNCamera()
            cam.camera?.fieldOfView = 26
            cam.camera?.wantsHDR = true
            cam.position = SCNVector3(0, 0.04, 4.7)
            scene.rootNode.addChildNode(cam)

            // 磨砂银：偏环境填充 + 冷中性方向光，避免洗白/灰塑
            let ambient = SCNNode()
            ambient.light = SCNLight()
            ambient.light?.type = .ambient
            ambient.light?.intensity = 620
            ambient.light?.color = UIColor(white: 0.96, alpha: 1)
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 700
            key.light?.castsShadow = false
            key.light?.color = UIColor(red: 0.95, green: 0.96, blue: 0.98, alpha: 1)
            key.eulerAngles = SCNVector3(-0.85, 0.45, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 420
            fill.light?.color = UIColor(red: 0.88, green: 0.90, blue: 0.93, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.15, -1.0, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 320
            rim.light?.color = UIColor(red: 0.92, green: 0.94, blue: 0.97, alpha: 1)
            rim.eulerAngles = SCNVector3(-0.4, 2.4, 0)
            scene.rootNode.addChildNode(rim)

            return scene
        }

        private func frostedAluminum() -> SCNMaterial {
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            // 磨砂银：优先有颗粒的 BrushedMetal；禁 multiply 压平 PBR
            if let metal = UIImage(named: "BrushedMetal") ?? UIImage(named: "FrostedSilver") {
                alum.diffuse.contents = metal
                alum.metalness.contents = NSNumber(value: 0.82)
                alum.roughness.contents = NSNumber(value: 0.55)
                alum.diffuse.wrapS = .repeat
                alum.diffuse.wrapT = .repeat
                alum.diffuse.contentsTransform = SCNMatrix4MakeScale(2.2, 2.2, 1)
            } else {
                alum.diffuse.contents = UIColor(red: 0.78, green: 0.80, blue: 0.82, alpha: 1)
                alum.metalness.contents = NSNumber(value: 0.78)
                alum.roughness.contents = NSNumber(value: 0.58)
            }
            alum.emission.contents = UIColor(white: 0.03, alpha: 1)
            return alum
        }

        private func makePuck() -> SCNNode {
            let g = SCNNode()
            let R: CGFloat = 1.0
            // 略薄、边缘更圆：贴近 Waveshare 1.75 实物比例
            let halfT: CGFloat = 0.22
            let fillet: CGFloat = 0.085
            let alum = frostedAluminum()

            // 主体圆柱略缩半径，前后用环面补圆角（实物级 fillet，非尖锐 CG 边）
            let bodyR = R - fillet * 0.55
            let body = SCNCylinder(radius: bodyR, height: halfT * 2)
            body.radialSegmentCount = 128
            body.heightSegmentCount = 2
            body.materials = [alum]
            let bodyNode = SCNNode(geometry: body)
            bodyNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(bodyNode)

            // 前后边缘圆角（torus）
            for sign: Float in [-1, 1] {
                let torus = SCNTorus(ringRadius: bodyR, pipeRadius: fillet)
                torus.ringSegmentCount = 96
                torus.pipeSegmentCount = 24
                torus.materials = [alum]
                let rim = SCNNode(geometry: torus)
                rim.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
                rim.position = SCNVector3(0, 0, sign * Float(halfT - fillet * 0.15))
                g.addChildNode(rim)
            }

            // 侧壁补圈：填 torus 与圆柱之间的缝，保持连续磨砂银
            let side = SCNCylinder(radius: R - fillet * 0.15, height: max(0.04, halfT * 2 - fillet * 1.6))
            side.radialSegmentCount = 96
            side.materials = [alum]
            let sideNode = SCNNode(geometry: side)
            sideNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(sideNode)

            let bezel = SCNPlane(width: 1.80, height: 1.80)
            bezel.cornerRadius = 0.90
            let bezelMat = SCNMaterial()
            bezelMat.diffuse.contents = UIColor(white: 0.04, alpha: 1)
            bezelMat.lightingModel = .constant
            bezelMat.isDoubleSided = true
            bezel.materials = [bezelMat]
            let bezelNode = SCNNode(geometry: bezel)
            bezelNode.position = SCNVector3(0, 0, Float(halfT) + 0.002)
            g.addChildNode(bezelNode)

            // 屏贴：正方形贴图 + 圆角平面，居中对齐圆脸。
            // 取向：ConnectPuckScreen（mockup C）本身正立（上 200 m / 下 28）。
            // SceneKit 对 UIImage 已按正立采样——勿再加 Y contentsTransform，也勿 eulerAngles.z=π。
            let screen = SCNPlane(width: 1.64, height: 1.64)
            screen.cornerRadius = 0.82
            let face = SCNMaterial()
            face.lightingModel = .constant
            face.diffuse.contents = UIImage(named: "ConnectPuckScreen") ?? UIColor.darkGray
            face.isDoubleSided = true
            screen.materials = [face]
            let screenNode = SCNNode(geometry: screen)
            screenNode.position = SCNVector3(0, 0, Float(halfT) + 0.005)
            g.addChildNode(screenNode)

            // 右侧：上胶囊键 + 5 孔扬声器格栅 + 下圆键（贴合实物侧视）
            addSideControls(to: g, radius: R - fillet * 0.2, material: alum)

            // 软椭圆地面阴影：专用宽软贴图（与圆屏 Tab 紧影 GroundShadow 分开）
            let shadow = SCNPlane(width: 2.55, height: 2.55)
            let shadowMat = SCNMaterial()
            if let img = UIImage(named: "ConnectGroundShadow") ?? UIImage(named: "GroundShadow") {
                shadowMat.diffuse.contents = img
            } else {
                shadowMat.diffuse.contents = UIColor.black.withAlphaComponent(0.16)
            }
            shadowMat.lightingModel = .constant
            shadowMat.writesToDepthBuffer = false
            shadowMat.blendMode = .alpha
            shadowMat.isDoubleSided = true
            shadow.materials = [shadowMat]
            let shadowNode = SCNNode(geometry: shadow)
            shadowNode.eulerAngles = SCNVector3(-Float.pi / 2, 0, 0)
            shadowNode.position = SCNVector3(0, -1.18, 0)
            shadowNode.scale = SCNVector3(1.08, 0.34, 1.0)
            g.addChildNode(shadowNode)

            g.scale = SCNVector3(0.68, 0.68, 0.68)
            return g
        }

        private func addSideControls(to g: SCNNode, radius: CGFloat, material: SCNMaterial) {
            // 略外探，保证正视也能看见格栅与实体键（防白塑光洁圆柱回退）
            let sideX = Float(radius) * 1.02

            // 上键：胶囊
            let topBtn = SCNCapsule(capRadius: 0.036, height: 0.14)
            topBtn.materials = [material]
            let topNode = SCNNode(geometry: topBtn)
            topNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            topNode.position = SCNVector3(sideX, 0.28, 0.02)
            g.addChildNode(topNode)

            // 扬声器：竖直 5 孔
            let holeMat = SCNMaterial()
            holeMat.lightingModel = .constant
            holeMat.diffuse.contents = UIColor(white: 0.08, alpha: 1)
            for i in 0..<5 {
                let hole = SCNSphere(radius: 0.022)
                hole.segmentCount = 12
                hole.materials = [holeMat]
                let hn = SCNNode(geometry: hole)
                let y = 0.09 - Float(i) * 0.048
                hn.position = SCNVector3(sideX + 0.018, y, 0.015)
                g.addChildNode(hn)
            }

            // 下键：圆点
            let botBtn = SCNSphere(radius: 0.032)
            botBtn.segmentCount = 16
            botBtn.materials = [material]
            let botNode = SCNNode(geometry: botBtn)
            botNode.position = SCNVector3(sideX, -0.22, 0.02)
            g.addChildNode(botNode)
        }

        func setSpinning(_ on: Bool, duration: TimeInterval) {
            guard let puck else { return }
            if on {
                guard !spinning else { return }
                spinning = true
                puck.removeAllActions()
                puck.eulerAngles = SCNVector3(-0.12, 0, 0)
                let spin = SCNAction.rotateBy(x: 0, y: CGFloat.pi * 2, z: 0, duration: duration)
                spin.timingMode = .easeInEaseOut
                puck.runAction(spin) { [weak self] in
                    self?.spinning = false
                }
            } else if spinning {
                // 让当前动作自然结束
            } else {
                let yaw = CGFloat(puck.eulerAngles.y)
                let settle = SCNAction.rotateTo(x: -0.08, y: yaw, z: 0, duration: 0.55)
                settle.timingMode = .easeOut
                puck.runAction(settle)
            }
        }
    }
}
