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

            // 磨砂银：高环境光 + 弱方向光，压掉镜面高光
            let ambient = SCNNode()
            ambient.light = SCNLight()
            ambient.light?.type = .ambient
            ambient.light?.intensity = 620
            ambient.light?.color = UIColor(white: 0.96, alpha: 1)
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 680
            key.light?.castsShadow = false
            key.light?.color = UIColor(red: 0.97, green: 0.98, blue: 1.0, alpha: 1)
            key.eulerAngles = SCNVector3(-0.85, 0.45, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 420
            fill.light?.color = UIColor(red: 0.93, green: 0.94, blue: 0.96, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.15, -1.0, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 280
            rim.eulerAngles = SCNVector3(-0.4, 2.4, 0)
            scene.rootNode.addChildNode(rim)

            return scene
        }

        private func frostedAluminum() -> SCNMaterial {
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            if let metal = UIImage(named: "FrostedSilver") ?? UIImage(named: "BrushedMetal") {
                alum.diffuse.contents = metal
                alum.metalness.contents = 0.78
                alum.roughness.contents = 0.62
                alum.diffuse.wrapS = .repeat
                alum.diffuse.wrapT = .repeat
                alum.diffuse.contentsTransform = SCNMatrix4MakeScale(1.6, 1.6, 1)
            } else {
                alum.diffuse.contents = UIColor(red: 0.78, green: 0.80, blue: 0.82, alpha: 1)
                alum.metalness.contents = 0.72
                alum.roughness.contents = 0.68
            }
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

            // 软椭圆地面阴影（中心实、边缘高斯衰减；无中空圆环）
            let shadow = SCNPlane(width: 2.55, height: 2.55)
            let shadowMat = SCNMaterial()
            if let img = UIImage(named: "GroundShadow") {
                shadowMat.diffuse.contents = img
            } else {
                shadowMat.diffuse.contents = UIColor.black.withAlphaComponent(0.18)
            }
            shadowMat.lightingModel = .constant
            shadowMat.writesToDepthBuffer = false
            shadowMat.blendMode = .alpha
            shadow.materials = [shadowMat]
            let shadowNode = SCNNode(geometry: shadow)
            shadowNode.eulerAngles = SCNVector3(-Float.pi / 2, 0, 0)
            shadowNode.position = SCNVector3(0, -1.18, 0)
            // 扁椭圆接触影；略拉长前后，避免圆柱投影叠出环
            shadowNode.scale = SCNVector3(1.08, 0.34, 1.0)
            g.addChildNode(shadowNode)

            g.scale = SCNVector3(0.68, 0.68, 0.68)
            return g
        }

        private func addSideControls(to g: SCNNode, radius: CGFloat, material: SCNMaterial) {
            let sideX = Float(radius) * 0.98

            // 上键：胶囊
            let topBtn = SCNCapsule(capRadius: 0.032, height: 0.13)
            topBtn.materials = [material]
            let topNode = SCNNode(geometry: topBtn)
            topNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            topNode.position = SCNVector3(sideX, 0.26, 0.01)
            g.addChildNode(topNode)

            // 扬声器：竖直 5 孔
            let holeMat = SCNMaterial()
            holeMat.lightingModel = .constant
            holeMat.diffuse.contents = UIColor(white: 0.12, alpha: 1)
            for i in 0..<5 {
                let hole = SCNSphere(radius: 0.018)
                hole.segmentCount = 12
                hole.materials = [holeMat]
                let hn = SCNNode(geometry: hole)
                let y = 0.08 - Float(i) * 0.045
                hn.position = SCNVector3(sideX + 0.012, y, 0.01)
                g.addChildNode(hn)
            }

            // 下键：圆点
            let botBtn = SCNSphere(radius: 0.028)
            botBtn.segmentCount = 16
            botBtn.materials = [material]
            let botNode = SCNNode(geometry: botBtn)
            botNode.position = SCNVector3(sideX, -0.20, 0.01)
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
