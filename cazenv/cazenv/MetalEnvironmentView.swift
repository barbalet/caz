import MetalKit
import SwiftUI

#if os(macOS)
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
#else
import UIKit

struct MetalEnvironmentView: UIViewRepresentable {
    var snapshot: EnvSnapshot

    func makeCoordinator() -> EnvironmentRenderer {
        EnvironmentRenderer()
    }

    func makeUIView(context: Context) -> EnvironmentMTKView {
        let view = EnvironmentMTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
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

    func updateUIView(_ uiView: EnvironmentMTKView, context: Context) {
        context.coordinator.snapshot = snapshot
    }
}

final class EnvironmentMTKView: MTKView {
    weak var renderer: EnvironmentRenderer?
    private var lastPanTranslation: CGPoint = .zero

    override init(frame frameRect: CGRect, device: MTLDevice?) {
        super.init(frame: frameRect, device: device)
        installGestures()
    }

    required init(coder: NSCoder) {
        super.init(coder: coder)
        installGestures()
    }

    private func installGestures() {
        isMultipleTouchEnabled = true

        let pan = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        pan.minimumNumberOfTouches = 1
        pan.maximumNumberOfTouches = 2
        addGestureRecognizer(pan)

        let pinch = UIPinchGestureRecognizer(target: self, action: #selector(handlePinch(_:)))
        addGestureRecognizer(pinch)
    }

    @objc private func handlePan(_ gesture: UIPanGestureRecognizer) {
        let translation = gesture.translation(in: self)
        if gesture.state == .began {
            lastPanTranslation = translation
            return
        }

        let dx = Float(translation.x - lastPanTranslation.x)
        let dy = Float(translation.y - lastPanTranslation.y)
        lastPanTranslation = translation

        if gesture.numberOfTouches >= 2 {
            renderer?.orbit(deltaYaw: dx * 0.006, deltaPitch: dy * 0.005)
        } else {
            renderer?.pan(deltaX: -dx * 0.025, deltaZ: dy * 0.025)
        }

        if gesture.state == .ended || gesture.state == .cancelled || gesture.state == .failed {
            lastPanTranslation = .zero
        }
    }

    @objc private func handlePinch(_ gesture: UIPinchGestureRecognizer) {
        let delta = Float(1.0 - gesture.scale) * 16.0
        renderer?.zoom(delta: delta)
        gesture.scale = 1.0
    }
}
#endif
