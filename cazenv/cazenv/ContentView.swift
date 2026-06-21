import SwiftUI

private enum DroidFilter: String, CaseIterable, Identifiable {
    case all = "All"
    case low = "Low"
    case charging = "Charge"
    case solar = "Solar"
    case tapping = "Tap"
    case depleted = "Empty"
    case fallback = "Fallback"
    case blocked = "Blocked"

    var id: String { rawValue }
}

struct ContentView: View {
    @StateObject private var runtime = CazEnvRuntime()
    @State private var filter: DroidFilter = .all

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
        let visibleDroids = filteredDroids

        return VStack(alignment: .leading, spacing: 10) {
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
                metric("LOW", "\(runtime.snapshot.droids.filter { $0.charge < 0.20 }.count)")
                metric("TAP", "\(runtime.snapshot.droids.filter { $0.mode == 6 }.count)")
                metric("FALLBACK", "\(runtime.snapshot.droids.filter { $0.navCause == 2 || $0.navCause == 3 }.count)")
                metric("BLOCKED", "\(runtime.snapshot.droids.filter { $0.navStatus == 2 || $0.obstacleState != 0 }.count)")
                metric("VM LOAD", "\(runtime.snapshot.droids.filter { $0.bytecodeLoaded }.count)/\(runtime.snapshot.droids.count)")
                metric("VM RUN", "\(runtime.snapshot.droids.filter { $0.bytecodeSteppingEnabled && !$0.bytecodeHalted && !$0.bytecodeFaulted }.count)")
                metric("NAV", "\(runtime.snapshot.droids.filter { $0.navIntent != 0 }.count)")
            }

            Picker("Filter", selection: $filter) {
                ForEach(DroidFilter.allCases) { item in
                    Text(item.rawValue).tag(item)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .frame(width: 720)

            Divider()
                .background(.white.opacity(0.22))

            if let loadError = runtime.snapshot.loadError {
                Text(loadError)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(.red)
                    .lineLimit(3)
            }

            ScrollView {
                VStack(alignment: .leading, spacing: 5) {
                    ForEach(visibleDroids.prefix(14)) { droid in
                        droidRow(droid)
                    }
                }
            }
            .frame(maxHeight: 360)
        }
        .padding(14)
        .foregroundStyle(.white)
        .background(.black.opacity(0.58), in: RoundedRectangle(cornerRadius: 8))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(.white.opacity(0.16), lineWidth: 1)
        )
        .frame(width: 1040, alignment: .leading)
    }

    private var filteredDroids: [EnvDroid] {
        runtime.snapshot.droids.filter { droid in
            switch filter {
            case .all:
                return true
            case .low:
                return droid.charge < 0.20
            case .charging:
                return droid.mode == 3
            case .solar:
                return droid.mode == 5 || droid.energySource == 2
            case .tapping:
                return droid.mode == 6 || droid.energySource == 3
            case .depleted:
                return droid.mode == 4 || droid.charge <= 0.000001
            case .fallback:
                return droid.navCause == 2 || droid.navCause == 3
            case .blocked:
                return droid.navStatus == 2 || droid.obstacleState != 0 || droid.blockedJunctionCount > 0
            }
        }
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

    private func droidRow(_ droid: EnvDroid) -> some View {
        let nav = navSummary(droid)
        let controls = controlSummary(droid)
        let gains = gainSummary(droid)
        let vm = vmSummary(droid)
        let strategy = droid.feral >= 0.62 ? "feral" : "house"
        let strategyColor: Color = droid.feral >= 0.62 ? .orange : .secondary
        let reflexColor: Color = droid.reflexState == 0 ? .secondary : .yellow
        let blocked = droid.blockedMovementCount > 0 || droid.blockedJunctionCount > 0

        return HStack(spacing: 8) {
            Text(String(format: "%02d", droid.id))
                .font(.system(.caption, design: .monospaced))
                .frame(width: 24, alignment: .leading)
            Text(droid.programName)
                .font(.system(.caption, design: .monospaced))
                .frame(width: 128, alignment: .leading)
            ProgressView(value: Double(droid.charge))
                .progressViewStyle(.linear)
                .frame(width: 76)
            Text(modeName(droid.mode))
                .font(.system(.caption, design: .monospaced))
                .foregroundStyle(modeColor(droid.mode))
                .frame(width: 48, alignment: .leading)
            Text(energyName(droid.energySource))
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(.secondary)
                .frame(width: 44, alignment: .leading)
            Text(strategy)
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(strategyColor)
                .frame(width: 42, alignment: .leading)
            Text(nav)
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(navColor(droid.navCause))
                .frame(width: 86, alignment: .leading)
            Text(gains)
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(gainColor(droid))
                .frame(width: 128, alignment: .leading)
            Text(controls)
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(reflexColor)
                .frame(width: 118, alignment: .leading)
            Text(blocked ? "blk:\(droid.blockedMovementCount)/j\(droid.blockedJunctionCount)" : "clear")
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(blocked ? .red : .secondary)
                .frame(width: 74, alignment: .leading)
            Text(vm)
                .font(.system(.caption2, design: .monospaced))
                .foregroundStyle(droid.bytecodeFaulted ? .red : .secondary)
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

    private func navSummary(_ droid: EnvDroid) -> String {
        "\(navName(droid.navIntent))/\(navStatusName(droid.navStatus)):\(navCauseName(droid.navCause))"
    }

    private func controlSummary(_ droid: EnvDroid) -> String {
        "g\(droid.bytecodeGait) h\(droid.headYaw) s\(droid.skill) r\(droid.reflexState)"
    }

    private func gainSummary(_ droid: EnvDroid) -> String {
        if droid.chargerGain > 0 {
            return String(format: "dock %.3f", droid.chargerGain)
        }
        if droid.navSolarGain > 0 || droid.fallbackSolarGain > 0 {
            return String(format: "sun %.3f/%.3f", droid.navSolarGain, droid.fallbackSolarGain)
        }
        if droid.navTapGain > 0 || droid.fallbackTapGain > 0 {
            return String(format: "tap %.3f/%.3f", droid.navTapGain, droid.fallbackTapGain)
        }
        return String(format: "pass %.3f", droid.passiveSolarGain)
    }

    private func vmSummary(_ droid: EnvDroid) -> String {
        let state: String
        if droid.bytecodeFaulted {
            state = "fault"
        } else if droid.bytecodeHalted {
            state = "halt"
        } else if !droid.bytecodeLoaded {
            state = "unload"
        } else if droid.bytecodeSteppingEnabled {
            state = "run"
        } else {
            state = "off"
        }
        return "vm:\(state) \(droid.bytecodeProgramName) #\(droid.bytecodeInstructions)"
    }

    private func navName(_ intent: UInt8) -> String {
        switch intent {
        case 1: return "chg"
        case 2: return "sun"
        case 3: return "jct"
        default: return "wnd"
        }
    }

    private func navStatusName(_ status: UInt8) -> String {
        switch status {
        case 1: return "run"
        case 2: return "blk"
        case 3: return "dock"
        case 4: return "tap"
        case 5: return "sun"
        default: return "idle"
        }
    }

    private func navCauseName(_ cause: UInt8) -> String {
        switch cause {
        case 1: return "bc"
        case 2: return "sup"
        case 3: return "fail"
        default: return "none"
        }
    }

    private func navColor(_ cause: UInt8) -> Color {
        switch cause {
        case 1: return .cyan
        case 2: return .orange
        case 3: return .red
        default: return .secondary
        }
    }

    private func gainColor(_ droid: EnvDroid) -> Color {
        if droid.fallbackSolarGain > 0 || droid.fallbackTapGain > 0 || droid.navCause == 2 {
            return .orange
        }
        if droid.navCause == 3 {
            return .red
        }
        if droid.chargerGain > 0 || droid.navSolarGain > 0 || droid.navTapGain > 0 {
            return .cyan
        }
        return .secondary
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
