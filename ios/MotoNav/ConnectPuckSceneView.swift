import SwiftUI
import SceneKit

/// SceneKit 铝合金圆盘：高精屏贴 + 拉丝金属 + 椭圆地面阴影；连接中绕 Y 轴 ease-in-out 转满 360°。
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
            ambient.light?.intensity = 380
            ambient.light?.color = UIColor.white
            scene.rootNode.addChildNode(ambient)

            let key = SCNNode()
            key.light = SCNLight()
            key.light?.type = .directional
            key.light?.intensity = 1200
            key.light?.castsShadow = false
            key.eulerAngles = SCNVector3(-0.9, 0.6, 0)
            scene.rootNode.addChildNode(key)

            let fill = SCNNode()
            fill.light = SCNLight()
            fill.light?.type = .directional
            fill.light?.intensity = 520
            fill.light?.color = UIColor(red: 0.95, green: 0.96, blue: 0.98, alpha: 1)
            fill.eulerAngles = SCNVector3(-0.2, -0.9, 0)
            scene.rootNode.addChildNode(fill)

            let rim = SCNNode()
            rim.light = SCNLight()
            rim.light?.type = .directional
            rim.light?.intensity = 520
            rim.eulerAngles = SCNVector3(-0.5, 2.5, 0)
            scene.rootNode.addChildNode(rim)

            return scene
        }

        private func makePuck() -> SCNNode {
            let g = SCNNode()
            let R: CGFloat = 1.0
            let halfT: CGFloat = 0.28

            let body = SCNCylinder(radius: R, height: halfT * 2)
            body.radialSegmentCount = 128
            body.heightSegmentCount = 2
            let alum = SCNMaterial()
            alum.lightingModel = .physicallyBased
            // 拉丝金属贴图；无资产时回退纯色
            if let metal = UIImage(named: "BrushedMetal") {
                alum.diffuse.contents = metal
                alum.metalness.contents = 1.0
                alum.roughness.contents = 0.28
                alum.diffuse.wrapS = .repeat
                alum.diffuse.wrapT = .repeat
                alum.diffuse.contentsTransform = SCNMatrix4MakeScale(2, 1, 1)
            } else {
                alum.diffuse.contents = UIColor(red: 0.89, green: 0.91, blue: 0.92, alpha: 1)
                alum.metalness.contents = 1.0
                alum.roughness.contents = 0.22
            }
            body.materials = [alum]
            let bodyNode = SCNNode(geometry: body)
            bodyNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            g.addChildNode(bodyNode)

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
            face.diffuse.contentsTransform = SCNMatrix4Translate(SCNMatrix4MakeScale(1, -1, 1), 0, 1, 0)
            face.isDoubleSided = true
            screen.materials = [face]
            let screenNode = SCNNode(geometry: screen)
            screenNode.position = SCNVector3(0, 0, Float(halfT) + 0.005)
            screenNode.eulerAngles = SCNVector3(0, 0, Float.pi)
            g.addChildNode(screenNode)

            let btn = SCNCapsule(capRadius: 0.035, height: 0.12)
            btn.materials = [alum]
            let btnNode = SCNNode(geometry: btn)
            btnNode.eulerAngles = SCNVector3(0, 0, Float.pi / 2)
            btnNode.position = SCNVector3(Float(R) * 0.96, 0.22, 0)
            g.addChildNode(btnNode)

            // 椭圆地面阴影（贴图 + 横向压缩，去掉方影）
            let shadow = SCNPlane(width: 2.2, height: 2.2)
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
            // 压扁成椭圆投影，贴合圆形机身
            shadowNode.scale = SCNVector3(1.0, 0.42, 1.0)
            g.addChildNode(shadowNode)

            // SF Blue 连接环（贴近机身底部）
            let ring = SCNTorus(ringRadius: 1.02, pipeRadius: 0.012)
            let ringMat = SCNMaterial()
            ringMat.diffuse.contents = UIColor(red: 0.0, green: 0.478, blue: 1.0, alpha: 0.85)
            ringMat.emission.contents = UIColor(red: 0.0, green: 0.478, blue: 1.0, alpha: 0.55)
            ringMat.lightingModel = .constant
            ring.materials = [ringMat]
            let ringNode = SCNNode(geometry: ring)
            ringNode.eulerAngles = SCNVector3(Float.pi / 2, 0, 0)
            ringNode.position = SCNVector3(0, -0.95, 0)
            g.addChildNode(ringNode)

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
