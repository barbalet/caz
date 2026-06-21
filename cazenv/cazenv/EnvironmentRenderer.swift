import Foundation
import MetalKit
import simd

struct EnvVertex {
    var position: SIMD3<Float>
    var color: SIMD4<Float>
}

private struct EnvCamera {
    var target = SIMD3<Float>(0, 5.5, 0)
    var yaw: Float = 0.72
    var pitch: Float = -0.42
    var distance: Float = 72
}

final class EnvironmentRenderer: NSObject, MTKViewDelegate {
    var snapshot: EnvSnapshot = .empty

    private var device: MTLDevice?
    private var commandQueue: MTLCommandQueue?
    private var pipelineState: MTLRenderPipelineState?
    private var frame: Float = 0
    private var camera = EnvCamera()
    private lazy var catMesh = EnvLowPolyCatMesh.load()

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

    func pan(deltaX: Float, deltaZ: Float) {
        let right = SIMD3<Float>(cos(camera.yaw), 0, -sin(camera.yaw))
        let forward = SIMD3<Float>(sin(camera.yaw), 0, cos(camera.yaw))
        camera.target += right * deltaX + forward * deltaZ
        camera.target.x = clamp(camera.target.x, -24, 24)
        camera.target.z = clamp(camera.target.z, -42, 42)
    }

    func zoom(delta: Float) {
        camera.distance = clamp(camera.distance + delta, 8, 125)
    }

    func orbit(deltaYaw: Float, deltaPitch: Float) {
        camera.yaw += deltaYaw
        camera.pitch = clamp(camera.pitch + deltaPitch, -1.28, 0.08)
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
        let aspect = Float(view.drawableSize.width / max(view.drawableSize.height, 1))
        let vertices = buildVertices(aspect: aspect, time: frame / 60)
        guard !vertices.isEmpty else { return }

        let byteCount = vertices.count * MemoryLayout<EnvVertex>.stride
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

    private func buildVertices(aspect: Float, time: Float) -> [EnvVertex] {
        var triangles: [WorldTriangle] = []
        triangles.reserveCapacity(32000)

        addRoom(to: &triangles)
        for fixture in snapshot.fixtures {
            addFixture(fixture, to: &triangles)
        }
        for droid in snapshot.droids {
            addDroid(droid, time: time, to: &triangles)
        }

        var projected: [ProjectedTriangle] = []
        projected.reserveCapacity(triangles.count)
        let view = CameraBasis(camera: camera)
        for triangle in triangles {
            if let item = project(triangle, view: view, aspect: aspect) {
                projected.append(item)
            }
        }

        projected.sort { $0.depth > $1.depth }
        var vertices: [EnvVertex] = []
        vertices.reserveCapacity(projected.count * 3)
        for triangle in projected {
            vertices.append(EnvVertex(position: triangle.a, color: triangle.color))
            vertices.append(EnvVertex(position: triangle.b, color: triangle.color))
            vertices.append(EnvVertex(position: triangle.c, color: triangle.color))
        }
        return vertices
    }

    private func addRoom(to triangles: inout [WorldTriangle]) {
        let floorColor = SIMD4<Float>(0.18, 0.17, 0.145, 1)
        let gridColor = SIMD4<Float>(0.40, 0.39, 0.33, 0.34)
        let wallColor = SIMD4<Float>(0.12, 0.15, 0.17, 0.38)
        let w: Float = 15
        let l: Float = 30
        let h: Float = 15

        addQuad3D(&triangles,
                  SIMD3<Float>(-w, 0, -l), SIMD3<Float>(w, 0, -l),
                  SIMD3<Float>(w, 0, l), SIMD3<Float>(-w, 0, l),
                  floorColor)
        addQuad3D(&triangles,
                  SIMD3<Float>(-w, 0, -l), SIMD3<Float>(-w, h, -l),
                  SIMD3<Float>(w, h, -l), SIMD3<Float>(w, 0, -l),
                  wallColor)
        addQuad3D(&triangles,
                  SIMD3<Float>(-w, 0, l), SIMD3<Float>(w, 0, l),
                  SIMD3<Float>(w, h, l), SIMD3<Float>(-w, h, l),
                  wallColor)
        addQuad3D(&triangles,
                  SIMD3<Float>(-w, 0, -l), SIMD3<Float>(-w, 0, l),
                  SIMD3<Float>(-w, h, l), SIMD3<Float>(-w, h, -l),
                  wallColor)
        addQuad3D(&triangles,
                  SIMD3<Float>(w, 0, -l), SIMD3<Float>(w, h, -l),
                  SIMD3<Float>(w, h, l), SIMD3<Float>(w, 0, l),
                  wallColor)

        for x in stride(from: -15, through: 15, by: 5) {
            addLine3D(&triangles, SIMD3<Float>(Float(x), 0.012, -30), SIMD3<Float>(Float(x), 0.012, 30), width: 0.035, color: gridColor)
        }
        for z in stride(from: -30, through: 30, by: 5) {
            addLine3D(&triangles, SIMD3<Float>(-15, 0.014, Float(z)), SIMD3<Float>(15, 0.014, Float(z)), width: 0.035, color: gridColor)
        }
    }

    private func addFixture(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        switch fixture.type {
        case 1:
            addBox(&triangles,
                   center: SIMD3<Float>(fixture.x, fixture.y, fixture.z),
                   size: SIMD3<Float>(fixture.width, fixture.height, fixture.length),
                   yaw: fixture.yaw,
                   color: SIMD4<Float>(0.18, 0.28, 0.34, 1))
            addChargingPads(fixture, to: &triangles)
        case 2:
            addCatBed(fixture, to: &triangles)
        case 3:
            addTallTree(fixture, to: &triangles)
        case 4:
            addMidTree(fixture, to: &triangles)
        case 5:
            addCompactTree(fixture, to: &triangles)
        case 6:
            addScratchPost(fixture, to: &triangles)
        case 7:
            addJunctionBox(fixture, to: &triangles)
        default:
            break
        }
    }

    private func addChargingPads(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        for row in 0..<2 {
            for column in 0..<4 {
                let local = SIMD3<Float>(
                    -1.05 + Float(column) * 0.70,
                    fixture.height + 0.025,
                    -0.42 + Float(row) * 0.84
                )
                let world = rotateY(local, fixture.yaw) + SIMD3<Float>(fixture.x, 0, fixture.z)
                addBox(&triangles,
                       center: world,
                       size: SIMD3<Float>(0.46, 0.05, 0.52),
                       yaw: fixture.yaw,
                       color: SIMD4<Float>(0.08, 0.95, 1.0, 0.78))
            }
        }
    }

    private func addCatBed(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        let center = SIMD3<Float>(fixture.x, 0.18, fixture.z)
        addOvalDisc(&triangles,
                    center: center,
                    radiusX: fixture.width * 0.5,
                    radiusZ: fixture.length * 0.5,
                    yaw: fixture.yaw,
                    color: SIMD4<Float>(0.42, 0.23, 0.30, 1))
        addOvalRing(&triangles,
                    center: center + SIMD3<Float>(0, 0.14, 0),
                    radiusX: fixture.width * 0.55,
                    radiusZ: fixture.length * 0.55,
                    tube: 0.14,
                    yaw: fixture.yaw,
                    color: SIMD4<Float>(0.62, 0.38, 0.46, 1))
    }

    private func addTallTree(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        let base = SIMD3<Float>(fixture.x, 0, fixture.z)
        addBox(&triangles, center: base + SIMD3<Float>(0, 0.08, 0), size: SIMD3<Float>(fixture.width, 0.16, fixture.length), yaw: fixture.yaw, color: SIMD4<Float>(0.38, 0.34, 0.28, 1))
        for offset in [SIMD3<Float>(-0.55, 0, -0.45), SIMD3<Float>(0.55, 0, -0.30), SIMD3<Float>(0.0, 0, 0.55)] {
            let world = rotateY(offset, fixture.yaw) + base
            addCylinder(&triangles, center: world + SIMD3<Float>(0, 2.55, 0), radius: 0.12, height: 5.1, color: SIMD4<Float>(0.52, 0.45, 0.32, 1))
        }
        for (height, size) in [(2.15 as Float, 1.20 as Float), (3.95, 1.08), (5.85, 1.28)] {
            addBox(&triangles, center: base + SIMD3<Float>(0, height, 0), size: SIMD3<Float>(size, 0.16, size), yaw: fixture.yaw + height * 0.2, color: SIMD4<Float>(0.50, 0.48, 0.42, 1))
        }
        addBox(&triangles, center: base + rotateY(SIMD3<Float>(0.45, 1.35, 0.35), fixture.yaw), size: SIMD3<Float>(0.82, 0.70, 0.78), yaw: fixture.yaw, color: SIMD4<Float>(0.46, 0.42, 0.36, 1))
    }

    private func addMidTree(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        let base = SIMD3<Float>(fixture.x, 0, fixture.z)
        addBox(&triangles, center: base + SIMD3<Float>(0, 0.08, 0), size: SIMD3<Float>(fixture.width, 0.16, fixture.length), yaw: fixture.yaw, color: SIMD4<Float>(0.34, 0.30, 0.25, 1))
        addBox(&triangles, center: base + SIMD3<Float>(0, 0.88, 0.15), size: SIMD3<Float>(1.05, 0.92, 0.90), yaw: fixture.yaw, color: SIMD4<Float>(0.42, 0.39, 0.34, 1))
        for offset in [SIMD3<Float>(-0.55, 0, -0.45), SIMD3<Float>(0.55, 0, 0.45)] {
            let world = rotateY(offset, fixture.yaw) + base
            addCylinder(&triangles, center: world + SIMD3<Float>(0, 1.75, 0), radius: 0.12, height: 3.2, color: SIMD4<Float>(0.56, 0.48, 0.33, 1))
        }
        addBox(&triangles, center: base + SIMD3<Float>(0, 3.45, 0), size: SIMD3<Float>(1.25, 0.15, 1.05), yaw: fixture.yaw + 0.4, color: SIMD4<Float>(0.50, 0.48, 0.42, 1))
    }

    private func addCompactTree(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        let base = SIMD3<Float>(fixture.x, 0, fixture.z)
        addBox(&triangles, center: base + SIMD3<Float>(0, 0.08, 0), size: SIMD3<Float>(fixture.width, 0.16, fixture.length), yaw: fixture.yaw, color: SIMD4<Float>(0.40, 0.36, 0.30, 1))
        addBox(&triangles, center: base + SIMD3<Float>(0, 0.70, 0), size: SIMD3<Float>(0.85, 0.80, 0.78), yaw: fixture.yaw, color: SIMD4<Float>(0.46, 0.42, 0.36, 1))
        addCylinder(&triangles, center: base + SIMD3<Float>(0, 1.25, -0.35), radius: 0.11, height: 1.7, color: SIMD4<Float>(0.58, 0.49, 0.34, 1))
        addBox(&triangles, center: base + SIMD3<Float>(0, 1.76, -0.35), size: SIMD3<Float>(0.92, 0.14, 0.82), yaw: fixture.yaw + 0.18, color: SIMD4<Float>(0.52, 0.49, 0.42, 1))
    }

    private func addScratchPost(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        let base = SIMD3<Float>(fixture.x, 0, fixture.z)
        addBox(&triangles, center: base + SIMD3<Float>(0, 0.07, 0), size: SIMD3<Float>(fixture.width, 0.14, fixture.length), yaw: fixture.yaw, color: SIMD4<Float>(0.32, 0.28, 0.23, 1))
        addCylinder(&triangles, center: base + SIMD3<Float>(0, fixture.height * 0.5, 0), radius: 0.16, height: fixture.height, color: SIMD4<Float>(0.62, 0.52, 0.34, 1))
        addBox(&triangles, center: base + SIMD3<Float>(0, fixture.height + 0.05, 0), size: SIMD3<Float>(0.62, 0.10, 0.62), yaw: fixture.yaw, color: SIMD4<Float>(0.50, 0.45, 0.38, 1))
    }

    private func addJunctionBox(_ fixture: EnvFixture, to triangles: inout [WorldTriangle]) {
        let center = SIMD3<Float>(fixture.x, fixture.y, fixture.z)
        addBox(&triangles,
               center: center,
               size: SIMD3<Float>(fixture.width, fixture.height, fixture.length),
               yaw: fixture.yaw,
               color: SIMD4<Float>(0.24, 0.25, 0.24, 1))

        let face = center + rotateY(SIMD3<Float>(0, 0, -0.52), fixture.yaw)
        addBox(&triangles,
               center: face,
               size: SIMD3<Float>(0.72, 0.62, 0.055),
               yaw: fixture.yaw,
               color: SIMD4<Float>(0.06, 0.50, 0.62, 0.90))
        addBox(&triangles,
               center: center + SIMD3<Float>(0, fixture.height * 0.66, 0),
               size: SIMD3<Float>(0.12, fixture.height * 1.35, 0.12),
               yaw: fixture.yaw,
               color: SIMD4<Float>(0.16, 0.17, 0.16, 1))
    }

    private func addDroid(_ droid: EnvDroid, time: Float, to triangles: inout [WorldTriangle]) {
        guard !catMesh.triangles.isEmpty else {
            addBox(&triangles, center: SIMD3<Float>(droid.x, 0.45, droid.z), size: SIMD3<Float>(1.6, 0.65, 0.42), yaw: droid.yaw, color: droidColor(droid))
            return
        }

        let pose = EnvCatPose(gait: droid.gait, mode: droid.mode, phase: time * 4.0 + Float(droid.id) * 0.77)
        let baseColor = droidColor(droid)
        for triangle in catMesh.triangles {
            var a = catMesh.transform(triangle.a, segment: triangle.segment, pose: pose)
            var b = catMesh.transform(triangle.b, segment: triangle.segment, pose: pose)
            var c = catMesh.transform(triangle.c, segment: triangle.segment, pose: pose)
            a = rotateY(a, droid.yaw) + SIMD3<Float>(droid.x, 0, droid.z)
            b = rotateY(b, droid.yaw) + SIMD3<Float>(droid.x, 0, droid.z)
            c = rotateY(c, droid.yaw) + SIMD3<Float>(droid.x, 0, droid.z)
            let normal = normalFor(a, b, c)
            triangles.append(WorldTriangle(a: a, b: b, c: c, color: shade(baseColor, normal: normal)))
        }
        addDroidClaws(droid, active: droid.mode == 6, to: &triangles)
    }

    private func addDroidClaws(_ droid: EnvDroid, active: Bool, to triangles: inout [WorldTriangle]) {
        let color = active ? SIMD4<Float>(1.0, 0.66, 0.14, 1.0) : SIMD4<Float>(0.06, 0.065, 0.06, 1.0)
        let localStarts = [
            SIMD3<Float>(-0.64, 0.08, -0.16),
            SIMD3<Float>(-0.64, 0.08, 0.16),
            SIMD3<Float>(0.44, 0.07, -0.15),
            SIMD3<Float>(0.44, 0.07, 0.15)
        ]
        for start in localStarts {
            let reach: Float = active && start.x < 0 ? 0.30 : 0.12
            let end = start + SIMD3<Float>(start.x < 0 ? -reach : reach, -0.03, start.z < 0 ? -0.03 : 0.03)
            let a = rotateY(start, droid.yaw) + SIMD3<Float>(droid.x, 0, droid.z)
            let b = rotateY(end, droid.yaw) + SIMD3<Float>(droid.x, 0, droid.z)
            addLine3D(&triangles, a, b, width: active ? 0.035 : 0.018, color: color)
        }
    }

    private func droidColor(_ droid: EnvDroid) -> SIMD4<Float> {
        if droid.mode == 3 {
            return SIMD4<Float>(0.10, 0.78, 0.92, 1)
        }
        if droid.mode == 5 {
            return SIMD4<Float>(0.64, 0.58, 0.24, 1)
        }
        if droid.mode == 6 {
            return SIMD4<Float>(0.82, 0.43, 0.14, 1)
        }
        if droid.mode == 2 || droid.charge < 0.22 {
            return SIMD4<Float>(0.88, 0.54, 0.18, 1)
        }
        let palette: [SIMD4<Float>] = [
            SIMD4<Float>(0.36, 0.40, 0.40, 1),
            SIMD4<Float>(0.42, 0.38, 0.33, 1),
            SIMD4<Float>(0.30, 0.42, 0.36, 1),
            SIMD4<Float>(0.40, 0.35, 0.45, 1)
        ]
        return palette[droid.id % palette.count]
    }

    private func project(_ triangle: WorldTriangle, view: CameraBasis, aspect: Float) -> ProjectedTriangle? {
        let pa = view.cameraSpace(triangle.a)
        let pb = view.cameraSpace(triangle.b)
        let pc = view.cameraSpace(triangle.c)
        guard pa.z > 0.2, pb.z > 0.2, pc.z > 0.2 else { return nil }
        let fovScale: Float = 1.0 / tan(55.0 * .pi / 360.0)
        func projectPoint(_ p: SIMD3<Float>) -> SIMD3<Float> {
            SIMD3<Float>((p.x * fovScale / aspect) / p.z, (p.y * fovScale) / p.z, 0)
        }
        return ProjectedTriangle(
            a: projectPoint(pa),
            b: projectPoint(pb),
            c: projectPoint(pc),
            depth: (pa.z + pb.z + pc.z) / 3,
            color: triangle.color
        )
    }
}

private struct WorldTriangle {
    let a: SIMD3<Float>
    let b: SIMD3<Float>
    let c: SIMD3<Float>
    let color: SIMD4<Float>
}

private struct ProjectedTriangle {
    let a: SIMD3<Float>
    let b: SIMD3<Float>
    let c: SIMD3<Float>
    let depth: Float
    let color: SIMD4<Float>
}

private struct CameraBasis {
    let eye: SIMD3<Float>
    let right: SIMD3<Float>
    let up: SIMD3<Float>
    let forward: SIMD3<Float>

    init(camera: EnvCamera) {
        let cp = cos(camera.pitch)
        let offset = SIMD3<Float>(
            sin(camera.yaw) * cp,
            sin(camera.pitch),
            cos(camera.yaw) * cp
        ) * camera.distance
        eye = camera.target + offset
        forward = simd_normalize(camera.target - eye)
        right = simd_normalize(simd_cross(forward, SIMD3<Float>(0, 1, 0)))
        up = simd_cross(right, forward)
    }

    func cameraSpace(_ point: SIMD3<Float>) -> SIMD3<Float> {
        let relative = point - eye
        return SIMD3<Float>(simd_dot(relative, right), simd_dot(relative, up), simd_dot(relative, forward))
    }
}

private enum EnvCatSegment {
    case body
    case head
    case tailBase
    case tailTip
    case leftFrontLeg
    case rightFrontLeg
    case leftRearLeg
    case rightRearLeg
}

private struct EnvCatTriangle {
    let a: SIMD3<Float>
    let b: SIMD3<Float>
    let c: SIMD3<Float>
    let segment: EnvCatSegment
}

private struct EnvCatPose {
    let headYaw: Float
    let tailBase: Float
    let tailTip: Float
    let leftFrontLeg: Float
    let rightFrontLeg: Float
    let leftRearLeg: Float
    let rightRearLeg: Float

    init(gait: UInt8, mode: UInt8, phase: Float) {
        let moving = gait == 1 || gait == 2 || gait == 3 || mode == 2 || mode == 6
        let stride: Float = moving ? 0.36 : 0.05
        let pounceBias: Float = gait == 3 ? 0.22 : 0
        headYaw = sin(phase * 0.55) * 0.18
        tailBase = sin(phase * 0.82) * 0.22 + (mode == 2 || mode == 6 ? 0.34 : 0.08)
        tailTip = sin(phase * 1.1 + 0.8) * (mode == 6 ? 0.40 : 0.28)
        leftFrontLeg = sin(phase) * stride
        rightFrontLeg = sin(phase + .pi) * stride
        leftRearLeg = sin(phase + 2.35) * stride + pounceBias
        rightRearLeg = sin(phase + 2.35 + .pi) * stride + pounceBias
    }
}

private struct EnvLowPolyCatMesh {
    let triangles: [EnvCatTriangle]
    private let pivots: [EnvCatSegment: SIMD3<Float>]

    func pivot(_ segment: EnvCatSegment) -> SIMD3<Float> {
        pivots[segment] ?? .zero
    }

    func transform(_ point: SIMD3<Float>, segment: EnvCatSegment, pose: EnvCatPose) -> SIMD3<Float> {
        var transformed = point
        switch segment {
        case .head:
            transformed = rotateAround(pivot(.head), point: transformed, y: pose.headYaw)
        case .tailBase:
            transformed = rotateAround(pivot(.tailBase), point: transformed, z: pose.tailBase)
        case .tailTip:
            transformed = rotateAround(pivot(.tailBase), point: transformed, z: pose.tailBase)
            transformed = rotateAround(pivot(.tailTip), point: transformed, z: pose.tailTip)
        case .leftFrontLeg:
            transformed = rotateAround(pivot(.leftFrontLeg), point: transformed, z: pose.leftFrontLeg)
        case .rightFrontLeg:
            transformed = rotateAround(pivot(.rightFrontLeg), point: transformed, z: pose.rightFrontLeg)
        case .leftRearLeg:
            transformed = rotateAround(pivot(.leftRearLeg), point: transformed, z: pose.leftRearLeg)
        case .rightRearLeg:
            transformed = rotateAround(pivot(.rightRearLeg), point: transformed, z: pose.rightRearLeg)
        case .body:
            break
        }
        return transformed
    }

    static func load() -> EnvLowPolyCatMesh {
        guard
            let url = sourceURL(),
            let data = try? Data(contentsOf: url),
            let mesh = parse(data: data)
        else {
            return EnvLowPolyCatMesh(triangles: [], pivots: [:])
        }
        return mesh
    }

    private static func sourceURL() -> URL? {
        if let bundled = Bundle.main.url(forResource: "LowpolyCAT_fixed", withExtension: "stl") {
            return bundled
        }
        let fileURL = URL(fileURLWithPath: #filePath)
        let root = fileURL.deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        let candidates = [
            root.appendingPathComponent("stock/3d_models/LowpolyCAT_fixed.stl"),
            URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("stock/3d_models/LowpolyCAT_fixed.stl")
        ]
        return candidates.first { FileManager.default.fileExists(atPath: $0.path) }
    }

    private static func parse(data: Data) -> EnvLowPolyCatMesh? {
        guard data.count >= 84 else { return nil }
        let triangleCount = Int(readUInt32(data, at: 80))
        guard data.count >= 84 + triangleCount * 50 else { return nil }

        var rawTriangles: [[SIMD3<Float>]] = []
        var minimum = SIMD3<Float>(Float.greatestFiniteMagnitude, Float.greatestFiniteMagnitude, Float.greatestFiniteMagnitude)
        var maximum = SIMD3<Float>(-Float.greatestFiniteMagnitude, -Float.greatestFiniteMagnitude, -Float.greatestFiniteMagnitude)

        for index in 0..<triangleCount {
            let offset = 84 + index * 50 + 12
            let vertices = (0..<3).map { vertexIndex -> SIMD3<Float> in
                let vertexOffset = offset + vertexIndex * 12
                return SIMD3<Float>(
                    readFloat(data, at: vertexOffset),
                    readFloat(data, at: vertexOffset + 4),
                    readFloat(data, at: vertexOffset + 8)
                )
            }
            for vertex in vertices {
                minimum = simd_min(minimum, vertex)
                maximum = simd_max(maximum, vertex)
            }
            rawTriangles.append(vertices)
        }

        let dimensions = maximum - minimum
        guard dimensions.x > 0, dimensions.y > 0, dimensions.z > 0 else { return nil }
        let modelScale = Float(1.8) / max(dimensions.y, 0.0001)
        let center = SIMD3<Float>((minimum.x + maximum.x) * 0.5, (minimum.y + maximum.y) * 0.5, minimum.z)

        func convert(_ raw: SIMD3<Float>) -> SIMD3<Float> {
            SIMD3<Float>(
                -(raw.y - center.y) * modelScale,
                (raw.z - center.z) * modelScale,
                (raw.x - center.x) * modelScale
            )
        }

        func rawPoint(width: Float, length: Float, height: Float) -> SIMD3<Float> {
            convert(SIMD3<Float>(
                minimum.x + dimensions.x * width,
                minimum.y + dimensions.y * length,
                minimum.z + dimensions.z * height
            ))
        }

        let pivots: [EnvCatSegment: SIMD3<Float>] = [
            .body: rawPoint(width: 0.50, length: 0.46, height: 0.45),
            .head: rawPoint(width: 0.50, length: 0.69, height: 0.55),
            .tailBase: rawPoint(width: 0.50, length: 0.24, height: 0.48),
            .tailTip: rawPoint(width: 0.50, length: 0.10, height: 0.54),
            .leftFrontLeg: rawPoint(width: 0.30, length: 0.56, height: 0.34),
            .rightFrontLeg: rawPoint(width: 0.70, length: 0.56, height: 0.34),
            .leftRearLeg: rawPoint(width: 0.30, length: 0.36, height: 0.34),
            .rightRearLeg: rawPoint(width: 0.70, length: 0.36, height: 0.34)
        ]

        let triangles = rawTriangles.map { vertices -> EnvCatTriangle in
            let centroid = (vertices[0] + vertices[1] + vertices[2]) / 3
            let normalized = (centroid - minimum) / dimensions
            return EnvCatTriangle(
                a: convert(vertices[0]),
                b: convert(vertices[1]),
                c: convert(vertices[2]),
                segment: classify(width: normalized.x, length: normalized.y, height: normalized.z)
            )
        }

        return EnvLowPolyCatMesh(triangles: triangles, pivots: pivots)
    }

    private static func classify(width: Float, length: Float, height: Float) -> EnvCatSegment {
        if length > 0.67 && height > 0.34 { return .head }
        if length < 0.12 && height > 0.28 { return .tailTip }
        if length < 0.26 && height > 0.26 { return .tailBase }
        if height < 0.42 {
            if length > 0.50 {
                return width < 0.50 ? .leftFrontLeg : .rightFrontLeg
            }
            return width < 0.50 ? .leftRearLeg : .rightRearLeg
        }
        return .body
    }

    private static func readUInt32(_ data: Data, at offset: Int) -> UInt32 {
        UInt32(data[offset])
            | UInt32(data[offset + 1]) << 8
            | UInt32(data[offset + 2]) << 16
            | UInt32(data[offset + 3]) << 24
    }

    private static func readFloat(_ data: Data, at offset: Int) -> Float {
        Float(bitPattern: readUInt32(data, at: offset))
    }
}

private func addBox(_ triangles: inout [WorldTriangle], center: SIMD3<Float>, size: SIMD3<Float>, yaw: Float, color: SIMD4<Float>) {
    let hx = size.x * 0.5
    let hy = size.y * 0.5
    let hz = size.z * 0.5
    let raw = [
        SIMD3<Float>(-hx, -hy, -hz), SIMD3<Float>(hx, -hy, -hz), SIMD3<Float>(hx, hy, -hz), SIMD3<Float>(-hx, hy, -hz),
        SIMD3<Float>(-hx, -hy, hz), SIMD3<Float>(hx, -hy, hz), SIMD3<Float>(hx, hy, hz), SIMD3<Float>(-hx, hy, hz)
    ].map { rotateY($0, yaw) + center }
    let faces = [(0, 1, 2, 3), (5, 4, 7, 6), (4, 0, 3, 7), (1, 5, 6, 2), (3, 2, 6, 7), (4, 5, 1, 0)]
    for face in faces {
        addQuad3D(&triangles, raw[face.0], raw[face.1], raw[face.2], raw[face.3], color)
    }
}

private func addCylinder(_ triangles: inout [WorldTriangle], center: SIMD3<Float>, radius: Float, height: Float, color: SIMD4<Float>) {
    let segments = 18
    let bottom = center.y - height * 0.5
    let top = center.y + height * 0.5
    for index in 0..<segments {
        let a0 = Float(index) / Float(segments) * .pi * 2
        let a1 = Float(index + 1) / Float(segments) * .pi * 2
        let p0 = SIMD3<Float>(center.x + cos(a0) * radius, bottom, center.z + sin(a0) * radius)
        let p1 = SIMD3<Float>(center.x + cos(a1) * radius, bottom, center.z + sin(a1) * radius)
        let p2 = SIMD3<Float>(center.x + cos(a1) * radius, top, center.z + sin(a1) * radius)
        let p3 = SIMD3<Float>(center.x + cos(a0) * radius, top, center.z + sin(a0) * radius)
        addQuad3D(&triangles, p0, p1, p2, p3, color)
    }
}

private func addOvalDisc(_ triangles: inout [WorldTriangle], center: SIMD3<Float>, radiusX: Float, radiusZ: Float, yaw: Float, color: SIMD4<Float>) {
    let segments = 28
    for index in 0..<segments {
        let a0 = Float(index) / Float(segments) * .pi * 2
        let a1 = Float(index + 1) / Float(segments) * .pi * 2
        let p0 = center
        let p1 = center + rotateY(SIMD3<Float>(cos(a0) * radiusX, 0, sin(a0) * radiusZ), yaw)
        let p2 = center + rotateY(SIMD3<Float>(cos(a1) * radiusX, 0, sin(a1) * radiusZ), yaw)
        triangles.append(WorldTriangle(a: p0, b: p1, c: p2, color: color))
    }
}

private func addOvalRing(_ triangles: inout [WorldTriangle], center: SIMD3<Float>, radiusX: Float, radiusZ: Float, tube: Float, yaw: Float, color: SIMD4<Float>) {
    let segments = 28
    for index in 0..<segments {
        let a0 = Float(index) / Float(segments) * .pi * 2
        let a1 = Float(index + 1) / Float(segments) * .pi * 2
        let outer0 = center + rotateY(SIMD3<Float>(cos(a0) * radiusX, 0, sin(a0) * radiusZ), yaw)
        let outer1 = center + rotateY(SIMD3<Float>(cos(a1) * radiusX, 0, sin(a1) * radiusZ), yaw)
        let inner0 = center + rotateY(SIMD3<Float>(cos(a0) * max(radiusX - tube, 0.05), 0, sin(a0) * max(radiusZ - tube, 0.05)), yaw)
        let inner1 = center + rotateY(SIMD3<Float>(cos(a1) * max(radiusX - tube, 0.05), 0, sin(a1) * max(radiusZ - tube, 0.05)), yaw)
        addQuad3D(&triangles, inner0, inner1, outer1, outer0, color)
    }
}

private func addLine3D(_ triangles: inout [WorldTriangle], _ a: SIMD3<Float>, _ b: SIMD3<Float>, width: Float, color: SIMD4<Float>) {
    let delta = b - a
    let side = simd_normalize(SIMD3<Float>(-delta.z, 0, delta.x)) * (width * 0.5)
    addQuad3D(&triangles, a + side, b + side, b - side, a - side, color)
}

private func addQuad3D(_ triangles: inout [WorldTriangle], _ a: SIMD3<Float>, _ b: SIMD3<Float>, _ c: SIMD3<Float>, _ d: SIMD3<Float>, _ color: SIMD4<Float>) {
    triangles.append(WorldTriangle(a: a, b: b, c: c, color: color))
    triangles.append(WorldTriangle(a: a, b: c, c: d, color: color))
}

private func rotateY(_ point: SIMD3<Float>, _ angle: Float) -> SIMD3<Float> {
    let c = cos(angle)
    let s = sin(angle)
    return SIMD3<Float>(point.x * c + point.z * s, point.y, -point.x * s + point.z * c)
}

private func rotateAround(_ pivot: SIMD3<Float>, point: SIMD3<Float>, z: Float = 0, y: Float = 0) -> SIMD3<Float> {
    var local = point - pivot
    if z != 0 {
        let c = cos(z)
        let s = sin(z)
        local = SIMD3<Float>(local.x * c - local.y * s, local.x * s + local.y * c, local.z)
    }
    if y != 0 {
        local = rotateY(local, y)
    }
    return pivot + local
}

private func normalFor(_ a: SIMD3<Float>, _ b: SIMD3<Float>, _ c: SIMD3<Float>) -> SIMD3<Float> {
    let n = simd_cross(b - a, c - a)
    let length = simd_length(n)
    return length > 0.00001 ? n / length : SIMD3<Float>(0, 1, 0)
}

private func shade(_ color: SIMD4<Float>, normal: SIMD3<Float>) -> SIMD4<Float> {
    let light = simd_normalize(SIMD3<Float>(-0.35, 0.9, -0.25))
    let amount = clamp(simd_dot(normal, light) * 0.44 + 0.72, 0.38, 1.14)
    return SIMD4<Float>(color.x * amount, color.y * amount, color.z * amount, color.w)
}

private func clamp(_ value: Float, _ minimum: Float, _ maximum: Float) -> Float {
    min(max(value, minimum), maximum)
}
