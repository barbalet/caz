import SwiftUI

struct ContentView: View {
    @StateObject private var runtime = CazEnvRuntime()

    var body: some View {
        ZStack(alignment: .topLeading) {
            MetalEnvironmentView(snapshot: runtime.snapshot)
                .ignoresSafeArea()

            overlay
                .padding(18)
        }
        .background(Color.black)
    }

    private var overlay: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 12) {
                Text("CazEnv")
                    .font(.system(size: 22, weight: .semibold, design: .rounded))
                Text("30 x 60 x 15 ft")
                    .font(.system(.body, design: .monospaced))
                    .foregroundStyle(.secondary)
                Button("Reset") {
                    runtime.reset()
                }
                .buttonStyle(.bordered)
            }

            HStack(spacing: 18) {
                metric("DROIDS", "\(runtime.snapshot.droids.count)")
                metric("BEDS", "\(runtime.snapshot.fixtures.filter { $0.type == 2 }.count)")
                metric("TREES", "\(runtime.snapshot.fixtures.filter { $0.type >= 3 && $0.type <= 6 }.count)")
                metric("JBOX", "\(runtime.snapshot.fixtures.filter { $0.type == 7 }.count)")
                metric("CHARGING", "\(runtime.snapshot.droids.filter { $0.mode == 3 }.count)/8")
                metric("TAP", "\(runtime.snapshot.droids.filter { $0.mode == 6 }.count)")
                metric("VM", "\(runtime.snapshot.droids.filter { $0.bytecodeSteppingEnabled }.count)/\(runtime.snapshot.droids.count)")
            }

            Divider()
                .background(.white.opacity(0.22))

            VStack(alignment: .leading, spacing: 5) {
                ForEach(runtime.snapshot.droids.prefix(8)) { droid in
                    HStack(spacing: 8) {
                        Text(String(format: "%02d", droid.id))
                            .font(.system(.caption, design: .monospaced))
                            .frame(width: 24, alignment: .leading)
                        Text(droid.programName)
                            .font(.system(.caption, design: .monospaced))
                            .frame(width: 140, alignment: .leading)
                        ProgressView(value: Double(droid.charge))
                            .progressViewStyle(.linear)
                            .frame(width: 86)
                        Text(modeName(droid.mode))
                            .font(.system(.caption, design: .monospaced))
                            .foregroundStyle(modeColor(droid.mode))
                            .frame(width: 48, alignment: .leading)
                        Text(energyName(droid.energySource))
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(.secondary)
                            .frame(width: 44, alignment: .leading)
                        Text(droid.feral >= 0.62 ? "feral" : "house")
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(droid.feral >= 0.62 ? .orange : .secondary)
                            .frame(width: 42, alignment: .leading)
                        Text(vmSummary(droid))
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(droid.bytecodeFaulted ? .red : .secondary)
                    }
                }
            }
        }
        .padding(14)
        .foregroundStyle(.white)
        .background(.black.opacity(0.58), in: RoundedRectangle(cornerRadius: 8))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(.white.opacity(0.16), lineWidth: 1)
        )
        .frame(width: 620, alignment: .leading)
    }

    private func metric(_ label: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.system(size: 10, weight: .medium, design: .monospaced))
                .foregroundStyle(.secondary)
            Text(value)
                .font(.system(size: 15, weight: .semibold, design: .monospaced))
        }
    }

    private func modeName(_ mode: UInt8) -> String {
        switch mode {
        case 2: return "return"
        case 3: return "charge"
        case 4: return "empty"
        case 5: return "solar"
        case 6: return "tap"
        default: return "run"
        }
    }

    private func energyName(_ source: UInt8) -> String {
        switch source {
        case 1: return "dock"
        case 2: return "sun"
        case 3: return "wire"
        default: return "cell"
        }
    }

    private func vmSummary(_ droid: EnvDroid) -> String {
        let state = droid.bytecodeFaulted ? "fault" : (droid.bytecodeSteppingEnabled ? "run" : "off")
        return "vm:\(state) \(droid.bytecodeProgramName) #\(droid.bytecodeInstructions)"
    }

    private func modeColor(_ mode: UInt8) -> Color {
        switch mode {
        case 2, 3: return .cyan
        case 5: return .yellow
        case 6: return .orange
        case 4: return .red
        default: return .secondary
        }
    }
}
