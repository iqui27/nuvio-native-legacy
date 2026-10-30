// Compose approved closed mocks and production-renderer captures without resampling UI geometry.
import AppKit
import Foundation
let root = "/Volumes/ExternalSSD/tmp/rev/ios-m3"
let mocks = "/Volumes/ExternalSSD/tmp/rev/dono-envio"
let screens = [("home", "ios-01-home.jpg"), ("sources", "ios-04-sources.jpg"), ("search", "ios-02-search.jpg"), ("settings", "ios-11-settings.jpg")]
func load(_ path: String) -> NSImage {
    guard let image = NSImage(contentsOfFile: path) else { fatalError("Missing image: \(path)") }
    return image
}
for (screen, mockName) in screens {
    let mock = load("\(mocks)/\(mockName)")
    let app = load("\(root)/raw/\(screen).png")
    let crop = NSRect(x: 55, y: mock.size.height - 955, width: 586, height: 843)
    let canvas = NSImage(size: NSSize(width: 1208, height: 930))
    canvas.lockFocus()
    NSColor(calibratedWhite: 0.06, alpha: 1).setFill()
    NSRect(x: 0, y: 0, width: 1208, height: 930).fill()
    let labels = [("APPROVED MOCK", CGFloat(16)), ("APP · OFFLINE PRODUCTION RENDERER", CGFloat(620))]
    for (title, x) in labels {
        (title as NSString).draw(at: NSPoint(x: x, y: 895), withAttributes: [.font: NSFont.systemFont(ofSize: 15, weight: .semibold), .foregroundColor: NSColor.white])
    }
    mock.draw(in: NSRect(x: 16, y: 32, width: 580, height: 843), from: crop, operation: .sourceOver, fraction: 1)
    app.draw(in: NSRect(x: 620, y: 32, width: 580, height: 843), from: .zero, operation: .sourceOver, fraction: 1)
    canvas.unlockFocus()
    let folder = "\(root)/\(screen)"
    try FileManager.default.createDirectory(atPath: folder, withIntermediateDirectories: true)
    let bitmap = NSBitmapImageRep(data: canvas.tiffRepresentation!)!
    try bitmap.representation(using: .jpeg, properties: [.compressionFactor: 0.9])!.write(to: URL(fileURLWithPath: "\(folder)/mock-vs-app.jpg"))
    try Data(contentsOf: URL(fileURLWithPath: "\(root)/raw/\(screen).png")).write(to: URL(fileURLWithPath: "\(folder)/app.png"))
}
print("PASS: four closed mock/app JPG comparisons")
