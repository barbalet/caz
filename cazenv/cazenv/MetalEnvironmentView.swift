import MetalKit
import SwiftUI

struct MetalEnvironmentView: NSViewRepresentable {
    var snapshot: EnvSnapshot

    func makeCoordinator() -> EnvironmentRenderer {
        EnvironmentRenderer()
    }

    func makeNSView(context: Context) -> EnvironmentMTKView {
        let view = EnvironmentMTKView()
        view.device = MTLCreateSystemDefaultDevice()
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.010, green: 0.012, blue: 0.016, alpha: 1.0)
        view.preferredFramesPerSecond = 60
        view.enableSetNeedsDisplay = false
        view.isPaused = false
        view.renderer = context.coordinator
        context.coordinator.configure(view: view)
        view.delegate = context.coordinator
        return view
    }

    func updateNSView(_ nsView: EnvironmentMTKView, context: Context) {
        context.coordinator.snapshot = snapshot
    }
}

final class EnvironmentMTKView: MTKView {
    weak var renderer: EnvironmentRenderer?
    private var lastDrag: NSPoint?

    override var acceptsFirstResponder: Bool { true }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        window?.makeFirstResponder(self)
    }

    override func scrollWheel(with event: NSEvent) {
        if event.modifierFlags.contains(.command) {
            renderer?.orbit(deltaYaw: Float(event.scrollingDeltaX) * 0.008,
                            deltaPitch: Float(event.scrollingDeltaY) * 0.006)
        } else if event.modifierFlags.contains(.option) {
            renderer?.zoom(delta: Float(event.scrollingDeltaY) * 0.05)
        } else {
            renderer?.pan(deltaX: Float(event.scrollingDeltaX) * 0.035,
                          deltaZ: Float(event.scrollingDeltaY) * 0.035)
        }
    }

    override func magnify(with event: NSEvent) {
        renderer?.zoom(delta: Float(-event.magnification) * 45.0)
    }

    override func rotate(with event: NSEvent) {
        renderer?.orbit(deltaYaw: Float(event.rotation) * 0.014, deltaPitch: 0)
    }

    override func mouseDown(with event: NSEvent) {
        lastDrag = convert(event.locationInWindow, from: nil)
    }

    override func rightMouseDown(with event: NSEvent) {
        lastDrag = convert(event.locationInWindow, from: nil)
    }

    override func mouseDragged(with event: NSEvent) {
        dragCamera(with: event, orbit: false)
    }

    override func rightMouseDragged(with event: NSEvent) {
        dragCamera(with: event, orbit: true)
    }

    private func dragCamera(with event: NSEvent, orbit: Bool) {
        let point = convert(event.locationInWindow, from: nil)
        defer { lastDrag = point }
        guard let lastDrag else { return }
        let dx = Float(point.x - lastDrag.x)
        let dy = Float(point.y - lastDrag.y)
        if orbit {
            renderer?.orbit(deltaYaw: dx * 0.006, deltaPitch: dy * 0.005)
        } else {
            renderer?.pan(deltaX: -dx * 0.025, deltaZ: dy * 0.025)
        }
    }
}
