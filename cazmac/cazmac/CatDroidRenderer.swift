import MetalKit
import simd

struct CatVertex {
    var position: SIMD2<Float>
    var color: SIMD4<Float>
}

final class CatDroidRenderer: NSObject, MTKViewDelegate {
    var snapshot: CazSnapshot = .empty

    private var device: MTLDevice?
    private var commandQueue: MTLCommandQueue?
    private var pipelineState: MTLRenderPipelineState?
    private var frame: Float = 0
    private let rig = CazRigGenerated.definition

    func configure(view: MTKView) {
        guard let device = view.device else { return }
        self.device = device
        commandQueue = device.makeCommandQueue()

        let descriptor = MTLRenderPipelineDescriptor()
        let library = device.makeDefaultLibrary()
        descriptor.vertexFunction = library?.makeFunction(name: "vertex_main")
        descriptor.fragmentFunction = library?.makeFunction(name: "fragment_main")
        descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
        descriptor.colorAttachments[0].isBlendingEnabled = true
        descriptor.colorAttachments[0].rgbBlendOperation = .add
        descriptor.colorAttachments[0].alphaBlendOperation = .add
        descriptor.colorAttachments[0].sourceRGBBlendFactor = .sourceAlpha
        descriptor.colorAttachments[0].sourceAlphaBlendFactor = .sourceAlpha
        descriptor.colorAttachments[0].destinationRGBBlendFactor = .oneMinusSourceAlpha
        descriptor.colorAttachments[0].destinationAlphaBlendFactor = .oneMinusSourceAlpha
        pipelineState = try? device.makeRenderPipelineState(descriptor: descriptor)
    }

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

    func draw(in view: MTKView) {
        guard
            let device,
            let commandQueue,
            let pipelineState,
            let drawable = view.currentDrawable,
            let passDescriptor = view.currentRenderPassDescriptor
        else { return }

        frame += 1
        var vertices: [CatVertex] = []
        buildScene(vertices: &vertices, aspect: Float(view.drawableSize.width / max(view.drawableSize.height, 1)), time: frame / 60)

        guard !vertices.isEmpty else { return }
        let byteCount = vertices.count * MemoryLayout<CatVertex>.stride
        guard let vertexBuffer = device.makeBuffer(bytes: vertices, length: byteCount, options: .storageModeShared) else { return }

        let commandBuffer = commandQueue.makeCommandBuffer()
        let encoder = commandBuffer?.makeRenderCommandEncoder(descriptor: passDescriptor)
        encoder?.setRenderPipelineState(pipelineState)
        encoder?.setVertexBuffer(vertexBuffer, offset: 0, index: 0)
        encoder?.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: vertices.count)
        encoder?.endEncoding()
        commandBuffer?.present(drawable)
        commandBuffer?.commit()
    }

    private func buildScene(vertices: inout [CatVertex], aspect: Float, time: Float) {
        let skyTop = SIMD4<Float>(0.02, 0.035, 0.052, 1.0)
        let skyBottom = SIMD4<Float>(0.055, 0.071, 0.073, 1.0)
        let floor = SIMD4<Float>(0.20, 0.17, 0.12, 1.0)
        addQuad(&vertices, x0: -aspect, y0: -1, x1: aspect, y1: 1, color: skyTop)
        addQuad(&vertices, x0: -aspect, y0: -1, x1: aspect, y1: -0.42, color: floor)
        addQuad(&vertices, x0: -aspect, y0: -0.42, x1: aspect, y1: -0.38, color: skyBottom)

        for index in 0..<7 {
            let x = -aspect + Float(index) * (2 * aspect / 6)
            addLine(&vertices,
                    from: SIMD2<Float>(x, -0.43),
                    to: SIMD2<Float>(x + 0.22, -1.0),
                    width: 0.004,
                    color: SIMD4<Float>(0.36, 0.31, 0.22, 0.28))
        }

        let style = skillStyle()
        let gait = snapshot.gait % 6
        let scale: Float = 0.92
        let crawlDrop: Float = snapshot.activeSkill == SkillID.crawl ? -0.08 : 0
        let restDrop: Float = snapshot.activeSkill == SkillID.rest ? -0.05 : 0
        let pounceLift: Float = snapshot.activeSkill == SkillID.pounce ? 0.06 : 0
        let bob: Float = (gait == 1 || snapshot.activeSkill == SkillID.walk || snapshot.activeSkill == SkillID.pounce) ? sin(time * 8) * 0.018 : 0
        let bodyHeight = jointNorm(8) * 0.08
        let bodyCenter = SIMD2<Float>(0.12, -0.06 + bodyHeight + crawlDrop + restDrop + pounceLift + bob)
        let bodyRoll = jointNorm(15) * 0.12 + (Float(snapshot.imuRoll) - 128) / 128 * 0.05

        drawRigTorso(vertices: &vertices, body: bodyCenter, scale: scale, roll: bodyRoll, style: style)
        drawRigLegs(vertices: &vertices, body: bodyCenter, scale: scale, time: time, style: style)
        drawRigTail(vertices: &vertices, body: bodyCenter, scale: scale, time: time, style: style)

        let headCenter = rigPoint("head", body: bodyCenter, scale: scale)
            + SIMD2<Float>(jointNorm(0) * 0.09, jointNorm(1) * 0.06)
        drawRigHead(vertices: &vertices, head: headCenter, scale: scale, style: style)
        drawRigEars(vertices: &vertices, head: headCenter, scale: scale, style: style)
        drawEyes(vertices: &vertices, head: headCenter, eyelid: snapshot.eyelid, color: style.eye, glow: style.eyeGlow)

        addLine(&vertices,
                from: headCenter + SIMD2<Float>(-0.12, -0.06),
                to: headCenter + SIMD2<Float>(-0.03, -0.09),
                width: 0.012,
                color: snapshot.vocal == 4 ? SIMD4<Float>(1.0, 0.24, 0.15, 1.0) : style.joint)

        drawSensorRays(vertices: &vertices, head: headCenter, snapshot: snapshot, time: time)
        drawReflexCue(vertices: &vertices, body: bodyCenter, scale: scale, time: time)
    }

    private struct RigStyle {
        let body: SIMD4<Float>
        let shell: SIMD4<Float>
        let dark: SIMD4<Float>
        let joint: SIMD4<Float>
        let accent: SIMD4<Float>
        let paw: SIMD4<Float>
        let eye: SIMD4<Float>
        let eyeGlow: SIMD4<Float>
    }

    private enum SkillID {
        static let rest: UInt8 = 2
        static let sit: UInt8 = 3
        static let walk: UInt8 = 4
        static let crawl: UInt8 = 5
        static let pounce: UInt8 = 6
        static let sniff: UInt8 = 7
        static let scratch: UInt8 = 8
    }

    private func skillStyle() -> RigStyle {
        let base = RigStyle(
            body: SIMD4<Float>(0.34, 0.38, 0.39, 1.0),
            shell: SIMD4<Float>(0.46, 0.50, 0.50, 1.0),
            dark: SIMD4<Float>(0.16, 0.19, 0.20, 1.0),
            joint: SIMD4<Float>(0.78, 0.58, 0.27, 1.0),
            accent: SIMD4<Float>(0.16, 0.72, 1.0, 1.0),
            paw: SIMD4<Float>(0.08, 0.08, 0.075, 1.0),
            eye: SIMD4<Float>(0.17, 0.92, 1.0, 1.0),
            eyeGlow: SIMD4<Float>(0.10, 1.0, 0.42, 0.32)
        )

        switch snapshot.activeSkill {
        case SkillID.rest:
            return RigStyle(body: SIMD4<Float>(0.28, 0.31, 0.32, 1.0), shell: base.shell, dark: base.dark, joint: SIMD4<Float>(0.58, 0.48, 0.32, 1.0), accent: SIMD4<Float>(0.30, 0.72, 0.62, 1.0), paw: base.paw, eye: SIMD4<Float>(0.12, 0.64, 0.78, 1.0), eyeGlow: SIMD4<Float>(0.08, 0.42, 0.50, 0.22))
        case SkillID.sit:
            return RigStyle(body: base.body, shell: SIMD4<Float>(0.50, 0.49, 0.43, 1.0), dark: base.dark, joint: base.joint, accent: SIMD4<Float>(0.88, 0.72, 0.32, 1.0), paw: base.paw, eye: base.eye, eyeGlow: base.eyeGlow)
        case SkillID.walk:
            return base
        case SkillID.crawl:
            return RigStyle(body: SIMD4<Float>(0.26, 0.36, 0.33, 1.0), shell: SIMD4<Float>(0.38, 0.48, 0.44, 1.0), dark: SIMD4<Float>(0.12, 0.20, 0.18, 1.0), joint: base.joint, accent: SIMD4<Float>(0.20, 0.95, 0.58, 1.0), paw: base.paw, eye: SIMD4<Float>(0.34, 1.0, 0.58, 1.0), eyeGlow: SIMD4<Float>(0.20, 1.0, 0.48, 0.25))
        case SkillID.pounce:
            return RigStyle(body: SIMD4<Float>(0.42, 0.34, 0.32, 1.0), shell: SIMD4<Float>(0.56, 0.45, 0.40, 1.0), dark: base.dark, joint: SIMD4<Float>(0.95, 0.55, 0.24, 1.0), accent: SIMD4<Float>(1.0, 0.36, 0.22, 1.0), paw: base.paw, eye: SIMD4<Float>(1.0, 0.74, 0.20, 1.0), eyeGlow: SIMD4<Float>(1.0, 0.32, 0.10, 0.28))
        case SkillID.sniff:
            return RigStyle(body: base.body, shell: base.shell, dark: base.dark, joint: base.joint, accent: SIMD4<Float>(0.26, 0.84, 1.0, 1.0), paw: base.paw, eye: SIMD4<Float>(0.28, 0.92, 1.0, 1.0), eyeGlow: SIMD4<Float>(0.10, 0.82, 1.0, 0.28))
        case SkillID.scratch:
            return RigStyle(body: SIMD4<Float>(0.38, 0.34, 0.42, 1.0), shell: SIMD4<Float>(0.48, 0.43, 0.55, 1.0), dark: base.dark, joint: SIMD4<Float>(0.88, 0.62, 0.92, 1.0), accent: SIMD4<Float>(0.98, 0.52, 1.0, 1.0), paw: base.paw, eye: SIMD4<Float>(0.96, 0.72, 1.0, 1.0), eyeGlow: SIMD4<Float>(0.72, 0.28, 1.0, 0.22))
        default:
            return base
        }
    }

    private func color(for slot: CazRigColorSlot, style: RigStyle) -> SIMD4<Float> {
        switch slot {
        case .accent: return style.accent
        case .body: return style.body
        case .dark: return style.dark
        case .joint: return style.joint
        case .mesh: return style.shell
        case .paw: return style.paw
        case .sensor: return style.eyeGlow
        case .shell: return style.shell
        }
    }

    private func jointNorm(_ index: Int) -> Float {
        guard index >= 0 && index < snapshot.jointValues.count else { return 0 }
        return (Float(snapshot.jointValues[index]) - 128) / 127
    }

    private func rigPoint(_ linkName: String, body: SIMD2<Float>, scale: Float) -> SIMD2<Float> {
        guard let link = rig.link(named: linkName) else { return body }
        return body + link.center * scale
    }

    private func jointPoint(_ jointName: String, body: SIMD2<Float>, scale: Float) -> SIMD2<Float> {
        guard let joint = rig.joint(named: jointName) else { return body }
        return body + joint.origin * scale
    }

    private func drawRigTorso(vertices: inout [CatVertex], body: SIMD2<Float>, scale: Float, roll: Float, style: RigStyle) {
        if let torso = rig.link(named: "torso") {
            let center = body + torso.center * scale
            addEllipse(&vertices, center: center, radius: torso.size * scale * SIMD2<Float>(0.62, 0.70), color: style.dark, segments: 56)
            addEllipse(&vertices, center: center + SIMD2<Float>(-0.03, 0.025 + roll), radius: torso.size * scale * SIMD2<Float>(0.55, 0.56), color: color(for: torso.colorSlot, style: style), segments: 56)
        }
        if let chest = rig.link(named: "chest") {
            addEllipse(&vertices, center: body + chest.center * scale + SIMD2<Float>(jointNorm(9) * -0.05, roll), radius: chest.size * scale * SIMD2<Float>(0.62, 0.62), color: color(for: chest.colorSlot, style: style), segments: 36)
        }
        if let neck = rig.link(named: "neck") {
            let center = body + neck.center * scale
            addLine(&vertices, from: center + SIMD2<Float>(-neck.size.x * scale * 0.45, 0), to: center + SIMD2<Float>(neck.size.x * scale * 0.45, 0.01), width: neck.size.y * scale, color: color(for: neck.colorSlot, style: style))
        }
        addQuad(&vertices, x0: body.x - 0.12, y0: body.y - 0.025, x1: body.x + 0.18, y1: body.y + 0.04, color: SIMD4<Float>(0.05, 0.08, 0.09, 0.75))
    }

    private func drawRigHead(vertices: inout [CatVertex], head: SIMD2<Float>, scale: Float, style: RigStyle) {
        let headSize = rig.link(named: "head")?.size ?? SIMD2<Float>(0.28, 0.22)
        addEllipse(&vertices, center: head, radius: headSize * scale * SIMD2<Float>(0.65, 0.72), color: style.dark, segments: 48)
        addEllipse(&vertices, center: head + SIMD2<Float>(-0.025, 0.018), radius: headSize * scale * SIMD2<Float>(0.54, 0.56), color: style.shell, segments: 44)
        if snapshot.activeSkill == SkillID.sniff {
            addEllipse(&vertices, center: head + SIMD2<Float>(-0.15, -0.035), radius: SIMD2<Float>(0.035, 0.022), color: style.accent, segments: 18)
        }
    }

    private struct RigLegSpec {
        let shoulder: String
        let bend: String
        let upper: String
        let lower: String
        let phase: Float
        let rear: Bool
    }

    private func drawRigLegs(vertices: inout [CatVertex], body: SIMD2<Float>, scale: Float, time: Float, style: RigStyle) {
        let legs = [
            RigLegSpec(shoulder: "left-front-shoulder", bend: "left-elbow", upper: "left-front-upper-leg", lower: "left-front-lower-leg", phase: 0.0, rear: false),
            RigLegSpec(shoulder: "right-front-shoulder", bend: "right-elbow", upper: "right-front-upper-leg", lower: "right-front-lower-leg", phase: 1.8, rear: false),
            RigLegSpec(shoulder: "left-rear-hip", bend: "left-knee", upper: "left-rear-upper-leg", lower: "left-rear-lower-leg", phase: 3.2, rear: true),
            RigLegSpec(shoulder: "right-rear-hip", bend: "right-knee", upper: "right-rear-upper-leg", lower: "right-rear-lower-leg", phase: 4.9, rear: true)
        ]
        let pawSpread = jointNorm(14) * 0.035
        for (index, leg) in legs.enumerated() {
            guard
                let shoulderJoint = rig.joint(named: leg.shoulder),
                let bendJoint = rig.joint(named: leg.bend),
                let upperLink = rig.link(named: leg.upper),
                let lowerLink = rig.link(named: leg.lower)
            else { continue }

            let stride: Float = snapshot.activeSkill == SkillID.walk ? sin(time * 9 + leg.phase) * 0.22 : 0
            let crawlFold: Float = snapshot.activeSkill == SkillID.crawl ? 0.35 : 0
            let pounceReach: Float = snapshot.activeSkill == SkillID.pounce ? (leg.rear ? 0.18 : -0.20) : 0
            let scratchLift: Float = snapshot.activeSkill == SkillID.scratch && index == 1 ? 0.33 + sin(time * 18) * 0.04 : 0
            let shoulder = body + shoulderJoint.origin * scale + SIMD2<Float>(index.isMultiple(of: 2) ? -pawSpread : pawSpread, 0)
            let upperLength = max(upperLink.size.y * scale, 0.16)
            let lowerLength = max(lowerLink.size.y * scale, 0.16)
            let baseAngle = -Float.pi / 2
            let shoulderInput = jointNorm(shoulderJoint.cazJointIndex) * 0.70
            let crawlBias: Float = crawlFold * (leg.rear ? -0.35 : 0.20)
            let shoulderAngle = baseAngle + shoulderInput + stride + crawlBias
            let knee = shoulder + SIMD2<Float>(cos(shoulderAngle), sin(shoulderAngle)) * upperLength
            let bendInput = jointNorm(bendJoint.cazJointIndex) * 0.78
            let bendAngle = shoulderAngle + 0.35 + bendInput + crawlFold
            var paw = knee + SIMD2<Float>(cos(bendAngle), sin(bendAngle)) * lowerLength + SIMD2<Float>(pounceReach, scratchLift)
            if scratchLift == 0 {
                paw.y = min(paw.y, -0.55)
            }

            addLine(&vertices, from: shoulder, to: knee, width: upperLink.size.x * scale, color: color(for: upperLink.colorSlot, style: style))
            addLine(&vertices, from: knee, to: paw, width: lowerLink.size.x * scale, color: color(for: lowerLink.colorSlot, style: style))
            addEllipse(&vertices, center: knee, radius: SIMD2<Float>(0.033, 0.026), color: style.joint, segments: 16)
            addEllipse(&vertices, center: paw + SIMD2<Float>(0.02, -0.005), radius: SIMD2<Float>(0.060, 0.022), color: style.paw, segments: 18)
        }
    }

    private func drawRigTail(vertices: inout [CatVertex], body: SIMD2<Float>, scale: Float, time: Float, style: RigStyle) {
        guard
            let baseJoint = rig.joint(named: "tail-base"),
            let tipJoint = rig.joint(named: "tail-tip"),
            let baseLink = rig.link(named: "tail-base"),
            let tipLink = rig.link(named: "tail-tip")
        else { return }
        let root = body + baseJoint.origin * scale
        let twitch = snapshot.tailPose == 7 ? sin(time * 18) * 0.12 : 0
        let high: Float = (snapshot.tailPose == 6 || snapshot.tailPose == 8 || snapshot.activeSkill == SkillID.pounce) ? 0.55 : 0.05
        let curl: Float = (snapshot.tailPose == 1 || snapshot.tailPose == 2 || snapshot.activeSkill == SkillID.rest) ? 0.45 : 0
        let baseAngle = jointNorm(baseJoint.cazJointIndex) * 0.85 + high + twitch
        let tipAngle = baseAngle + jointNorm(tipJoint.cazJointIndex) * 0.75 + curl
        let mid = root + SIMD2<Float>(cos(baseAngle), sin(baseAngle)) * baseLink.size.x * scale
        let tip = mid + SIMD2<Float>(cos(tipAngle), sin(tipAngle)) * tipLink.size.x * scale
        let width = snapshot.tailPose == 8 ? baseLink.size.y * scale * 1.45 : baseLink.size.y * scale
        addLine(&vertices, from: root, to: mid, width: width, color: style.dark)
        addLine(&vertices, from: mid, to: tip, width: max(width * 0.72, 0.018), color: style.dark)
        addEllipse(&vertices, center: root, radius: SIMD2<Float>(0.050, 0.040), color: style.joint, segments: 18)
    }

    private func drawRigEars(vertices: inout [CatVertex], head: SIMD2<Float>, scale: Float, style: RigStyle) {
        let flat = snapshot.earPose == 3
        let swivel = snapshot.earPose == 4
        let lift: Float = flat ? 0.02 : 0.16
        let lean: Float = swivel ? 0.045 : 0
        let earSize = (rig.link(named: "left-ear")?.size ?? SIMD2<Float>(0.12, 0.18)) * scale
        let leftBase = head + SIMD2<Float>(-0.08, 0.09)
        let rightBase = head + SIMD2<Float>(0.07, 0.09)
        let leftTip = leftBase + SIMD2<Float>(-earSize.x * 0.7 - lean, lift)
        let rightTip = rightBase + SIMD2<Float>(earSize.x * 0.7 + lean, lift)
        addTriangle(&vertices, a: leftBase, b: leftBase + SIMD2<Float>(earSize.x * 0.85, 0.025), c: leftTip, color: style.dark)
        addTriangle(&vertices, a: rightBase, b: rightBase + SIMD2<Float>(earSize.x * 0.85, 0.018), c: rightTip, color: style.dark)
        addTriangle(&vertices, a: leftBase + SIMD2<Float>(0.018, 0.018), b: leftBase + SIMD2<Float>(earSize.x * 0.6, 0.026), c: leftTip + SIMD2<Float>(0.018, -0.035), color: style.joint)
        addTriangle(&vertices, a: rightBase + SIMD2<Float>(0.028, 0.018), b: rightBase + SIMD2<Float>(earSize.x * 0.65, 0.018), c: rightTip + SIMD2<Float>(-0.018, -0.035), color: style.joint)
    }

    private func drawReflexCue(vertices: inout [CatVertex], body: SIMD2<Float>, scale: Float, time: Float) {
        guard snapshot.reflexState != 0 else { return }
        let pulse = 0.72 + sin(time * 12) * 0.14
        let color = SIMD4<Float>(1.0, 0.18, 0.10, 0.28)
        addEllipse(&vertices, center: body, radius: SIMD2<Float>(0.55 * scale * pulse, 0.24 * scale * pulse), color: color, segments: 64)
        addLine(&vertices, from: body + SIMD2<Float>(-0.44, 0.24), to: body + SIMD2<Float>(0.44, 0.24), width: 0.010, color: SIMD4<Float>(1.0, 0.20, 0.10, 0.9))
    }

    private func drawLegs(vertices: inout [CatVertex], body: SIMD2<Float>, gait: UInt8, time: Float, color: SIMD4<Float>, joint: SIMD4<Float>) {
        let crouchDrop: Float = gait == 2 ? -0.08 : 0
        let pounceReach: Float = gait == 3 ? 0.09 : 0
        let offsets: [Float] = [-0.27, -0.12, 0.16, 0.31]
        for (index, xOffset) in offsets.enumerated() {
            let phase = sin(time * 9 + Float(index) * 1.7) * (gait == 1 ? 0.06 : 0.02)
            let shoulder = body + SIMD2<Float>(xOffset, -0.03)
            let paw = SIMD2<Float>(body.x + xOffset + phase + pounceReach, -0.56 + crouchDrop)
            let knee = SIMD2<Float>((shoulder.x + paw.x) * 0.5 + (index.isMultiple(of: 2) ? -0.03 : 0.03), -0.32 + crouchDrop * 0.4)
            addLine(&vertices, from: shoulder, to: knee, width: 0.035, color: color)
            addLine(&vertices, from: knee, to: paw, width: 0.032, color: color)
            addEllipse(&vertices, center: knee, radius: SIMD2<Float>(0.035, 0.028), color: joint, segments: 16)
            addEllipse(&vertices, center: paw + SIMD2<Float>(0.02, -0.005), radius: SIMD2<Float>(0.065, 0.022), color: SIMD4<Float>(0.08, 0.08, 0.075, 1.0), segments: 18)
        }
    }

    private func drawTail(vertices: inout [CatVertex], body: SIMD2<Float>, pose: UInt8, time: Float, color: SIMD4<Float>, joint: SIMD4<Float>) {
        let root = body + SIMD2<Float>(0.40, 0.04)
        let twitch = pose == 7 ? sin(time * 18) * 0.08 : 0
        let bottle = pose == 8
        let high = pose == 6 || pose == 8
        let curl = pose == 1 || pose == 2
        let p1 = root + SIMD2<Float>(0.16, high ? 0.16 : 0.04)
        let p2 = root + SIMD2<Float>(0.34, curl ? 0.16 : (high ? 0.26 : 0.02 + twitch))
        let p3 = root + SIMD2<Float>(0.48, curl ? -0.02 : (high ? 0.20 : -0.07 + twitch))
        let width: Float = bottle ? 0.08 : 0.045
        addLine(&vertices, from: root, to: p1, width: width, color: color)
        addLine(&vertices, from: p1, to: p2, width: width * 0.88, color: color)
        addLine(&vertices, from: p2, to: p3, width: width * 0.72, color: color)
        addEllipse(&vertices, center: root, radius: SIMD2<Float>(0.055, 0.045), color: joint, segments: 18)
    }

    private func drawEars(vertices: inout [CatVertex], head: SIMD2<Float>, pose: UInt8, color: SIMD4<Float>, inner: SIMD4<Float>) {
        let flat = pose == 3
        let swivel = pose == 4
        let lift: Float = flat ? 0.02 : 0.17
        let lean: Float = swivel ? 0.05 : 0.0
        let leftBase = head + SIMD2<Float>(-0.10, 0.10)
        let rightBase = head + SIMD2<Float>(0.08, 0.10)
        let leftTip = leftBase + SIMD2<Float>(-0.08 - lean, lift)
        let rightTip = rightBase + SIMD2<Float>(0.08 + lean, lift)
        addTriangle(&vertices, a: leftBase, b: leftBase + SIMD2<Float>(0.10, 0.03), c: leftTip, color: color)
        addTriangle(&vertices, a: rightBase, b: rightBase + SIMD2<Float>(0.11, 0.02), c: rightTip, color: color)
        addTriangle(&vertices, a: leftBase + SIMD2<Float>(0.02, 0.02), b: leftBase + SIMD2<Float>(0.07, 0.03), c: leftTip + SIMD2<Float>(0.02, -0.04), color: inner)
        addTriangle(&vertices, a: rightBase + SIMD2<Float>(0.03, 0.02), b: rightBase + SIMD2<Float>(0.08, 0.02), c: rightTip + SIMD2<Float>(-0.02, -0.04), color: inner)
    }

    private func drawEyes(vertices: inout [CatVertex], head: SIMD2<Float>, eyelid: UInt8, color: SIMD4<Float>, glow: SIMD4<Float>) {
        let openness = max(0.18, Float(eyelid) / 255)
        let eyeRadius = SIMD2<Float>(0.034, 0.018 * openness)
        let left = head + SIMD2<Float>(-0.075, 0.025)
        let right = head + SIMD2<Float>(0.045, 0.025)
        addEllipse(&vertices, center: left, radius: eyeRadius * 2.2, color: glow, segments: 24)
        addEllipse(&vertices, center: right, radius: eyeRadius * 2.2, color: glow, segments: 24)
        addEllipse(&vertices, center: left, radius: eyeRadius, color: color, segments: 20)
        addEllipse(&vertices, center: right, radius: eyeRadius, color: color, segments: 20)
    }

    private func drawSensorRays(vertices: inout [CatVertex], head: SIMD2<Float>, snapshot: CazSnapshot, time: Float) {
        let motionAlpha = Float(snapshot.eyeMotion) / 255
        let soundAlpha = Float(snapshot.earVolume) / 255
        let scan = sin(time * 3) * 0.08
        addLine(&vertices,
                from: head + SIMD2<Float>(-0.12, 0.04),
                to: head + SIMD2<Float>(-0.55, 0.08 + scan),
                width: 0.006,
                color: SIMD4<Float>(0.12, 1.0, 0.42, 0.10 + motionAlpha * 0.28))
        addLine(&vertices,
                from: head + SIMD2<Float>(0.08, 0.16),
                to: head + SIMD2<Float>(0.34, 0.34),
                width: 0.005,
                color: SIMD4<Float>(0.16, 0.72, 1.0, 0.10 + soundAlpha * 0.24))
        addLine(&vertices,
                from: head + SIMD2<Float>(-0.10, 0.16),
                to: head + SIMD2<Float>(-0.36, 0.32),
                width: 0.005,
                color: SIMD4<Float>(0.16, 0.72, 1.0, 0.10 + soundAlpha * 0.24))
    }
}

private func addQuad(_ vertices: inout [CatVertex], x0: Float, y0: Float, x1: Float, y1: Float, color: SIMD4<Float>) {
    vertices.append(CatVertex(position: SIMD2<Float>(x0, y0), color: color))
    vertices.append(CatVertex(position: SIMD2<Float>(x1, y0), color: color))
    vertices.append(CatVertex(position: SIMD2<Float>(x0, y1), color: color))
    vertices.append(CatVertex(position: SIMD2<Float>(x1, y0), color: color))
    vertices.append(CatVertex(position: SIMD2<Float>(x1, y1), color: color))
    vertices.append(CatVertex(position: SIMD2<Float>(x0, y1), color: color))
}

private func addTriangle(_ vertices: inout [CatVertex], a: SIMD2<Float>, b: SIMD2<Float>, c: SIMD2<Float>, color: SIMD4<Float>) {
    vertices.append(CatVertex(position: a, color: color))
    vertices.append(CatVertex(position: b, color: color))
    vertices.append(CatVertex(position: c, color: color))
}

private func addEllipse(_ vertices: inout [CatVertex], center: SIMD2<Float>, radius: SIMD2<Float>, color: SIMD4<Float>, segments: Int) {
    let count = max(segments, 8)
    for index in 0..<count {
        let a0 = Float(index) / Float(count) * Float.pi * 2
        let a1 = Float(index + 1) / Float(count) * Float.pi * 2
        vertices.append(CatVertex(position: center, color: color))
        vertices.append(CatVertex(position: center + SIMD2<Float>(cos(a0) * radius.x, sin(a0) * radius.y), color: color))
        vertices.append(CatVertex(position: center + SIMD2<Float>(cos(a1) * radius.x, sin(a1) * radius.y), color: color))
    }
}

private func addLine(_ vertices: inout [CatVertex], from: SIMD2<Float>, to: SIMD2<Float>, width: Float, color: SIMD4<Float>) {
    let delta = to - from
    let length = max(simd_length(delta), 0.0001)
    let normal = SIMD2<Float>(-delta.y / length, delta.x / length) * (width * 0.5)
    let a = from + normal
    let b = from - normal
    let c = to + normal
    let d = to - normal
    addTriangle(&vertices, a: a, b: b, c: c, color: color)
    addTriangle(&vertices, a: b, b: d, c: c, color: color)
}
