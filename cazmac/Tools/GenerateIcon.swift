import AppKit
import Foundation

let scriptURL = URL(fileURLWithPath: CommandLine.arguments[0]).standardizedFileURL
let projectRoot = scriptURL.deletingLastPathComponent().deletingLastPathComponent()
let sourceURL = projectRoot.appendingPathComponent("Artwork/caz-reference.png")
let output = projectRoot.appendingPathComponent("cazmac/Assets.xcassets/AppIcon.appiconset")

guard let reference = NSImage(contentsOf: sourceURL) else {
    fatalError("Could not load reference artwork at \(sourceURL.path)")
}

try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)

let sizes = [16, 32, 64, 128, 256, 512, 1024]
let sourceSize = reference.size

for size in sizes {
    let image = NSImage(size: NSSize(width: size, height: size))
    image.lockFocus()
    NSGraphicsContext.current?.shouldAntialias = true
    NSGraphicsContext.current?.imageInterpolation = size <= 64 ? .high : .default

    let scale = CGFloat(size) / 1024.0
    let canvas = NSRect(x: 0, y: 0, width: size, height: size)
    let plate = canvas.insetBy(dx: 38 * scale, dy: 38 * scale)
    let corner = 164 * scale

    func scaledRect(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat) -> NSRect {
        NSRect(x: x * scale, y: y * scale, width: w * scale, height: h * scale)
    }

    NSColor(calibratedRed: 0.010, green: 0.012, blue: 0.014, alpha: 1).setFill()
    NSBezierPath(rect: canvas).fill()

    let outer = NSBezierPath(roundedRect: plate, xRadius: corner, yRadius: corner)
    NSColor(calibratedRed: 0.025, green: 0.028, blue: 0.031, alpha: 1).setFill()
    outer.fill()

    NSGraphicsContext.saveGraphicsState()
    outer.addClip()
    reference.draw(in: plate,
                   from: NSRect(origin: .zero, size: sourceSize),
                   operation: .sourceOver,
                   fraction: 1.0,
                   respectFlipped: false,
                   hints: [.interpolation: NSImageInterpolation.high])

    let topShade = NSGradient(colors: [
        NSColor(calibratedWhite: 0.0, alpha: 0.22),
        NSColor(calibratedWhite: 0.0, alpha: 0.0)
    ])
    topShade?.draw(in: plate, angle: 270)

    let vignette = NSBezierPath(ovalIn: scaledRect(40, 30, 944, 910))
    NSColor(calibratedWhite: 0.0, alpha: 0.22).setFill()
    vignette.fill()
    NSGraphicsContext.restoreGraphicsState()

    let border = NSBezierPath(roundedRect: plate.insetBy(dx: 2 * scale, dy: 2 * scale),
                              xRadius: corner,
                              yRadius: corner)
    border.lineWidth = max(4.0 * scale, 0.75)
    NSColor(calibratedRed: 0.46, green: 0.50, blue: 0.51, alpha: 0.34).setStroke()
    border.stroke()

    let innerRadius = max(corner - 18 * scale, 1)
    let innerBorder = NSBezierPath(roundedRect: plate.insetBy(dx: 26 * scale, dy: 26 * scale),
                                   xRadius: innerRadius,
                                   yRadius: innerRadius)
    innerBorder.lineWidth = max(1.5 * scale, 0.5)
    NSColor(calibratedRed: 0.68, green: 0.72, blue: 0.73, alpha: 0.16).setStroke()
    innerBorder.stroke()

    image.unlockFocus()

    guard
        let tiff = image.tiffRepresentation,
        let bitmap = NSBitmapImageRep(data: tiff),
        let png = bitmap.representation(using: .png, properties: [:])
    else {
        fatalError("Could not render icon size \(size)")
    }

    try png.write(to: output.appendingPathComponent("cazmac-icon-\(size).png"))
}

print("Generated CazMac icon set from \(sourceURL.path)")
