import MetalKit
import SwiftUI

struct MetalCatView: NSViewRepresentable {
    var snapshot: CazSnapshot

    func makeCoordinator() -> CatDroidRenderer {
        CatDroidRenderer()
    }

    func makeNSView(context: Context) -> MTKView {
        let view = MTKView()
        view.device = MTLCreateSystemDefaultDevice()
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.015, green: 0.018, blue: 0.022, alpha: 1.0)
        view.preferredFramesPerSecond = 60
        view.enableSetNeedsDisplay = false
        view.isPaused = false
        context.coordinator.configure(view: view)
        view.delegate = context.coordinator
        return view
    }

    func updateNSView(_ nsView: MTKView, context: Context) {
        context.coordinator.snapshot = snapshot
    }
}
