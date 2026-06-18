import SwiftUI

struct ContentView: View {
    @StateObject private var runtime = CazRuntime()
    private let frameClock = Timer.publish(every: 1.0 / 30.0, on: .main, in: .common).autoconnect()

    var body: some View {
        ZStack(alignment: .leading) {
            MetalCatView(snapshot: runtime.snapshot)
                .ignoresSafeArea()

            LinearGradient(
                colors: [
                    Color.black.opacity(0.78),
                    Color.black.opacity(0.50),
                    Color.black.opacity(0.08),
                    Color.clear
                ],
                startPoint: .leading,
                endPoint: .trailing
            )
            .frame(width: 580)
            .ignoresSafeArea()
            .allowsHitTesting(false)

            CazOverlay(runtime: runtime)
                .frame(width: 430)
                .padding(.leading, 24)
                .padding(.vertical, 24)
        }
        .background(Color.black)
        .onReceive(frameClock) { _ in
            runtime.stepFrame()
        }
    }
}

private struct CazOverlay: View {
    @ObservedObject var runtime: CazRuntime

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            header
            controls
            sensorGrid
            instructionPanel
            codeListing
            Spacer(minLength: 0)
        }
        .padding(18)
        .foregroundStyle(.green)
        .background(Color.black.opacity(0.46))
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(Color.green.opacity(0.28), lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .font(.system(.body, design: .monospaced))
        .shadow(color: .green.opacity(0.18), radius: 18, x: 0, y: 0)
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text("CAZMAC")
                .font(.system(size: 30, weight: .bold, design: .monospaced))
            Text(runtime.snapshot.modeLine)
                .font(.system(size: 11, weight: .medium, design: .monospaced))
                .foregroundStyle(Color.green.opacity(0.72))
        }
    }

    private var controls: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Button {
                    runtime.isRunning.toggle()
                } label: {
                    Image(systemName: runtime.isRunning ? "pause.fill" : "play.fill")
                }
                .help(runtime.isRunning ? "Pause Caz" : "Run Caz")

                Button {
                    runtime.stepInstruction()
                } label: {
                    Image(systemName: "forward.frame.fill")
                }
                .help("Step one Caz instruction")

                Button {
                    runtime.reset()
                } label: {
                    Image(systemName: "arrow.counterclockwise")
                }
                .help("Reset VM and droid")

                Spacer()
            }
            .buttonStyle(.bordered)

            Picker("Program", selection: $runtime.programChoice) {
                ForEach(CazProgramChoice.allCases) { choice in
                    Text(choice.title).tag(choice)
                }
            }
            .pickerStyle(.segmented)

            Picker("World", selection: $runtime.scenarioChoice) {
                ForEach(CazScenarioChoice.allCases) { choice in
                    Text(choice.title).tag(choice)
                }
            }
            .pickerStyle(.menu)

            HStack {
                Image(systemName: "speedometer")
                    .foregroundStyle(Color.green.opacity(0.8))
                Slider(value: $runtime.speed, in: 8...160, step: 8)
                Text("\(Int(runtime.speed))")
                    .frame(width: 42, alignment: .trailing)
            }
            .font(.system(size: 12, design: .monospaced))
        }
        .tint(.green)
    }

    private var sensorGrid: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("EYES  LUMA \(runtime.snapshot.eyeLuma.hex2)  MOT \(runtime.snapshot.eyeMotion.hex2)  EDGE \(runtime.snapshot.eyeEdge.hex2)")
            Text("EARS  VOL  \(runtime.snapshot.earVolume.hex2)  PITCH \(runtime.snapshot.earPitch.hex2)  BRG \(runtime.snapshot.earBearing.hex2)")
            Text("POSE  \(runtime.snapshot.gaitName.uppercased())  HEAD \(runtime.snapshot.headYaw.hex2)  EARS \(runtime.snapshot.earPoseName.uppercased())")
            Text("TAIL  \(runtime.snapshot.tailPoseName.uppercased())  VOCAL \(runtime.snapshot.vocalName.uppercased())  LID \(runtime.snapshot.eyelid.hex2)")
            Text("AF \(runtime.snapshot.af)  PC \(runtime.snapshot.pc)  CYC \(runtime.snapshot.cycles)")
        }
        .font(.system(size: 12, weight: .medium, design: .monospaced))
        .foregroundStyle(Color.green.opacity(0.86))
    }

    private var instructionPanel: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("CURRENT")
                .font(.system(size: 10, weight: .bold, design: .monospaced))
                .foregroundStyle(Color.green.opacity(0.56))
            Text(runtime.snapshot.currentInstruction)
                .font(.system(size: 15, weight: .bold, design: .monospaced))
            ForEach(runtime.recentInstructions.prefix(5), id: \.self) { line in
                Text(line)
                    .foregroundStyle(Color.green.opacity(0.52))
                    .font(.system(size: 11, design: .monospaced))
            }
        }
    }

    private var codeListing: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 3) {
                ForEach(Array(runtime.sourceLines.enumerated()), id: \.offset) { index, line in
                    Text(line)
                        .foregroundStyle(index == runtime.highlightedSourceLine ? Color.white : Color.green.opacity(0.78))
                        .font(.system(size: 11, weight: index == runtime.highlightedSourceLine ? .bold : .regular, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.vertical, index == runtime.highlightedSourceLine ? 2 : 0)
                        .background(index == runtime.highlightedSourceLine ? Color.green.opacity(0.18) : Color.clear)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .scrollIndicators(.hidden)
        .mask(
            LinearGradient(
                stops: [
                    .init(color: .clear, location: 0.0),
                    .init(color: .black, location: 0.06),
                    .init(color: .black, location: 0.92),
                    .init(color: .clear, location: 1.0)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        )
    }
}

private extension UInt8 {
    var hex2: String {
        String(format: "%02X", self)
    }
}
