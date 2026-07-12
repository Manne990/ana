import AppKit
import CoreImage
import CoreMedia
import CoreGraphics
import Foundation
import ScreenCaptureKit

final class FrameWriter: NSObject, SCStreamOutput {
    private let outputDirectory: URL
    private let context = CIContext(options: nil)
    private let lock = NSLock()
    private var frameIndex = 0

    init(outputDirectory: URL) {
        self.outputDirectory = outputDirectory
    }

    var count: Int {
        lock.lock()
        defer { lock.unlock() }
        return frameIndex
    }

    func stream(
        _ stream: SCStream,
        didOutputSampleBuffer sampleBuffer: CMSampleBuffer,
        of outputType: SCStreamOutputType
    ) {
        guard outputType == .screen,
              sampleBuffer.isValid,
              let pixelBuffer = sampleBuffer.imageBuffer else {
            return
        }

        let image = CIImage(cvPixelBuffer: pixelBuffer)
        guard let cgImage = context.createCGImage(image, from: image.extent) else {
            return
        }
        let bitmap = NSBitmapImageRep(cgImage: cgImage)
        guard let data = bitmap.representation(using: .png, properties: [:]) else {
            return
        }

        lock.lock()
        frameIndex += 1
        let index = frameIndex
        lock.unlock()

        let name = String(format: "screen-recording-%03d.png", index)
        try? data.write(to: outputDirectory.appendingPathComponent(name), options: .atomic)
    }
}

@main
struct CaptureWindowFrames {
    static func main() async throws {
        _ = NSApplication.shared
        guard CommandLine.arguments.count == 5,
              let pid = pid_t(CommandLine.arguments[1]),
              let duration = Double(CommandLine.arguments[3]),
              let fps = Int(CommandLine.arguments[4]),
              duration > 0,
              fps > 0 else {
            FileHandle.standardError.write(
                Data("usage: capture_window_frames PID OUTPUT_DIR SECONDS FPS\n".utf8)
            )
            Foundation.exit(2)
        }

        let outputDirectory = URL(fileURLWithPath: CommandLine.arguments[2])
        try FileManager.default.createDirectory(
            at: outputDirectory,
            withIntermediateDirectories: true
        )

        let content = try await SCShareableContent.excludingDesktopWindows(
            false,
            onScreenWindowsOnly: true
        )
        guard let window = content.windows.first(where: {
            $0.owningApplication?.processID == pid &&
                $0.frame.width >= 200 &&
                $0.frame.height >= 150
        }) else {
            FileHandle.standardError.write(
                Data("could not find a visible window for PID \(pid)\n".utf8)
            )
            Foundation.exit(3)
        }

        let center = CGPoint(x: window.frame.midX, y: window.frame.midY)
        guard let display = content.displays.first(where: { $0.frame.contains(center) }) else {
            FileHandle.standardError.write(
                Data("could not find the display containing the FS-UAE window\n".utf8)
            )
            Foundation.exit(5)
        }

        // Capturing the independent window omits FS-UAE's OpenGL surface on
        // macOS. Capture the post-composited display and crop it to the window
        // instead; this is the same pixel stream a person sees on screen.
        let filter = SCContentFilter(display: display, excludingWindows: [])
        let configuration = SCStreamConfiguration()
        let displayMode = CGDisplayCopyDisplayMode(display.displayID)
        let displayBounds = CGDisplayBounds(display.displayID)
        let scale = max(
            1.0,
            Double(displayMode?.pixelWidth ?? Int(display.frame.width)) /
                max(1.0, Double(displayBounds.width))
        )
        configuration.sourceRect = CGRect(
            x: window.frame.minX - display.frame.minX,
            y: window.frame.minY - display.frame.minY,
            width: window.frame.width,
            height: window.frame.height
        )
        configuration.width = max(1, Int(window.frame.width * scale))
        configuration.height = max(1, Int(window.frame.height * scale))
        configuration.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(fps))
        configuration.queueDepth = 6
        configuration.showsCursor = false
        configuration.pixelFormat = kCVPixelFormatType_32BGRA

        let writer = FrameWriter(outputDirectory: outputDirectory)
        let stream = SCStream(filter: filter, configuration: configuration, delegate: nil)
        let queue = DispatchQueue(label: "org.ana.capture-window-frames")
        try stream.addStreamOutput(writer, type: .screen, sampleHandlerQueue: queue)
        try await stream.startCapture()
        try await Task.sleep(for: .seconds(duration))
        try await stream.stopCapture()
        try await Task.sleep(for: .milliseconds(300))

        print("captured_frames=\(writer.count)")
        if writer.count == 0 {
            Foundation.exit(4)
        }
    }
}
