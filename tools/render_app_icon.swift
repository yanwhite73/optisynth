// swift tools/render_app_icon.swift <lucky-dip_pattern-6.png> <output.iconset>
import Foundation
import CoreGraphics
import CoreText
import ImageIO
import UniformTypeIdentifiers

let input = URL(fileURLWithPath: CommandLine.arguments[1])
let output = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let source = CGImageSourceCreateWithURL(input as CFURL, nil)!
let original = CGImageSourceCreateImageAtIndex(source, 0, nil)!
precondition(original.width == 7600 && original.height == 3800)
let detail = original.cropping(to: CGRect(x: 4450, y: 0, width: 3000, height: 3000))!
let font = CTFontCreateWithName("Baskerville-SemiBoldItalic" as CFString, 800, nil)
precondition(CTFontCopyPostScriptName(font) as String == "Baskerville-SemiBoldItalic")
var letter = UniChar(79), glyph = CGGlyph()
precondition(CTFontGetGlyphsForCharacters(font, &letter, &glyph, 1))
let outline = CTFontCreatePathForGlyph(font, glyph, nil)!
let colors = CGColorSpace(name: CGColorSpace.sRGB)!

for points in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = points * scale
        let c = CGContext(data: nil, width: pixels, height: pixels,
                          bitsPerComponent: 8, bytesPerRow: 0, space: colors,
                          bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        let unit = CGFloat(pixels) / 1024
        c.scaleBy(x: unit, y: unit)
        let tile = CGRect(x: 54, y: 54, width: 916, height: 916)
        c.addPath(CGPath(roundedRect: tile, cornerWidth: 211, cornerHeight: 211, transform: nil))
        c.clip()
        c.interpolationQuality = .high
        c.draw(detail, in: tile)
        // Optical centring uses the visible outline, independent of font bearings.
        let bounds = outline.boundingBoxOfPath
        let fit = (points <= 32 ? 676.0 : 633.0) / bounds.height
        c.translateBy(x: 512 - bounds.midX * fit, y: 512 - bounds.midY * fit)
        c.scaleBy(x: fit, y: fit)
        c.addPath(outline)
        c.setLineJoin(.round)
        c.setLineWidth((points <= 32 ? 25 : 13) / fit)
        c.setStrokeColor(CGColor(srgbRed: 0.09, green: 0.065, blue: 0.19, alpha: 0.92))
        c.strokePath()
        c.addPath(outline)
        c.setFillColor(CGColor(srgbRed: 1, green: 0.967, blue: 0.86, alpha: 1))
        c.fillPath()
        let suffix = scale == 2 ? "@2x" : ""
        let name = "icon_\(points)x\(points)\(suffix).png"
        let target = CGImageDestinationCreateWithURL(output.appendingPathComponent(name) as CFURL,
                                                      UTType.png.identifier as CFString, 1, nil)!
        CGImageDestinationAddImage(target, c.makeImage()!, nil)
        precondition(CGImageDestinationFinalize(target))
    }
}
