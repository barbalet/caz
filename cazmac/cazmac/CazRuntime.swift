import Foundation

enum CazProgramChoice: String, CaseIterable, Identifiable {
    case curiousPatrol
    case napWatch
    case farmyardMouser
    case skillCycle

    var id: String { rawValue }

    var title: String {
        switch self {
        case .curiousPatrol: return "Patrol"
        case .napWatch: return "Nap"
        case .farmyardMouser: return "Mouser"
        case .skillCycle: return "Skills"
        }
    }

    var cKind: CazProgramKind {
        switch self {
        case .curiousPatrol: return CAZ_PROGRAM_CURIOUS_PATROL
        case .napWatch: return CAZ_PROGRAM_NAP_WATCH
        case .farmyardMouser: return CAZ_PROGRAM_FARMYARD_MOUSER
        case .skillCycle: return CAZ_PROGRAM_FARMYARD_MOUSER
        }
    }

    var fileBaseName: String {
        switch self {
        case .curiousPatrol: return "curious-patrol"
        case .napWatch: return "nap-watch"
        case .farmyardMouser: return "farmyard-mouser"
        case .skillCycle: return "skill-cycle"
        }
    }
}

enum CazScenarioChoice: String, CaseIterable, Identifiable {
    case kitchen
    case farmyard
    case nightParlour
    case hedgerow

    var id: String { rawValue }

    var title: String {
        switch self {
        case .kitchen: return "Kitchen"
        case .farmyard: return "Farmyard"
        case .nightParlour: return "Night"
        case .hedgerow: return "Hedgerow"
        }
    }

    var cScenario: CazScenario {
        switch self {
        case .kitchen: return CAZ_SCENARIO_KITCHEN
        case .farmyard: return CAZ_SCENARIO_FARMYARD
        case .nightParlour: return CAZ_SCENARIO_NIGHT_PARLOUR
        case .hedgerow: return CAZ_SCENARIO_HEDGEROW
        }
    }
}

struct CazSnapshot {
    var tick: UInt64
    var eyeLuma: UInt8
    var eyeMotion: UInt8
    var eyeEdge: UInt8
    var eyeColourTemp: UInt8
    var earVolume: UInt8
    var earPitch: UInt8
    var earBearing: UInt8
    var earPattern: UInt8
    var gait: UInt8
    var headYaw: UInt8
    var earPose: UInt8
    var tailPose: UInt8
    var vocal: UInt8
    var eyelid: UInt8
    var imuRoll: UInt8
    var imuPitch: UInt8
    var lifted: UInt8
    var dropped: UInt8
    var battery: UInt8
    var terrain: UInt8
    var activeSkill: UInt8
    var skillStatus: UInt8
    var reflexState: UInt8
    var jointValues: [UInt8]
    var jointTargets: [UInt8]
    var energy: Int
    var curiosity: Int
    var comfort: Int
    var x: Int
    var y: Int
    var pc: String
    var af: String
    var cycles: String
    var currentInstruction: String
    var gaitName: String
    var earPoseName: String
    var tailPoseName: String
    var vocalName: String
    var patternName: String
    var scenarioName: String
    var activeSkillName: String
    var skillStatusName: String
    var reflexStateName: String

    var modeLine: String {
        "TICK \(String(format: "%04llu", tick))  \(scenarioName.uppercased())  \(activeSkillName.uppercased())  \(skillStatusName.uppercased())"
    }

    static let empty = CazSnapshot(
        tick: 0,
        eyeLuma: 0,
        eyeMotion: 0,
        eyeEdge: 0,
        eyeColourTemp: 0,
        earVolume: 0,
        earPitch: 0,
        earBearing: 0,
        earPattern: 0,
        gait: 0,
        headYaw: 128,
        earPose: 0,
        tailPose: 0,
        vocal: 0,
        eyelid: 128,
        imuRoll: 128,
        imuPitch: 128,
        lifted: 0,
        dropped: 0,
        battery: 255,
        terrain: 0,
        activeSkill: 0,
        skillStatus: 0,
        reflexState: 0,
        jointValues: Array(repeating: 128, count: 16),
        jointTargets: Array(repeating: 128, count: 16),
        energy: 0,
        curiosity: 0,
        comfort: 0,
        x: 0,
        y: 0,
        pc: "0000",
        af: "0000",
        cycles: "0",
        currentInstruction: "----",
        gaitName: "loaf",
        earPoseName: "neutral",
        tailPoseName: "low",
        vocalName: "silent",
        patternName: "silence",
        scenarioName: "farmyard",
        activeSkillName: "none",
        skillStatusName: "idle",
        reflexStateName: "clear"
    )
}

@MainActor
final class CazRuntime: ObservableObject {
    @Published var snapshot: CazSnapshot = .empty
    @Published var recentInstructions: [String] = []
    @Published var isRunning = true
    @Published var speed: Double = 48

    @Published var programChoice: CazProgramChoice = .farmyardMouser {
        didSet { reset() }
    }

    @Published var scenarioChoice: CazScenarioChoice = .farmyard {
        didSet { reset() }
    }

    private var cpu = CazCpu()
    private var droid = CazDroid()
    private var image = CazProgramImage()

    init() {
        reset()
    }

    var sourceLines: [String] {
        programChoice.sourceLines
    }

    var highlightedSourceLine: Int? {
        programChoice.highlightLine(for: snapshot.gait, pattern: snapshot.earPattern, skill: snapshot.activeSkill)
    }

    func reset() {
        recentInstructions.removeAll()
        caz_body_reset_skill_library()
        caz_droid_init(&droid, scenarioChoice.cScenario, 0)
        caz_cpu_init(&cpu, caz_droid_read_port, caz_droid_write_port, &droid)
        loadSelectedProgram()
        snapshot = makeSnapshot()
    }

    private func loadSelectedProgram() {
        guard let url = Bundle.main.url(forResource: programChoice.fileBaseName, withExtension: "caz") else {
            return
        }
        _ = url.withUnsafeFileSystemRepresentation { path in
            guard let path else { return false }
            return caz_loader_load_file(&cpu, path, &image)
        }
    }

    func stepFrame() {
        guard isRunning else { return }
        caz_droid_tick(&droid)
        runInstructions(Int(speed))
        snapshot = makeSnapshot()
    }

    func stepInstruction() {
        if droid.body_ticks == 0 {
            caz_droid_tick(&droid)
        }
        runInstructions(1)
        snapshot = makeSnapshot()
    }

    private func runInstructions(_ count: Int) {
        guard count > 0 else { return }
        for _ in 0..<count where !cpu.halted {
            let line = disassemble(address: cpu.pc)
            let result = caz_cpu_step(&cpu)
            if result < 0 {
                break
            }
            recentInstructions.insert(line, at: 0)
            if recentInstructions.count > 12 {
                recentInstructions.removeLast()
            }
        }
    }

    private func makeSnapshot() -> CazSnapshot {
        let jointValues = (0..<16).map { jointValue(at: $0) }
        let jointTargets = (0..<16).map { jointTarget(at: $0) }
        return CazSnapshot(
            tick: droid.body_ticks,
            eyeLuma: droid.eye_luma,
            eyeMotion: droid.eye_motion,
            eyeEdge: droid.eye_edge,
            eyeColourTemp: droid.eye_colour_temp,
            earVolume: droid.ear_volume,
            earPitch: droid.ear_pitch,
            earBearing: droid.ear_bearing,
            earPattern: droid.ear_pattern,
            gait: droid.gait,
            headYaw: droid.head_yaw,
            earPose: droid.ear_pose,
            tailPose: droid.tail_pose,
            vocal: droid.vocal,
            eyelid: droid.eyelid,
            imuRoll: droid.body.imu_roll,
            imuPitch: droid.body.imu_pitch,
            lifted: droid.body.lifted,
            dropped: droid.body.dropped,
            battery: droid.body.battery,
            terrain: droid.body.terrain,
            activeSkill: droid.body.active_skill,
            skillStatus: droid.body.skill_status,
            reflexState: droid.body.reflex_state,
            jointValues: jointValues,
            jointTargets: jointTargets,
            energy: Int(droid.energy),
            curiosity: Int(droid.curiosity),
            comfort: Int(droid.comfort),
            x: Int(droid.x),
            y: Int(droid.y),
            pc: String(format: "%04X", cpu.pc),
            af: String(format: "%02X%02X", cpu.a, cpu.f),
            cycles: String(format: "%llu", cpu.cycles),
            currentInstruction: disassemble(address: cpu.pc),
            gaitName: cString(caz_gait_name(droid.gait)),
            earPoseName: cString(caz_ear_pose_name(droid.ear_pose)),
            tailPoseName: cString(caz_tail_pose_name(droid.tail_pose)),
            vocalName: cString(caz_vocal_name(droid.vocal)),
            patternName: cString(caz_pattern_name(droid.ear_pattern)),
            scenarioName: cString(caz_scenario_name(droid.scenario)),
            activeSkillName: cString(caz_body_skill_name(droid.body.active_skill)),
            skillStatusName: cString(caz_body_skill_status_name(droid.body.skill_status)),
            reflexStateName: cString(caz_body_reflex_state_name(droid.body.reflex_state))
        )
    }

    private func jointValue(at index: Int) -> UInt8 {
        var body = droid.body
        return caz_body_joint_value(&body, UInt8(index))
    }

    private func jointTarget(at index: Int) -> UInt8 {
        var body = droid.body
        return caz_body_joint_target(&body, UInt8(index))
    }

    private func disassemble(address: UInt16) -> String {
        var buffer = [CChar](repeating: 0, count: 96)
        let bufferCount = buffer.count
        buffer.withUnsafeMutableBufferPointer { pointer in
            _ = caz_cpu_disassemble_at(&cpu, address, pointer.baseAddress, bufferCount)
        }
        return String(cString: buffer)
    }

    private func cString(_ pointer: UnsafePointer<CChar>?) -> String {
        guard let pointer else { return "" }
        return String(cString: pointer)
    }
}
