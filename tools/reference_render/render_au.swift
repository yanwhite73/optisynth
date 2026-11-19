// Offline reference renders of installed Audio Unit instruments (request 66).
// No audio device, no MIDI endpoints: notes go straight to the AU; audio is
// pulled with AVAudioEngine manual rendering at 48 kHz.
//
// render_au params <subtype> [manufacturer]
// render_au probe <subtype> <Display Name>   (display text at 21 normalised values)
// render_au render <subtype> <out.wav> <seconds> <notes> [preset=<Arturia preset file>] [Display Name=value | @settings-file ...]
//   preset= loads the whole preset into the AU state (in memory) before any parameter values.
//   notes: pitch:velocity:on_s:off_s separated by commas, e.g. 60:100:0:3,64:100:0:3
// Prints JSON lines: latency, render time, and the process's network sockets
// (lsof) mid-run and at the end.
import AVFoundation
import AudioToolbox
import Foundation

func fourCC(_ s: String) -> FourCharCode { s.utf8.reduce(0) { ($0 << 8) | FourCharCode($1) } }
// Plug-ins may crash in static teardown at process exit: flush and leave without it.
func finish(_ engine: AVAudioEngine) -> Never { engine.stop(); fflush(stdout); _exit(0) }
func fail(_ message: String, _ code: Int32 = 1) -> Never { FileHandle.standardError.write((message + "\n").data(using: .utf8)!); exit(code) }
func sockets() -> String {
    let p = Process(); p.executableURL = URL(fileURLWithPath: "/usr/sbin/lsof")
    p.arguments = ["-nP", "-a", "-i", "-p", String(ProcessInfo.processInfo.processIdentifier)]
    let pipe = Pipe(); p.standardOutput = pipe; p.standardError = Pipe()
    do { try p.run() } catch { return "lsof failed: \(error)" }
    p.waitUntilExit()
    let text = String(data: pipe.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
    return text.isEmpty ? "none" : text.replacingOccurrences(of: "\n", with: " | ")
}

let args = CommandLine.arguments
guard args.count >= 3 else { fail("usage: render_au params|render <subtype> ...", 2) }
let command = args[1], subtype = args[2]
let manufacturer = (command == "params" && args.count > 3) ? args[3] : "Artu"
let description = AudioComponentDescription(componentType: kAudioUnitType_MusicDevice, componentSubType: fourCC(subtype),
                                            componentManufacturer: fourCC(manufacturer), componentFlags: 0, componentFlagsMask: 0)
var loaded: AVAudioUnit?
var loadError: Error?
AVAudioUnit.instantiate(with: description, options: []) { unit, error in loaded = unit; loadError = error }
let deadline = Date().addingTimeInterval(60)
while loaded == nil && loadError == nil && Date() < deadline { RunLoop.current.run(until: Date().addingTimeInterval(0.05)) }
guard let instrument = loaded as? AVAudioUnitMIDIInstrument else { fail("instantiate failed: \(String(describing: loadError))") }
let au = instrument.auAudioUnit

let engine = AVAudioEngine()
let format = AVAudioFormat(standardFormatWithSampleRate: 48000, channels: 2)!
do {
    try engine.enableManualRenderingMode(.offline, format: format, maximumFrameCount: 512)
    engine.attach(instrument)
    engine.connect(instrument, to: engine.mainMixerNode, format: format)
    try engine.start()
} catch { fail("engine: \(error)") }
let parameters = au.parameterTree?.allParameters ?? []

if command == "params" {
    var rows: [[String: Any]] = []
    for p in parameters {
        rows.append(["name": p.displayName, "identifier": p.identifier, "address": p.address, "min": p.minValue, "max": p.maxValue,
                     "value": p.value, "unit": p.unitName ?? "", "values": p.valueStrings ?? []])
    }
    let data = try! JSONSerialization.data(withJSONObject: ["component": au.componentName ?? subtype, "version": au.componentVersion,
                                                            "latency_s": au.latency, "parameters": rows], options: [.prettyPrinted, .sortedKeys])
    FileHandle.standardOutput.write(data)
    print("")
    finish(engine)
}
if command == "state" {
    // Read-only: describe the AU's saved state (fullState) so presets can be loaded whole.
    for (key, value) in au.fullState ?? [:] {
        if let d = value as? Data {
            let text = String(decoding: d.prefix(160), as: UTF8.self).replacingOccurrences(of: "\n", with: " ")
            print(key, "Data", d.count, "bytes; archive:", d.range(of: "serialization::archive".data(using: .utf8)!) != nil, "| head:", text)
        } else { print(key, type(of: value), String(describing: value).prefix(120)) }
    }
    finish(engine)
}
if command == "presets" {
    for p in au.factoryPresets ?? [] { print(p.number, p.name) }
    print("current:", au.currentPreset?.name ?? "none", "| user presets supported:", au.supportsUserPresets)
    finish(engine)
}
if command == "probe" {
    guard args.count >= 4, let p = parameters.first(where: { $0.displayName == args[3] }) else { fail("unknown parameter", 3) }
    for i in 0...20 {
        var v = AUValue(Double(i) / 20 * Double(p.maxValue - p.minValue) + Double(p.minValue))
        print(String(format: "%.3f", v), p.string(fromValue: &v))
    }
    finish(engine)
}
guard command == "render", args.count >= 6, let seconds = Double(args[4]) else { fail("bad render arguments", 2) }
// Arturia AUs keep the full preset as a Boost archive in fullState["Controller State"].
func presetName(_ state: [String: Any]?) -> String {
    guard let d = state?["Controller State"] as? Data else { return "unknown" }
    let words = String(decoding: d.prefix(200), as: UTF8.self).split(separator: " ")
    // Layout: 22 serialization::archive 10 0 7 0 7 <len> <name...> 7 Factory ...
    guard words.count > 8, let length = Int(words[7]) else { return "unknown" }
    var name = ""; var i = 8
    while i < words.count && name.count < length { name += (name.isEmpty ? "" : " ") + words[i]; i += 1 }
    return name
}
var loadedPreset = presetName(au.fullState)
for a in args.dropFirst(6) where a.hasPrefix("preset=") {
    guard let data = FileManager.default.contents(atPath: String(a.dropFirst(7))) else { fail("cannot read preset", 3) }
    var state = au.fullState ?? [:]
    state["Controller State"] = data
    au.fullState = state
    loadedPreset = presetName(au.fullState)
}
var assignments: [String] = []
for a in args.dropFirst(6) where !a.hasPrefix("preset=") {
    if a.hasPrefix("@"), let text = try? String(contentsOfFile: String(a.dropFirst()), encoding: .utf8) {
        assignments += text.split(separator: "\n").map(String.init).filter { !$0.isEmpty && !$0.hasPrefix("#") }
    } else { assignments.append(a) }
}
for assignment in assignments {
    let parts = assignment.split(separator: "=", maxSplits: 1).map(String.init)
    guard parts.count == 2, let value = Float(parts[1]) else { fail("bad assignment \(assignment)", 2) }
    guard let p = parameters.first(where: { $0.displayName == parts[0] }) else { fail("unknown parameter \(parts[0])", 3) }
    p.value = AUValue(value)
}
struct NoteSpec { let pitch: UInt8, velocity: UInt8, on: Double, off: Double }
let notes: [NoteSpec] = args[5].split(separator: ",").map { item in
    let f = item.split(separator: ":").map(String.init)
    guard f.count == 4, let p = UInt8(f[0]), let v = UInt8(f[1]), let on = Double(f[2]), let off = Double(f[3]) else { fail("bad note \(item)", 2) }
    return NoteSpec(pitch: p, velocity: v, on: on, off: off)
}
var file: AVAudioFile?
do { file = try AVAudioFile(forWriting: URL(fileURLWithPath: args[3]), settings: format.settings, commonFormat: .pcmFormatFloat32, interleaved: false) }
catch { fail("output: \(error)") }
let buffer = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: 512)!
let total = AVAudioFramePosition(seconds * 48000)
var position: AVAudioFramePosition = 0
var renderTime = 0.0
var midSockets = ""
while position < total {
    let frames = AVAudioFrameCount(min(512, total - position))
    let t0 = Double(position) / 48000, t1 = Double(position + AVAudioFramePosition(frames)) / 48000
    for n in notes {
        if n.on >= t0 && n.on < t1 { instrument.startNote(n.pitch, withVelocity: n.velocity, onChannel: 0) }
        if n.off >= t0 && n.off < t1 { instrument.stopNote(n.pitch, onChannel: 0) }
    }
    let start = DispatchTime.now().uptimeNanoseconds
    let status: AVAudioEngineManualRenderingStatus
    do { status = try engine.renderOffline(frames, to: buffer) } catch { fail("render: \(error)") }
    renderTime += Double(DispatchTime.now().uptimeNanoseconds - start) / 1e9
    guard status == .success else { fail("render status \(status.rawValue)") }
    do { try file!.write(from: buffer) } catch { fail("write: \(error)") }
    position += AVAudioFramePosition(frames)
    if midSockets.isEmpty && position >= total / 2 { midSockets = sockets() }
}
file = nil   // finalises the WAV header before the fast exit
let report: [String: Any] = ["component": au.componentName ?? subtype, "preset": loadedPreset, "latency_s": au.latency, "audio_s": seconds,
                             "render_s": renderTime, "core_fraction": renderTime / seconds,
                             "sockets_mid": midSockets, "sockets_end": sockets()]
let data = try! JSONSerialization.data(withJSONObject: report, options: [.sortedKeys])
FileHandle.standardOutput.write(data)
print("")
finish(engine)
