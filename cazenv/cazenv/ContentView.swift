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
    case lowMotion = "Still"

    var id: String { rawValue }
}

struct ContentView: View {
    @StateObject private var runtime = CazEnvRuntime()
    @State private var filter: DroidFilter = .all

    var body: some View {
        GeometryReader { geometry in
            if geometry.size.width < 900 {
                VStack(spacing: 0) {
                    MetalEnvironmentView(snapshot: runtime.snapshot)
                        .frame(maxWidth: .infinity, minHeight: 260, maxHeight: .infinity)
                        .background(Color.black)
                        .layoutPriority(1)

                    Divider()
                        .background(.white.opacity(0.12))

                    sidePanel
                        .frame(maxWidth: .infinity)
                        .frame(height: max(320, geometry.size.height * 0.42))
                }
            } else {
                HStack(spacing: 0) {
                    MetalEnvironmentView(snapshot: runtime.snapshot)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                        .background(Color.black)
                        .layoutPriority(1)

                    Divider()
                        .background(.white.opacity(0.12))

                    sidePanel
                        .frame(width: min(430, max(360, geometry.size.width * 0.35)))
                        .frame(maxHeight: .infinity)
                }
            }
        }
        .background(Color.black)
        .desktopMinimumWindowSize()
    }

    private var sidePanel: some View {
        let visibleDroids = filteredDroids
        let metricColumns = [
            GridItem(.adaptive(minimum: 78), spacing: 10, alignment: .leading)
        ]

        return VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline, spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("CazEnv \(cazEnvVersion)")
                        .font(.system(size: 22, weight: .semibold, design: .rounded))
                    Text("30 x 60 x 15 ft")
                        .font(.system(.caption, design: .monospaced))
                        .foregroundStyle(.secondary)
                    Text("Core \(runtime.snapshot.coreVersion)")
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(.secondary)
                }

                Spacer()

                Button {
                    runtime.reset()
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .buttonStyle(.bordered)
                .help("Reset simulation")
            }

            LazyVGrid(columns: metricColumns, alignment: .leading, spacing: 8) {
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
                metric("MOVE", averageMovementRating)
                metric("STILL", "\(runtime.snapshot.droids.filter { $0.movementCycles > 120 && $0.movementRating < 5.0 }.count)")
            }

            Picker("Filter", selection: $filter) {
                ForEach(DroidFilter.allCases) { item in
                    Text(item.rawValue).tag(item)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .frame(maxWidth: .infinity)

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
                    ForEach(visibleDroids) { droid in
                        droidRow(droid)
                    }
                }
            }
        }
        .padding(16)
        .foregroundStyle(.white)
        .background(Color(red: 0.045, green: 0.052, blue: 0.058))
    }

    private var cazEnvVersion: String {
        guard
            let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String,
            !version.isEmpty,
            version != "$(MARKETING_VERSION)"
        else {
            return runtime.snapshot.coreVersion
        }
        return version
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
            case .lowMotion:
                return droid.movementCycles > 120 && droid.movementRating < 5.0
            }
        }
    }

    private var averageMovementRating: String {
        let droids = runtime.snapshot.droids
        guard !droids.isEmpty else {
            return "0.0"
        }
        let total = droids.reduce(0.0) { $0 + Double($1.movementRating) }
        return String(format: "%.1f", total / Double(droids.count))
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

        return VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                Text(String(format: "%02d", droid.id))
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(.secondary)
                    .frame(width: 24, alignment: .leading)

                Text(droid.programName)
                    .font(.system(.caption, design: .monospaced))
                    .lineLimit(1)
                    .truncationMode(.middle)

                Spacer(minLength: 8)

                Text(modeName(droid.mode))
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(modeColor(droid.mode))
                    .frame(width: 54, alignment: .trailing)
            }

            HStack(spacing: 8) {
                ProgressView(value: Double(droid.charge))
                    .progressViewStyle(.linear)
                    .frame(width: 112)

                Text(String(format: "%3.0f%%", droid.charge * 100))
                    .font(.system(.caption2, design: .monospaced))
                    .frame(width: 42, alignment: .trailing)

                Text(energyName(droid.energySource))
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(.secondary)
                    .frame(width: 40, alignment: .leading)

                Text(strategy)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(strategyColor)

                Spacer(minLength: 0)
            }

            HStack(spacing: 10) {
                Text(nav)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(navColor(droid.navCause))
                    .lineLimit(1)

                Text(gains)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(gainColor(droid))
                    .lineLimit(1)

                Spacer(minLength: 0)
            }

            HStack(spacing: 10) {
                Text(movementSummary(droid))
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(movementColor(droid.movementRating))
                    .lineLimit(1)

                Spacer(minLength: 0)
            }

            HStack(spacing: 10) {
                Text(controls)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(reflexColor)
                    .lineLimit(1)

                Text(blocked ? "blk:\(droid.blockedMovementCount)/j\(droid.blockedJunctionCount)" : "clear")
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(blocked ? .red : .secondary)
                    .lineLimit(1)

                Text(vm)
                    .font(.system(.caption2, design: .monospaced))
                    .foregroundStyle(droid.bytecodeFaulted ? .red : .secondary)
                    .lineLimit(1)
                    .truncationMode(.middle)
            }
        }
        .padding(.vertical, 7)
        .overlay(alignment: .bottom) {
            Rectangle()
                .fill(.white.opacity(0.08))
                .frame(height: 1)
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

    private func movementSummary(_ droid: EnvDroid) -> String {
        let activePercent = droid.movementCycles > 0
            ? Double(droid.movementActiveCycles) * 100.0 / Double(droid.movementCycles)
            : 0.0
        let stallPercent = droid.movementCommandCycles > 0
            ? Double(droid.movementStallCycles) * 100.0 / Double(droid.movementCommandCycles)
            : 0.0
        let spinPercent = droid.movementCommandCycles > 0
            ? Double(droid.movementSpinCycles) * 100.0 / Double(droid.movementCommandCycles)
            : 0.0
        return String(
            format: "move %.1f %.0fft str %.0fft act %.0f%% stall %.0f%% spin %.0f%%",
            Double(droid.movementRating),
            Double(droid.movementTotalFeet),
            Double(droid.movementLongestStreakFeet),
            activePercent,
            stallPercent,
            spinPercent
        )
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

    private func movementColor(_ rating: Float) -> Color {
        if rating >= 7.0 {
            return .green
        }
        if rating >= 5.0 {
            return .cyan
        }
        if rating >= 3.0 {
            return .yellow
        }
        return .red
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

private extension View {
    @ViewBuilder
    func desktopMinimumWindowSize() -> some View {
#if os(macOS)
        frame(minWidth: 980, minHeight: 620)
#else
        self
#endif
    }
}
