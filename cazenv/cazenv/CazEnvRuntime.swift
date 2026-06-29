import Foundation
import SwiftUI

struct EnvFixture: Identifiable {
    let id: Int
    let type: UInt8
    let x: Float
    let y: Float
    let z: Float
    let width: Float
    let length: Float
    let height: Float
    let yaw: Float
}

struct EnvDroid: Identifiable {
    let id: Int
    let x: Float
    let z: Float
    let yaw: Float
    let charge: Float
    let feral: Float
    let solarGain: Float
    let passiveSolarGain: Float
    let navSolarGain: Float
    let fallbackSolarGain: Float
    let chargerGain: Float
    let tapGain: Float
    let navTapGain: Float
    let fallbackTapGain: Float
    let speed: Float
    let movementTotalFeet: Float
    let movementLongestStreakFeet: Float
    let movementCurrentStreakFeet: Float
    let movementRating: Float
    let movementCycles: UInt64
    let movementActiveCycles: UInt64
    let movementCommandCycles: UInt64
    let movementStallCycles: UInt64
    let movementSpinCycles: UInt64
    let program: UInt8
    let gait: UInt8
    let mode: UInt8
    let chargingSlot: UInt8
    let energySource: UInt8
    let programName: String
    let bytecodeProgramName: String
    let bytecodeInstructions: UInt64
    let bytecodeCycles: UInt64
    let bytecodeLoaded: Bool
    let bytecodeSteppingEnabled: Bool
    let bytecodeHalted: Bool
    let bytecodeFaulted: Bool
    let navIntent: UInt8
    let navStatus: UInt8
    let navCause: UInt8
    let skill: UInt8
    let bytecodeGait: UInt8
    let headYaw: UInt8
    let earPose: UInt8
    let tailPose: UInt8
    let vocal: UInt8
    let eyelid: UInt8
    let reflexState: UInt8
    let obstacleState: UInt8
    let navTransitionCount: UInt32
    let blockedMovementCount: UInt32
    let blockedJunctionCount: UInt32
}

struct EnvSnapshot {
    let elapsed: Float
    let coreVersion: String
    let fixtures: [EnvFixture]
    let droids: [EnvDroid]
    let loadError: String?

    static let empty = EnvSnapshot(elapsed: 0, coreVersion: "0.000", fixtures: [], droids: [], loadError: nil)
}

@MainActor
final class CazEnvRuntime: ObservableObject {
    @Published var snapshot: EnvSnapshot = .empty

    private var state = CazEnvState()
    private var timer: Timer?
    private var lastTick = Date()
    private var loadError: String?

    init() {
        caz_env_init(&state, 0x20260621)
        loadPrograms()
        snapshot = makeSnapshot()
        start()
    }

    deinit {
        timer?.invalidate()
    }

    func reset() {
        caz_env_init(&state, UInt32(Date().timeIntervalSince1970))
        loadPrograms()
        snapshot = makeSnapshot()
        lastTick = Date()
    }

    private func loadPrograms() {
        guard let resourceURL = Bundle.main.resourceURL else {
            loadError = "CazEnv failed to locate app resource directory"
            return
        }

        var error = [CChar](repeating: 0, count: 512)
        let ok = resourceURL.withUnsafeFileSystemRepresentation { path -> Bool in
            guard let path else { return false }
            return caz_env_load_programs(&state, path, &error, error.count) != 0
        }
        loadError = ok ? nil : String(cString: error)
    }

    private func start() {
        timer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.step()
            }
        }
    }

    private func step() {
        let now = Date()
        let dt = min(Float(now.timeIntervalSince(lastTick)), 0.1)
        lastTick = now
        caz_env_step(&state, dt)
        snapshot = makeSnapshot()
    }

    private func makeSnapshot() -> EnvSnapshot {
        let fixtureCount = Int(caz_env_fixture_count())
        let droidCount = Int(caz_env_droid_count())

        let fixtures = (0..<fixtureCount).map { index -> EnvFixture in
            var item = CazEnvFixtureSnapshot()
            caz_env_fixture_snapshot(&state, Int32(index), &item)
            return EnvFixture(
                id: index,
                type: item.type,
                x: item.x,
                y: item.y,
                z: item.z,
                width: item.width,
                length: item.length,
                height: item.height,
                yaw: item.yaw
            )
        }

        let droids = (0..<droidCount).map { index -> EnvDroid in
            var item = CazEnvDroidSnapshot()
            caz_env_droid_snapshot(&state, Int32(index), &item)
            let cName = caz_env_program_name(item.program)
            let name = cName.map { String(cString: $0) } ?? "unknown"
            let cBytecodeName = caz_env_bytecode_program_name(&state, Int32(index))
            let bytecodeName = cBytecodeName.map { String(cString: $0) } ?? "unknown"
            return EnvDroid(
                id: index,
                x: item.x,
                z: item.z,
                yaw: item.yaw,
                charge: item.charge,
                feral: item.feral,
                solarGain: item.solar_gain,
                passiveSolarGain: item.passive_solar_gain,
                navSolarGain: item.nav_solar_gain,
                fallbackSolarGain: item.fallback_solar_gain,
                chargerGain: item.charger_gain,
                tapGain: item.tap_gain,
                navTapGain: item.nav_tap_gain,
                fallbackTapGain: item.fallback_tap_gain,
                speed: item.speed,
                movementTotalFeet: item.movement_total_ft,
                movementLongestStreakFeet: item.movement_longest_streak_ft,
                movementCurrentStreakFeet: item.movement_current_streak_ft,
                movementRating: item.movement_rating,
                movementCycles: item.movement_cycles,
                movementActiveCycles: item.movement_active_cycles,
                movementCommandCycles: item.movement_command_cycles,
                movementStallCycles: item.movement_stall_cycles,
                movementSpinCycles: item.movement_spin_cycles,
                program: item.program,
                gait: item.gait,
                mode: item.mode,
                chargingSlot: item.charging_slot,
                energySource: item.energy_source,
                programName: name,
                bytecodeProgramName: bytecodeName,
                bytecodeInstructions: item.bytecode_instructions,
                bytecodeCycles: item.bytecode_cycles,
                bytecodeLoaded: item.bytecode_loaded != 0,
                bytecodeSteppingEnabled: item.bytecode_stepping_enabled != 0,
                bytecodeHalted: item.bytecode_halted != 0,
                bytecodeFaulted: item.bytecode_faulted != 0,
                navIntent: item.nav_intent,
                navStatus: item.nav_status,
                navCause: item.nav_cause,
                skill: item.skill,
                bytecodeGait: item.bytecode_gait,
                headYaw: item.head_yaw,
                earPose: item.ear_pose,
                tailPose: item.tail_pose,
                vocal: item.vocal,
                eyelid: item.eyelid,
                reflexState: item.reflex_state,
                obstacleState: item.obstacle_state,
                navTransitionCount: item.nav_transition_count,
                blockedMovementCount: item.blocked_movement_count,
                blockedJunctionCount: item.blocked_junction_count
            )
        }

        return EnvSnapshot(
            elapsed: state.elapsed_seconds,
            coreVersion: String(cString: caz_env_core_version()),
            fixtures: fixtures,
            droids: droids,
            loadError: loadError
        )
    }
}
