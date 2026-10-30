import SwiftUI
import SceneKit

/// SceneKit 铝合金圆盘：连接中绕 Y 轴 ease-in-out 转满 360°，成功后面朝定格。
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

            let ambient = SCNNode()
            ambient.light = SCNLight()
            ambient.light?.type = .ambient
            ambient.light?.intensity = 420
            ambient.light?.color = UIColor.white
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 1100
            key.light?.castsShadow = false
            key.eulerAngles = SCNVector3(-0.9, 0.6, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 500
            fill.light?.color = UIColor(red: 0.95, green: 0.96, blue: 0.98, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.2, -0.9, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 450
            rim.eulerAngles = SCNVector3(-0.5, 2.5, 0)
            scene.rootNode.addChildNode(rim)

            return scene
        }

        private func makePuck() -> SCNNode {
            let g = SCNNode()
            let R: CGFloat = 1.0
            let halfT: CGFloat = 0.28

            let body = SCNCylinder(radius: R, height: halfT * 2)
            body.radialSegmentCount = 96
            body.heightSegmentCount = 1
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            alum.diffuse.contents = UIColor(red: 0.89, green: 0.91, blue: 0.92, alpha: 1)
            alum.metalness.contents = 1.0
            alum.roughness.contents = 0.22
            body.materials = [alum]
            let bodyNode = SCNNode(geometry: body)
            bodyNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(bodyNode)

            // 正面黑边 + 屏贴图：用圆角平面朝 +Z，避免圆柱端面 UV 扭向
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

            let screen = SCNPlane(width: 1.64, height: 1.64)
            screen.cornerRadius = 0.82
            let face = SCNMaterial()
            face.lightingModel = .constant
            face.diffuse.contents = UIImage(named: "ConnectPuckScreen") ?? UIColor.darkGray
            // SCNPlane 默认贴图正立朝 +Z；只做 Y 翻转对齐 UIKit 坐标
            face.diffuse.contentsTransform = SCNMatrix4Translate(SCNMatrix4MakeScale(1, -1, 1), 0, 1, 0)
            face.isDoubleSided = true
            screen.materials = [face]
            let screenNode = SCNNode(geometry: screen)
            screenNode.position = SCNVector3(0, 0, Float(halfT) + 0.005)
            // 屏内容绕平面法线转 180°（绕 Z 轴，仍朝 +Z，内容上下左右翻转）
            screenNode.eulerAngles = SCNVector3(0, 0, Float.pi)
            g.addChildNode(screenNode)

            // 侧面按键示意
            let btn = SCNCapsule(capRadius: 0.035, height: 0.12)
            btn.materials = [alum]
            let btnNode = SCNNode(geometry: btn)
            btnNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            btnNode.position = SCNVector3(Float(R) * 0.96, 0.22, 0)
            g.addChildNode(btnNode)

            // 接触阴影
            let shadow = SCNPlane(width: 1.7, height: 1.7)
            let shadowMat = SCNMaterial()
            shadowMat.diffuse.contents = UIColor.black.withAlphaComponent(0.14)
            shadowMat.lightingModel = .constant
            shadowMat.writesToDepthBuffer = false
            shadow.materials = [shadowMat]
            let shadowNode = SCNNode(geometry: shadow)
            shadowNode.eulerAngles = SCNVector3(-Float.pi / 2, 0, 0)
            shadowNode.position = SCNVector3(0, -1.15, 0)
            g.addChildNode(shadowNode)

            // 略缩小，避免边缘裁切（对齐 mock 0.68）
            g.scale = SCNVector3(0.68, 0.68, 0.68)
            return g
        }

        func setSpinning(_ on: Bool, duration: TimeInterval) {
            guard let puck else { return }
            if on {
                guard !spinning else { return }
                spinning = true
                puck.removeAllActions()
                puck.eulerAngles = SCNVector3(-0.12, 0, 0)
                // ease-in-out 一整圈（SceneKit timingMode）
                let spin = SCNAction.rotateBy(x: 0, y: CGFloat.pi * 2, z: 0, duration: duration)
                spin.timingMode = .easeInEaseOut
                puck.runAction(spin) { [weak self] in
                    self?.spinning = false
                }
            } else if spinning {
                // 让当前动作自然结束；已连上时不再打断
            } else {
                // 定格略抬正面
                let yaw = CGFloat(puck.eulerAngles.y)
                let settle = SCNAction.rotateTo(x: -0.08, y: yaw, z: 0, duration: 0.55)
                settle.timingMode = .easeOut
                puck.runAction(settle)
            }
        }
    }
}
