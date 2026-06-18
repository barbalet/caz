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

        let gait = snapshot.gait % 6
        let crouch = gait == 2 ? -0.10 : (gait == 3 ? 0.08 : 0.0)
        let bob = (gait == 1 || gait == 3) ? sin(time * 8) * 0.018 : 0
        let bodyCenter = SIMD2<Float>(0.28, -0.10 + crouch + bob)
        let headYaw = (Float(snapshot.headYaw) - 128) / 128
        let headCenter = SIMD2<Float>(-0.22 + headYaw * 0.08, 0.02 + crouch + bob * 0.6)
        let graphite = SIMD4<Float>(0.36, 0.39, 0.41, 1.0)
        let graphiteDark = SIMD4<Float>(0.18, 0.20, 0.22, 1.0)
        let seam = SIMD4<Float>(0.06, 0.09, 0.10, 0.9)
        let brass = SIMD4<Float>(0.78, 0.58, 0.27, 1.0)
        let cyan = SIMD4<Float>(0.17, 0.92, 1.0, 1.0)
        let greenGlow = SIMD4<Float>(0.10, 1.0, 0.42, 0.32)

        addEllipse(&vertices, center: bodyCenter, radius: SIMD2<Float>(0.43, 0.18), color: graphiteDark, segments: 56)
        addEllipse(&vertices, center: bodyCenter + SIMD2<Float>(-0.03, 0.03), radius: SIMD2<Float>(0.40, 0.15), color: graphite, segments: 56)
        addQuad(&vertices, x0: -0.05, y0: bodyCenter.y - 0.03, x1: 0.24, y1: bodyCenter.y + 0.05, color: seam)
        addEllipse(&vertices, center: bodyCenter + SIMD2<Float>(0.05, -0.005), radius: SIMD2<Float>(0.12, 0.055), color: SIMD4<Float>(0.10, 0.13, 0.14, 0.85), segments: 28)

        drawLegs(vertices: &vertices, body: bodyCenter, gait: gait, time: time, color: graphiteDark, joint: brass)
        drawTail(vertices: &vertices, body: bodyCenter, pose: snapshot.tailPose, time: time, color: graphiteDark, joint: brass)

        addLine(&vertices, from: bodyCenter + SIMD2<Float>(-0.32, 0.07), to: headCenter + SIMD2<Float>(0.12, -0.06), width: 0.06, color: graphiteDark)
        addEllipse(&vertices, center: headCenter, radius: SIMD2<Float>(0.20, 0.17), color: graphite, segments: 48)
        addEllipse(&vertices, center: headCenter + SIMD2<Float>(-0.03, 0.02), radius: SIMD2<Float>(0.17, 0.13), color: SIMD4<Float>(0.43, 0.46, 0.47, 1.0), segments: 44)

        drawEars(vertices: &vertices, head: headCenter, pose: snapshot.earPose, color: graphiteDark, inner: brass)
        drawEyes(vertices: &vertices, head: headCenter, eyelid: snapshot.eyelid, color: cyan, glow: greenGlow)

        addLine(&vertices,
                from: headCenter + SIMD2<Float>(-0.12, -0.06),
                to: headCenter + SIMD2<Float>(-0.03, -0.09),
                width: 0.012,
                color: snapshot.vocal == 4 ? SIMD4<Float>(1.0, 0.24, 0.15, 1.0) : brass)

        drawSensorRays(vertices: &vertices, head: headCenter, snapshot: snapshot, time: time)
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
