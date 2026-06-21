# OpenCat-Inspired Body Middle Layer Plan

This plan adds a Caz body middle layer between the Z80-shaped Caz VM and the rendered or physical cat droid. The goal is to let `.caz` programs think in cat-scale behaviours while still allowing precise joint and pose control when a program needs it.

The middle layer should be inspired by OpenCat's useful separation of concerns: Caz remains the small behavioural brain, while a body controller owns motion primitives, joint safety, pose frames, balance reflexes, sensor normalization, and eventual hardware bridging.

## Core Direction

Current flow:

```text
.caz source -> Caz loader -> Caz VM -> simple droid ports -> simulated body report
```

Target flow:

```text
.caz source
    -> Caz loader
    -> Caz VM
    -> Caz body middle layer
    -> simulation backend / Metal renderer / optional OpenCat bridge
    -> cat droid body state
```

The middle layer should become the single place where high-level cat intent is turned into servo-safe body motion.

## Current Development Stage

Active phase: `CazEnv Bytecode Survival`

Last completed cycle: `Cycle 20: NAV_INTENT Bridge To Room Goals`

Current cycle: `Cycle 21: Charger Docking Under Bytecode Control`

Next development action: make charger docking and queuing depend on bytecode `NAV_CHARGER` requests and charger slot availability, with visible blocked/loitering behaviour when all slots are full.

Progress rule: when the current cycle's exit criteria pass, change that cycle's status to `Done`, update this section to name the next cycle, and mark the next cycle's status as `Current`.

Integrity rule: a cycle marked `Done` can be reopened. If later work shows that an exit criterion was incomplete, too weak, or misleading, update that cycle to `Reopened`, add an entry to the Progress Integrity Ledger, and create or adjust a later corrective cycle.

## Progress Integrity

Progress must be tracked honestly. A completed cycle means the listed exit criteria passed in the current workspace at the time it was marked, not that the design is permanently correct or release-ready.

Status vocabulary:

- `Pending`: planned but not started.
- `Current`: the cycle that should be worked next.
- `Done`: implemented and verified against the cycle's current exit criteria.
- `Reopened`: previously marked done, but later evidence showed missing or incorrect work.
- `Blocked`: cannot proceed without a decision, dependency, or external input.
- `Superseded`: replaced by a later plan entry; keep a note explaining the replacement.

When a misstep is found:

1. Add an entry to the Progress Integrity Ledger with the date, affected cycle, evidence, and correction.
2. Reopen the original cycle if its own exit criteria were not actually met.
3. If the original exit criteria were met but proved too weak, leave the old cycle `Done` and add a new corrective cycle or amend a pending cycle.
4. Record verification commands and measured results, not just intent.

### Progress Integrity Ledger

| Date | Cycle | Finding | Action |
| --- | --- | --- | --- |
| 2026-06-21 | Cycle 12 | The survival harness now passes, but the pass is still environment-supervisor assisted. It is not proof of bytecode-owned survival. | Keep Cycle 12 `Done` because it established the experiment contract and current harness. Track bytecode ownership explicitly in Cycles 13-24 and make strict bytecode-owned survival the default in Cycle 34. |
| 2026-06-21 | Cycle 12 | `Done` reflects the current workspace state. The CazEnv project and `return-to-charge.caz` are still new workspace changes until they are committed or otherwise accepted. | Treat Cycle 12 as implemented in the workspace, not as a release checkpoint. Cycle 35 must record final commands and metrics before the bytecode survival phase is complete. |
| 2026-06-21 | Cycle 14 | Per-droid VM runtimes are assigned and visible, but bytecode images are not loaded yet: `make cazenv-survival` reports `assigned=20 loaded=0 stepping=0 instructions=0`. | Keep Cycle 14 `Done` as runtime-state scaffolding. Cycle 15 remains responsible for actual `.caz` program loading. |
| 2026-06-21 | Cycle 15 | CazEnv now loads bytecode images for all 20 droids. `make cazenv-survival` reports `assigned=20 loaded=20 stepping=0 faulted=0 instructions=0`. | Mark Cycle 15 `Done`. This proves program loading, not bytecode-owned survival; VM stepping remains Cycle 18. |
| 2026-06-21 | Cycle 16 | CazEnv survival/environment ports now return bounded deterministic values. Survival logs show `sample-ports droid=0 battery=97 charger=(bearing=15 distance=113 slots=8) junction=(bearing=128 distance=30) solar=89 energy=0 nav=0/0`. | Mark Cycle 16 `Done` for charger, junction, solar, battery, energy, and nav adapter work. Richer eye, ear, terrain, IMU, lifted, dropped, and reflex sensing remains Cycle 17. |
| 2026-06-21 | Cycle 17 | CazEnv body and behaviour ports now return changing context-derived values. `make cazenv-survival` logs eye, ear, body, terrain, reflex, and survival port samples at start and after simulation. | Mark Cycle 17 `Done`. Terrain remains a low ordinal caution signal because archived programs use `TERRAIN >= 4`; broad object proximity is exposed through `EYE_EDGE`. |
| 2026-06-21 | Cycle 18 | CazEnv now enables loaded VM runtimes with a fixed budget of one bytecode instruction per droid per environment step. A 14-day/3-seed run completed with `stepping=20 halted=0 faulted=0` and seed-0 final `instructions=120960000`. | Mark Cycle 18 `Done`. This proves bounded bytecode execution and deterministic tick order, but the C supervisor still owns movement and energy decisions until Cycles 19-20 bridge VM outputs into behaviour. |
| 2026-06-21 | Cycle 19 | Bytecode output latches now drive normal interpreted motion. Survival logs show `sample-output droid=0 ... gait=4 ... motion=(mode=1 speed=1.25 ...)`, and the renderer consumes bytecode head, tail, scratch/tap, and reflex state. | Mark Cycle 19 `Done`. Normal movement no longer depends on hardcoded per-program speed profiles, though supervisor recovery still has fallback speed overrides until Cycle 24. |
| 2026-06-21 | Cycle 20 | `NAV_INTENT` now maps to charger, solar, and junction goals. The nav probe shows `return-to-charge.caz` producing bytecode-caused recovery: `nav=3 status=4 cause=1`. The default survival run still reports `cause: bytecode-nav=2 supervisor=18`. | Mark Cycle 20 `Done` for bytecode-requested NAV goal execution. Do not treat this as strict bytecode-owned survival; Cycle 24 must remove/fail supervisor fallback and Cycle 25 must add survival prologues to non-recovery programs. |

## Design Rules

- Keep `.caz` programs readable as animal behaviour scripts.
- Preserve the existing eye, ear, and simple actuator ports for compatibility.
- Add richer body ports without requiring every program to describe every servo movement.
- Treat balance, fall recovery, and low-voltage protection as body-level reflexes that override normal behaviour.
- Keep hardware-specific assumptions out of the CPU and loader.
- Let the simulator and CazMac renderer consume the same middle-layer state.
- Use the OpenCat simple URDF as the rigging source for body proportions, joint hierarchy, and joint axes.
- Use Nybble STL parts as visual reference or optional mesh pieces, not as the source of truth for animation.

## Proposed Port Map

Existing ports remain valid.

| Range | Purpose |
| --- | --- |
| `0x10..0x13` | Eye sensors. |
| `0x20..0x23` | Ear sensors. |
| `0x30..0x35` | Body and power sensors. |
| `0x40..0x45` | Existing coarse actuators. |
| `0x50..0x5f` | OpenCat-inspired middle-layer control. |
| `0x60..0x6f` | Pose-frame transfer window. |

### New Sensor Ports

| Port | Symbol | Range | Meaning |
| --- | --- | --- | --- |
| `0x30` | `IMU_ROLL` | `0..255` | Roll angle normalized with `128` as level. |
| `0x31` | `IMU_PITCH` | `0..255` | Pitch angle normalized with `128` as level. |
| `0x32` | `LIFTED` | `0..1` | Body controller believes the droid is lifted. |
| `0x33` | `DROPPED` | `0..1` | Recent impact or drop event. |
| `0x34` | `BATTERY` | `0..255` | Battery estimate, where `255` is full. |
| `0x35` | `TERRAIN` | `0..255` | Combined footing caution signal from paws, IMU, and motion confidence. |

`TERRAIN` is included because terrain caution is part of the target behaviour breadth and should be available without making every Caz program interpret raw IMU noise.

### Middle-Layer Control Ports

| Port | Symbol | Direction | Meaning |
| --- | --- | --- | --- |
| `0x50` | `SKILL` | out | Start a named motion primitive. |
| `0x51` | `SKILL_ARG` | out | Optional speed, intensity, or direction argument for the next skill. |
| `0x52` | `SKILL_STATUS` | in | Current skill state: idle, running, blocked, complete, reflex. |
| `0x53` | `REFLEX_STATE` | in | Active reflex state visible to Caz. |
| `0x54` | `JOINT_INDEX` | out | Select one joint from the 16-DOF body map. |
| `0x55` | `JOINT_ANGLE` | out | Set target angle for the selected joint. |
| `0x56` | `JOINT_COMMIT` | out | Apply the selected joint target. |
| `0x57` | `POSE_FRAME_COMMIT` | out | Apply the buffered 16-DOF pose frame. |
| `0x58` | `POSE_FRAME_TIME` | out | Transition time or blend rate for the next frame. |
| `0x59` | `BODY_MODE` | out | Simulation, hardware bridge, calibration, or disabled body mode. |

### Pose-Frame Transfer Window

The VM still has 8-bit ports, so a 16-DOF frame should be transferred by index and value rather than by creating sixteen permanent actuator ports.

| Port | Symbol | Direction | Meaning |
| --- | --- | --- | --- |
| `0x60` | `POSE_FRAME_INDEX` | out | Select pose slot `0..15`. |
| `0x61` | `POSE_FRAME_VALUE` | out | Write target angle for the selected slot. |
| `0x62` | `POSE_FRAME_FLAGS` | out | Mirror, relative, clamp, or immediate flags. |

The middle layer owns angle conversion. Caz writes normalized values; the body backend converts those into renderer joints, simulated limbs, or physical servo commands.

## Skill Port

`SKILL` should start named motion primitives. The first implementation can use symbolic constants assembled by `caz_loader.c`.

| Symbol | Value | Primitive |
| --- | --- | --- |
| `SKILL_BALANCE` | `1` | Stand level and stabilize. |
| `SKILL_REST` | `2` | Fold into resting or loaf posture. |
| `SKILL_SIT` | `3` | Sit with front legs straight and haunches lowered. |
| `SKILL_WALK` | `4` | Forward walking gait. |
| `SKILL_CRAWL` | `5` | Low, cautious crawl. |
| `SKILL_POUNCE` | `6` | Short forward strike and recovery. |
| `SKILL_SNIFF` | `7` | Head and ear scan with small forward lean. |
| `SKILL_SCRATCH` | `8` | Repeated pawing motion against a surface. |
| `SKILL_GROOM` | `9` | Paw raise and face/side grooming loop. |
| `SKILL_STRETCH` | `10` | Foreleg or hindleg stretch. |
| `SKILL_STARTLE` | `11` | Snap alert, flatten ears, prepare retreat. |
| `SKILL_RECOVER` | `12` | Recover from dropped, tilted, or lifted state. |

Example `.caz` style for the first implementation:

```asm
    LD   A,SKILL_CRAWL
    OUT  (SKILL),A
```

Optional later loader sugar:

```asm
    SKILL crawl
```

The sugar should assemble to the same bytecode shape as `LD A,SKILL_CRAWL` followed by `OUT (SKILL),A`, keeping the CPU simple.

## Joint And Pose-Frame Ports

The joint map should model a 16-DOF cat droid even when a specific backend only exposes fewer joints.

| Index | Joint |
| --- | --- |
| `0` | Head yaw. |
| `1` | Head pitch. |
| `2` | Left ear swivel. |
| `3` | Right ear swivel. |
| `4` | Left front shoulder. |
| `5` | Left front knee. |
| `6` | Right front shoulder. |
| `7` | Right front knee. |
| `8` | Left rear hip. |
| `9` | Left rear knee. |
| `10` | Right rear hip. |
| `11` | Right rear knee. |
| `12` | Tail base yaw. |
| `13` | Tail lift. |
| `14` | Spine arch. |
| `15` | Body height or shoulder compression. |

The initial simulator can store these as normalized `0..255` values. The CazMac renderer can use the same normalized values to animate the cat droid. A future OpenCat bridge can map the relevant subset onto Nybble/Bittle servo indices and ignore or synthesize unsupported joints.

Single joint example:

```asm
    LD   A,12
    OUT  (JOINT_INDEX),A
    LD   A,180
    OUT  (JOINT_ANGLE),A
    LD   A,1
    OUT  (JOINT_COMMIT),A
```

Pose frame example:

```asm
    LD   A,0
    OUT  (POSE_FRAME_INDEX),A
    LD   A,128
    OUT  (POSE_FRAME_VALUE),A

    LD   A,1
    OUT  (POSE_FRAME_INDEX),A
    LD   A,110
    OUT  (POSE_FRAME_VALUE),A

    LD   A,24
    OUT  (POSE_FRAME_TIME),A
    LD   A,1
    OUT  (POSE_FRAME_COMMIT),A
```

## 3D Form And Asset Strategy

CazMac should use a two-source body model:

- Rigging source: OpenCat's simple URDF simulation model. This provides clean proportions, link names, joint hierarchy, origins, axes, and primitive body geometry suitable for a stable renderer and later physics work.
- Visual source: Nybble STL parts. These provide recognizable cat-droid form language, especially head, ears, eyes, nose, mouth, spine, body plates, thighs, shanks, and base pieces.

The URDF should drive animation. The STLs should be treated as surface detail, reference material, or optional per-link meshes after licensing and provenance are checked.

Asset pipeline:

1. Reference or import the OpenCat simple URDF into a CazMac asset source folder.
2. Parse the URDF link and joint tree into an intermediate `CazRig` description.
3. Map URDF joints onto the Caz 16-DOF joint indices.
4. Normalize units, axes, and origins into the CazMac Metal coordinate convention.
5. Generate a procedural fallback mesh from URDF boxes and capsules.
6. Select Nybble STL parts only where they improve the cat silhouette or recognizable hardware form.
7. Convert chosen STL parts into app-friendly assets such as OBJ, USDZ, or baked Metal vertex buffers.
8. Attach converted mesh pieces to the matching rig links.
9. Store source provenance, conversion settings, scale, orientation, and license notes beside generated assets.

Do not animate a whole imported STL as one rigid object. Every visible mesh should either be generated from the rig or attached to a moving link controlled by `JOINT` and `POSE_FRAME`.

## Reflex Priority

Reflexes should live below Caz behaviour and above the backend. They are not optional program logic; they are body survival rules.

Priority order:

1. Low-voltage protection.
2. Dropped or impact recovery.
3. Lifted handling.
4. Balance correction.
5. Terrain caution.
6. Normal Caz skill, joint, and pose commands.

When a reflex is active:

- The middle layer may ignore or defer normal `SKILL`, `JOINT`, and `POSE_FRAME` commands.
- `SKILL_STATUS` should report `reflex`.
- `REFLEX_STATE` should expose the current reason.
- Coarse outputs such as `GAIT`, `TAIL_POSE`, and `VOCAL` may still be accepted if they do not conflict with safety.

Suggested `REFLEX_STATE` values:

| Value | State |
| --- | --- |
| `0` | None. |
| `1` | Balancing. |
| `2` | Lifted. |
| `3` | Dropped. |
| `4` | Low battery. |
| `5` | Terrain caution. |
| `6` | Recovery. |

## Skill Library Format

The skill library should let `.caz` programs call body skills without spelling every servo movement.

Start with C-defined built-ins:

- Add a `CazBodySkill` table in the middle layer.
- Each entry has an ID, name, default duration, interruptibility, and update function.
- The update function writes target joint values into the body state.
- The simulator and CazMac renderer read the resulting body state.

Then add file-backed skill definitions:

```text
skills/
|-- balance.cazskill
|-- rest.cazskill
|-- sit.cazskill
|-- walk.cazskill
|-- crawl.cazskill
|-- pounce.cazskill
|-- sniff.cazskill
`-- scratch.cazskill
```

Suggested `.cazskill` shape:

```ini
name=walk
id=4
duration=32
interruptible=true

[frame 0]
time=0
joints=128,128,128,128,116,140,140,116,112,148,148,112,128,132,126,130

[frame 1]
time=8
joints=128,128,128,128,142,118,118,142,150,110,110,150,126,130,124,136
```

This keeps behavioural `.caz` source small while allowing makers to tune gait and posture files independently.

## Cat Behaviour Breadth

The first behaviour expansion should make the droid feel less like a patrol robot and more like a house cat with rural working instincts.

| Behaviour | Needed signals | Likely skills and outputs |
| --- | --- | --- |
| Grooming | Comfort high, low threat, fatigue rising. | `SKILL_GROOM`, purr, half eyelids. |
| Stretching | Wake transition, comfort high. | `SKILL_STRETCH`, tail lift. |
| Loafing | Low motion, safe soundscape. | `SKILL_REST`, wrapped tail, slow blink. |
| Crouching | Edge or prey confidence high. | `SKILL_CRAWL`, ears forward, low tail. |
| Stalking | Prey sound plus motion. | `SKILL_CRAWL`, head track, silent vocal. |
| Startle | Abrupt loudness or impact. | `SKILL_STARTLE`, ears flat, bottle tail. |
| Recovery | Dropped, lifted, high roll/pitch. | `SKILL_RECOVER`, balance reflex. |
| Play | Motion moderate, human voice, high curiosity. | `SKILL_POUNCE`, chirrup, question tail. |
| Greeting | Human voice close, safe light. | `SKILL_SNIFF`, purr or meow. |
| Avoidance | Machine, hiss, obstacle, terrain caution. | retreat gait, flat ears, low body. |
| Curiosity | Unknown pattern, moderate safety. | `SKILL_SNIFF`, scan ears, slow approach. |
| Fatigue | Battery or energy low. | `SKILL_REST`, reduced eyelid, low actuator rate. |
| Terrain caution | High terrain value, unstable roll/pitch. | paw-test, crawl, balance checks. |

## Priority Order

The work should proceed in vertical cycles. Each cycle must leave the command-line simulator buildable, keep existing `.caz` programs running, and make one visible improvement to the body model.

| Priority | Element | Reason |
| --- | --- | --- |
| `P0` | Regression baseline and body boundary. | Protect the current VM, loader, `.caz` programs, and CazMac bridge before adding motion complexity. |
| `P1` | Middle-layer ports and state. | All later work depends on a stable body contract for sensors, skills, joints, and pose frames. |
| `P2` | Reflex priority and body safety. | Balance, dropped, lifted, and low-voltage handling must exist before expressive motion or hardware bridging. |
| `P3` | `SKILL` port and built-in skills. | Behaviour programs need readable motion primitives before they can feel cat-like. |
| `P4` | `JOINT` and `POSE_FRAME` control. | Direct body control is powerful but should sit behind safety and skill support. |
| `P5` | Cat behaviour breadth in `.caz`. | New behaviours should be written once the body can express them. |
| `P6` | File-backed skill library. | Tunable `.cazskill` files are valuable after built-ins prove the format. |
| `P7` | CazMac asset pipeline and rendering integration. | Visual fidelity should follow the shared body state rather than inventing its own model. |
| `P8` | OpenCat hardware bridge. | Physical output should wait until commands, limits, and reflexes are stable. |

## Development Cycles

### Cycle 0: Baseline And Guardrails

Priority: `P0`

Status: Done

Deliverables:

- Record known-good CLI commands for `farmyard-mouser`, `curious-patrol`, and `nap-watch`.
- Record a known-good CazMac build command.
- Add a short regression checklist to the README or this plan.
- Confirm that old coarse ports still define the compatibility contract.

Exit criteria:

- `make` succeeds.
- Existing `.caz` programs still load from `programs/`.
- CazMac still builds with bundled `.caz` resources.

### Cycle 1: Body Middle-Layer Skeleton

Priority: `P1`

Status: Done

Deliverables:

- Add `src/caz_body.[ch]` with normalized body sensors, 16 joint targets, pose-frame buffer, active skill, skill status, and reflex state.
- Let `CazDroid` own or reference `CazBody` while keeping environment simulation in `src/caz_droid.c`.
- Route existing coarse actuator writes through the body layer.
- Add port names for the new sensor and control ranges.

Exit criteria:

- Existing coarse outputs still print the same user-facing gait, head, ear, tail, vocal, and eyelid values.
- New body state can be inspected from the simulator report without changing any `.caz` program.

### Cycle 2: New Ports And Loader Symbols

Priority: `P1`

Status: Done

Deliverables:

- Add constants for `IMU_ROLL`, `IMU_PITCH`, `LIFTED`, `DROPPED`, `BATTERY`, `TERRAIN`, `SKILL`, `SKILL_ARG`, `SKILL_STATUS`, `REFLEX_STATE`, `JOINT_INDEX`, `JOINT_ANGLE`, `JOINT_COMMIT`, `POSE_FRAME_INDEX`, `POSE_FRAME_VALUE`, `POSE_FRAME_FLAGS`, `POSE_FRAME_TIME`, and `POSE_FRAME_COMMIT`.
- Add loader symbols for the new ports.
- Add loader symbols for `SKILL_BALANCE`, `SKILL_REST`, `SKILL_SIT`, `SKILL_WALK`, `SKILL_CRAWL`, `SKILL_POUNCE`, `SKILL_SNIFF`, `SKILL_SCRATCH`, `SKILL_GROOM`, `SKILL_STRETCH`, `SKILL_STARTLE`, and `SKILL_RECOVER`.
- Add one tiny `.caz` fixture or example that calls a skill by symbol.

Exit criteria:

- Old programs assemble unchanged.
- A new skill-calling program assembles without numeric magic values.
- Unknown ports still fail softly or remain ignored as appropriate.

### Cycle 3: Reflex Engine First

Priority: `P2`

Status: Done

Deliverables:

- Compute normalized `IMU_ROLL`, `IMU_PITCH`, `LIFTED`, `DROPPED`, `BATTERY`, and `TERRAIN` values in the simulator.
- Implement reflex priority order: low voltage, dropped, lifted, balance, terrain caution, normal behaviour.
- Report `SKILL_STATUS=reflex` and a meaningful `REFLEX_STATE` when a reflex overrides normal commands.
- Make reflex overrides deterministic enough for repeatable simulation runs.

Exit criteria:

- A low-battery state blocks or slows unsafe motion.
- A dropped or highly tilted state overrides a requested walk or pounce.
- A normal safe state allows ordinary Caz commands through.

### Cycle 4: Built-In Skill MVP

Priority: `P3`

Status: Done

Deliverables:

- Add a `CazBodySkill` table for built-in skills.
- Implement balance, rest, sit, walk, crawl, pounce, sniff, and scratch first.
- Blend normalized joint targets over time.
- Translate old coarse `GAIT` commands into matching skills where sensible.
- Show active skill in the console report.

Exit criteria:

- A `.caz` program can call `SKILL_WALK`, `SKILL_CRAWL`, `SKILL_POUNCE`, and `SKILL_REST`.
- The simulator visibly reports active skill changes.
- Reflexes can interrupt or block a running skill.

### Cycle 5: Joint And Pose-Frame Control

Priority: `P4`

Status: Done

Deliverables:

- Implement `JOINT_INDEX`, `JOINT_ANGLE`, and `JOINT_COMMIT`.
- Implement `POSE_FRAME_INDEX`, `POSE_FRAME_VALUE`, `POSE_FRAME_FLAGS`, `POSE_FRAME_TIME`, and `POSE_FRAME_COMMIT`.
- Clamp normalized joint values inside body-layer limits.
- Add a pose-frame example program that moves at least head, shoulders, hips, tail, and spine/body-height slots.

Exit criteria:

- A `.caz` program can write one joint target.
- A `.caz` program can buffer and commit a 16-DOF pose frame.
- Unsafe values are clamped before reaching the simulator or renderer.

### Cycle 6: Cat Behaviour Program Expansion

Priority: `P5`

Status: Done

Deliverables:

- Add `programs/loaf-and-groom.caz`.
- Add `programs/stalk-and-pounce.caz`.
- Add `programs/farmyard-caution.caz`.
- Add `programs/greeting-play.caz`.
- Update `farmyard-mouser.caz` to use `SKILL_CRAWL`, `SKILL_POUNCE`, `SKILL_SNIFF`, and `SKILL_REST`.
- Keep one legacy-style program for comparison and regression.

Exit criteria:

- The new programs demonstrate grooming, stretching, loafing, crouching, stalking, startle, recovery, play, greeting, avoidance, curiosity, fatigue, and terrain caution across scenarios.
- Behaviour source remains readable and does not spell out full servo frames unless that is the point of the example.

### Cycle 7: File-Backed Skill Library

Priority: `P6`

Status: Done

Deliverables:

- Add `skills/` with `.cazskill` files for balance, rest, sit, walk, crawl, pounce, sniff, and scratch.
- Parse simple key/value metadata and frame sections.
- Load built-in skills first, then override them with file-backed skills when present.
- Document `.cazskill` in `docs/caz-language.md`.

Exit criteria:

- Removing the `skills/` directory still leaves built-in fallback skills.
- Editing a `.cazskill` file changes the simulator behaviour without changing `.caz` source.
- Invalid skill files report useful loader errors.

### Cycle 8: URDF And STL Asset Pipeline

Priority: `P7`

Status: Done

Deliverables:

- Add a CazMac asset source folder for third-party references and generated local assets.
- Import or reference the OpenCat simple URDF as the rigging source.
- Write a small conversion tool or documented conversion step that extracts link names, joint origins, joint axes, primitive boxes, and capsules.
- Map the imported rig onto the Caz 16-DOF joint map.
- Generate a procedural fallback body from URDF primitives.
- Select Nybble STL parts for recognizable form only where they can attach cleanly to rig links.
- Convert chosen STL pieces into app-friendly assets such as OBJ, USDZ, or baked Metal vertex buffers.
- Record provenance and license notes for every third-party source asset before bundling.

Exit criteria:

- The rig can be loaded or compiled into CazMac without hand-entering every joint.
- The generated fallback body renders without any STL dependency.
- Optional Nybble-derived mesh pieces can be disabled without breaking animation.
- Each imported asset has documented source, scale, orientation, and license status.

### Cycle 9: CazMac Body Rendering

Priority: `P7`

Status: Done

Deliverables:

- Feed active skill, reflex state, and 16-DOF joint values into the Swift/SwiftUI/Metal runtime.
- Animate the URDF-derived rig with the `JOINT` and `POSE_FRAME` layer.
- Attach generated or converted mesh pieces to the appropriate rig links.
- Make rest, sit, walk, crawl, pounce, sniff, and scratch visually distinct.
- Show active skill and reflex state in the left code overlay without hiding source code.
- Keep the droid readable at desktop and smaller window sizes.

Exit criteria:

- CazMac builds.
- The rendered body uses the URDF-derived joint hierarchy rather than a separate renderer-only skeleton.
- The rendered cat droid visibly responds to skill changes.
- Optional Nybble-derived meshes improve the cat silhouette without being required for the rig.
- Reflex states are visible enough for prototyping without turning the app into a debug dashboard.

### Cycle 10: OpenCat Bridge Dry Run

Priority: `P8`

Status: Done

Deliverables:

- Add an optional bridge module that translates body-layer skills and safe joint targets to OpenCat-style serial command strings.
- Provide a dry-run mode that prints commands rather than writing to a serial device.
- Map only calibrated, safe subsets at first.
- Document model-specific differences for Nybble, Bittle, and any future Caz droid hardware.

Exit criteria:

- Dry-run output shows plausible serial commands for skills and selected joints.
- No live hardware output is possible without an explicit command-line option and calibration data.
- Reflexes still override hardware-bound commands.

### Cycle 11: Live Hardware Gate

Priority: `P8`

Status: Done

Deliverables:

- Add serial-device selection only after dry-run output is validated.
- Require calibration limits before enabling servo output.
- Rate-limit joint and pose-frame commands.
- Add a hardware caution section to the docs.

Exit criteria:

- Live output is opt-in.
- Unsafe or uncalibrated joints are blocked.
- The bridge can be disabled without affecting the simulator or CazMac.

## Compatibility Notes

The existing `GAIT`, `HEAD_YAW`, `EAR_POSE`, `TAIL_POSE`, `VOCAL`, and `EYELID` ports should remain as coarse controls. They can be internally translated into skills or joint targets, but old `.caz` programs should continue to run.

The new middle layer should avoid adding new Z80 opcodes at first. The language can gain symbols and optional assembler sugar while preserving the bytecode model.

Third-party body assets should remain optional until their license and redistribution status are documented. The CazMac renderer should always have a procedural URDF-derived fallback so the repo can build and run without bundled STL meshes.

## Acceptance Criteria

- Existing programs in `programs/` still assemble and run.
- A `.caz` program can call `SKILL_WALK`, `SKILL_CRAWL`, `SKILL_POUNCE`, and `SKILL_REST`.
- A `.caz` program can write a single joint target through `JOINT_INDEX`, `JOINT_ANGLE`, and `JOINT_COMMIT`.
- A `.caz` program can buffer and commit a 16-DOF pose frame.
- The simulator exposes `IMU_ROLL`, `IMU_PITCH`, `LIFTED`, `DROPPED`, and `BATTERY`.
- Balance, lifted, dropped, and low-battery reflexes override normal behaviour.
- Documentation explains skill calls, pose frames, reflex priority, and the cat behaviour expansion.
- The CazMac body rig is derived from the OpenCat simple URDF or an equivalent checked-in intermediate generated from it.
- Nybble STL parts are used only as documented visual references or optional converted mesh attachments.
- CazMac renders visible body differences for at least rest, sit, walk, crawl, pounce, sniff, and scratch.

## Next Phase: CazEnv Bytecode Survival

This phase turns CazEnv from an environment-supervised simulator into a bytecode-governed survival experiment. Seven cycles would hide too many integration risks. The work should be split into 24 chronological cycles so each cycle can be run, measured, and reverted independently if it weakens long-term survival.

The target control flow is:

```text
CazEnv room state
    -> per-droid Caz VM input ports
    -> .caz bytecode survival and behaviour policy
    -> NAV_INTENT, SKILL, GAIT, HEAD_YAW, and pose outputs
    -> CazEnv physics, charger, solar, junction, and renderer
    -> survival experiment metrics
```

The C environment may keep hard safety assertions, but it should stop being the primary decision maker. A successful experiment must show that recovery transitions came from `.caz` output through ports such as `NAV_INTENT`, not from hidden C thresholds.

### Bytecode Survival Priorities

| Priority | Element | Reason |
| --- | --- | --- |
| `S0` | Repeatable experiment and failure criteria. | Every change must prove it has not reduced survival. |
| `S1` | Shared VM integration. | CazEnv needs the same bytecode machinery as the CLI and CazMac. |
| `S2` | Per-droid runtime state. | Twenty droids need independent CPUs, programs, outputs, and metrics. |
| `S3` | Environment port adapter. | Bytecode can only own survival if it can sense chargers, solar, junctions, slots, obstacles, and charge. |
| `S4` | Actuator and navigation intent bridge. | CazEnv must obey bytecode outputs through physics rather than hardcoded policy branches. |
| `S5` | Program survival prologues. | Every behaviour program must check energy before playful, house-cat, or feral-cat actions. |
| `S6` | Supervisor demotion. | C hardcoded recovery should become a measured fallback or test failure, not normal operation. |
| `S7` | Long-run confidence. | Survival claims need many seeds, longer durations, and regression artifacts. |

## CazEnv Bytecode Survival Development Cycles

### Cycle 12: Survival Experiment Contract

Priority: `S0`

Status: Done

Deliverables:

- Add a repeatable CazEnv survival harness.
- Make the harness fail when any droid reaches zero charge, enters depleted state, or ends at zero charge.
- Add `make cazenv-survival` as the standard command for the experiment.
- Add survival and navigation language ports: `ENERGY_SOURCE`, `CHARGER_BEARING`, `CHARGER_DISTANCE`, `CHARGER_SLOTS`, `JUNCTION_BEARING`, `JUNCTION_DISTANCE`, `SOLAR_LEVEL`, `NAV_INTENT`, and `NAV_STATUS`.
- Update `programs/return-to-charge.caz` into a reference survival policy.
- Fix the known zero-charge accounting hole in the current CazEnv C supervisor.

Exit criteria:

- `make` succeeds.
- `build/caz --program return-to-charge --steps 8` exercises the new symbols.
- `make cazenv-survival` passes the current 14-day, 3-seed, 20-droid run.
- CazEnv still builds in Xcode.

### Cycle 13: Shared VM Build Integration For CazEnv

Priority: `S1`

Status: Done

Deliverables:

- Add the shared Caz VM, loader, body, and droid source files to the CazEnv Xcode target or a common static library target.
- Avoid copying VM code into CazEnv-specific files.
- Keep CazEnv's room simulation in `cazenv/c_core` and VM/language mechanics in `src/`.
- Add a small compile-time adapter boundary so CazEnv can use the VM without depending on CLI-only `main.c`.

Exit criteria:

- CazEnv links against the shared VM sources.
- The CLI still builds with `make`.
- No duplicate VM implementation exists under `cazenv/`.
- Xcode build succeeds.

### Cycle 14: Per-Droid Bytecode Runtime State

Priority: `S2`

Status: Done

Deliverables:

- Extend each CazEnv droid with a `CazCpu` instance or an equivalent owned runtime wrapper.
- Add assigned program image metadata per droid; actual `.caz` image loading remains Cycle 15.
- Store VM output latch state per droid: `NAV_INTENT`, `SKILL`, `GAIT`, `HEAD_YAW`, `EAR_POSE`, `TAIL_POSE`, `VOCAL`, `EYELID`, and selected joint or pose-frame outputs.
- Preserve deterministic initialization from the CazEnv seed.

Exit criteria:

- All 20 droids can be initialized with independent bytecode runtime state.
- A debug snapshot can report each droid's assigned bytecode program and VM instruction count.
- CazEnv still runs if VM stepping is disabled by a temporary feature flag.

### Cycle 15: Program Loading In CazEnv

Priority: `S2`

Status: Done

Deliverables:

- Load `.caz` programs from the app bundle or repository path in the same naming scheme as the CLI.
- Assign archived programs to the 20 droids deterministically.
- Load `return-to-charge.caz` as an actual bytecode routine rather than just a C profile name.
- Surface load failures with useful program names and paths.

Exit criteria:

- Every CazEnv droid has a loaded bytecode image.
- Missing or invalid program files fail visibly rather than silently falling back to C profiles.
- The program assignment table no longer needs to encode speed and gait as the source of behaviour truth.

Verification:

- `make cazenv-survival` reports `assigned=20 loaded=20 stepping=0 faulted=0 instructions=0` and `survival-result=PASS failed_seeds=0/3`.
- `xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build` succeeds.

### Cycle 16: Environment Sensor Port Adapter

Priority: `S3`

Status: Done

Deliverables:

- Implement CazEnv VM read callbacks for existing body ports and new survival ports.
- Map `BATTERY` to normalized droid charge.
- Map charger bearing, distance, and slots from actual charger geometry and occupancy.
- Map junction bearing and distance from nearest usable junction box.
- Map `SOLAR_LEVEL` from the room's daylight/ambient-light model.
- Map `ENERGY_SOURCE` from actual current recovery state.

Exit criteria:

- A `.caz` program can read CazEnv-specific charger, junction, solar, and battery values.
- Port values are deterministic for a fixed seed and step count.
- Values are bounded to `0..255` and documented when saturated.

Verification:

- `make cazenv-survival` prints sample survival ports with bounded values: `battery=97`, `charger=(bearing=15 distance=113 slots=8)`, `junction=(bearing=128 distance=30)`, `solar=89`, `energy=0`, `nav=0/0`.
- `build/caz --program return-to-charge --steps 8 --sample-every 4` loads and runs the bytecode path.

### Cycle 17: Body And Behaviour Sensor Port Adapter

Priority: `S3`

Status: Done

Deliverables:

- Provide CazEnv readings for eye, ear, terrain, IMU, lifted, dropped, and reflex ports.
- Derive eye and ear values from room activity, nearby droids, fixtures, and simple noise models.
- Derive terrain and collision caution from beds, trees, charger, walls, and other droids.
- Keep readings simple and deterministic before adding richer perception.

Exit criteria:

- Existing archived behaviour programs can run in CazEnv without seeing only constant `0xff` fallback values.
- Motion, edge, sound, and terrain readings change with room context.
- Sensor generation can be inspected in survival logs.

Verification:

- `make cazenv-survival` logs body/perception ports such as `eye=(luma=86 motion=175 edge=111 colour=128)`, `ear=(volume=75 pitch=113 bearing=231 pattern=0)`, and `body=(roll=121 pitch=123 lifted=0 dropped=0 terrain=1 reflex=0)`.
- The same run later logs changed values for droid 0, including `eye=(luma=89 motion=87 edge=39 colour=128)` and `body=(roll=124 pitch=137 lifted=0 dropped=0 terrain=1 reflex=0)`.

### Cycle 18: Bytecode Instruction Budget And Tick Order

Priority: `S2`

Status: Done

Deliverables:

- Define a fixed instruction budget per simulation tick per droid.
- Step each droid's VM before applying movement and energy updates.
- Decide how halted or faulted CPUs are represented in survival metrics.
- Ensure VM stepping cost remains acceptable with 20 droids.

Exit criteria:

- CazEnv can run all 20 VMs for a normal frame without obvious slowdown.
- CPU faults or halted programs are visible in the harness and app overlay.
- Repeated runs with the same seed produce the same result.

Verification:

- `make cazenv-survival` completes the default 14-day/3-seed harness with `survival-result=PASS failed_seeds=0/3`; seed 0 ends with `stepping=20 halted=0 faulted=0 instructions=120960000`.
- Two repeated `build/cazenv-survival 1 1` runs produce identical metrics, including `instructions=8640000` and `survival-result=PASS failed_seeds=0/1`.
- `xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build` succeeds and the app overlay distinguishes `run`, `halt`, `fault`, `off`, and unloaded VM states.

### Cycle 19: Output Port Bridge To CazEnv Motion

Priority: `S4`

Status: Done

Deliverables:

- Convert bytecode `GAIT`, `SKILL`, and `HEAD_YAW` outputs into movement targets or steering.
- Keep the body middle layer's reflex outputs visible to the renderer.
- Preserve CazMac-compatible meanings for coarse outputs.
- Add a debug mode that shows raw output latches beside interpreted motion state.

Exit criteria:

- A simple `.caz` program can cause visible CazEnv movement through output ports alone.
- Movement stops or changes when the program changes `GAIT` or `SKILL`.
- C hardcoded program speed profiles are no longer needed for normal behaviour.

Verification:

- `make cazenv-survival` logs raw output latches beside interpreted motion, including `sample-output droid=0 ... gait=4 ... head=64 ... motion=(mode=1 speed=1.25 ...)`.
- CazEnv snapshots and the Swift overlay now expose `NAV_INTENT`, `NAV_STATUS`, nav cause, raw `GAIT`, `SKILL`, `HEAD_YAW`, `EAR_POSE`, `TAIL_POSE`, `VOCAL`, `EYELID`, reflex state, and nav transition count.
- The renderer uses bytecode head yaw and tail pose, shows scratch/tap claws from bytecode skill/nav status, and tints active reflex states.

### Cycle 20: NAV_INTENT Bridge To Room Goals

Priority: `S4`

Status: Done

Deliverables:

- Implement `NAV_WANDER`, `NAV_CHARGER`, `NAV_SOLAR`, and `NAV_JUNCTION` as bytecode-requested room goals.
- Convert requested goals into targets while still obeying physics, walls, occupancy, and fixture collisions.
- Set `NAV_STATUS` to running, blocked, docked, tapping, solar, or idle based on the executed result.
- Log each transition with its cause: bytecode output, fallback, or failure.

Exit criteria:

- `return-to-charge.caz` can drive a droid toward charging, solar, or junction recovery in CazEnv.
- Bytecode-requested recovery goals take precedence over environment fallback.
- Recovery logs show `NAV_INTENT` as the primary cause.

Verification:

- `make cazenv-survival` runs a `return-to-charge.caz` nav probe that ends with `output=(nav=3 status=4 cause=1 gait=5 skill=8 ...)`, proving bytecode-requested junction recovery reaches tapping status.
- The default 14-day/3-seed survival run passes with `survival-result=PASS failed_seeds=0/3`, `stepping=20 halted=0 faulted=0 instructions=120960000` for seed 0.
- The same run reports `nav: charger=2 solar=0 junction=0 cause: bytecode-nav=2 supervisor=18`, so most recovery is still supervisor fallback. This is expected until Cycles 24-25 and must not be mistaken for strict bytecode-owned survival.

### Cycle 21: Charger Docking Under Bytecode Control

Priority: `S4`

Status: Current

Deliverables:

- Make charger docking happen only after a droid requests `NAV_CHARGER` and reaches the charger.
- Report available slots through `CHARGER_SLOTS`.
- Set `NAV_STATUS=DOCKED` while charging.
- Handle full charger occupancy as blocked or loitering without hidden teleporting.

Exit criteria:

- Droids can queue or loiter when all eight slots are occupied.
- Bytecode can observe slot availability and choose a different strategy.
- Charging from empty still takes about 10 simulated minutes.

### Cycle 22: Solar Recovery Under Bytecode Control

Priority: `S4`

Status: Pending

Deliverables:

- Make enhanced solar recovery happen only after `NAV_SOLAR`.
- Keep passive solar as a very small environmental background value if needed, but measure it separately.
- Stop motion or reduce drain when the bytecode chooses solar conservation.
- Set `NAV_STATUS=SOLAR` while solar recovery is active.

Exit criteria:

- A droid can survive by intentionally entering solar recovery.
- The harness distinguishes passive solar gain from program-requested solar recovery.
- Solar recovery cannot hide a zero-charge event.

### Cycle 23: Junction Tapping Under Bytecode Control

Priority: `S4`

Status: Pending

Deliverables:

- Make junction tapping happen only after `NAV_JUNCTION`.
- Require a reachable junction and appropriate distance before granting tap gain.
- Set `NAV_STATUS=TAPPING` while energy is recovered from a junction.
- Keep claw/scratch animation tied to bytecode skill or nav status.

Exit criteria:

- A droid can intentionally route to and tap a junction.
- Failed or unreachable junction attempts report blocked status.
- Tap energy is credited only when the droid is near a junction.

### Cycle 24: Supervisor Demotion Pass

Priority: `S6`

Status: Pending

Deliverables:

- Remove or disable normal hardcoded low-charge mode selection in `caz_env_step`.
- Keep C code only for physics, energy accounting, actuator execution, and hard safety assertions.
- Add a counter for every time CazEnv uses a fallback not requested by bytecode.
- Make fallback use fail the strict survival experiment.

Exit criteria:

- A strict run fails if droids survive only because of hidden C supervisor decisions.
- The default experiment reports zero supervisor recoveries.
- CazEnv still prevents undefined states such as invalid charge, invalid slots, or out-of-room coordinates.

### Cycle 25: Program Survival Prologue Pattern

Priority: `S5`

Status: Pending

Deliverables:

- Define a standard survival prologue for `.caz` programs.
- Apply the prologue to all archived behaviour programs or introduce assembler include support for shared prologue code.
- Ensure every program checks `BATTERY` before high-drain behaviours.
- Branch to charger, solar, or junction logic before playful or exploratory routines.

Exit criteria:

- Every named program in `programs/` has an energy check path.
- No archived program can pounce, stalk, retreat, or wander indefinitely while below the survival threshold.
- The prologue remains readable in assembly source.

### Cycle 26: Shared Assembly Include Or Macro Support

Priority: `S5`

Status: Pending

Deliverables:

- Add minimal assembler support for shared includes or macros if duplicated prologues become brittle.
- Keep the CPU bytecode unchanged.
- Document include resolution and error reporting.
- Move common survival policy into a shared source fragment only after the direct version is proven.

Exit criteria:

- Program source can reuse survival logic without copy-paste drift.
- Bad include paths report a line-numbered assembler error.
- Existing programs without includes still assemble.

### Cycle 27: Bytecode-Owned Behaviour Regression Suite

Priority: `S0`

Status: Pending

Deliverables:

- Add a command or script that runs every named `.caz` program through the CLI and CazEnv bytecode adapter.
- Check for assembler errors, CPU faults, invalid ports, and halted programs.
- Record expected minimum survival outputs from `return-to-charge.caz`.
- Add regression output compact enough for repeated development cycles.

Exit criteria:

- One command verifies that all archived programs assemble and execute.
- A failed program reports the source file and failure mode.
- The survival experiment depends on this suite or runs it first.

### Cycle 28: Collision And Obstacle Feedback

Priority: `S3`

Status: Pending

Deliverables:

- Add simple collision/obstacle sensing for walls, beds, trees, charger, junctions, and other droids.
- Feed obstacle state into `EYE_EDGE`, `TERRAIN`, or a new documented port if existing ports are too overloaded.
- Let bytecode respond before CazEnv clamps or redirects movement.
- Log blocked movement events.

Exit criteria:

- Programs can detect that a chosen route is blocked.
- The harness reports whether survival failures came from navigation blockage.
- Droids no longer pass through major fixtures during recovery routes.

### Cycle 29: Charger Queue Strategy In Bytecode

Priority: `S5`

Status: Pending

Deliverables:

- Add bytecode logic for low charge when `CHARGER_SLOTS == 0`.
- Choose solar or junction instead of waiting blindly when the queue is risky.
- Introduce a low-drain loiter/rest strategy near the charger when waiting is safe.
- Keep this logic visible in source, not hidden in C.

Exit criteria:

- Full charger scenarios do not cause zero-charge failures.
- Logs show bytecode choosing alternate recovery when slots are unavailable.
- The queue behaviour remains stable with 20 droids.

### Cycle 30: House And Feral Strategy Split In Bytecode

Priority: `S5`

Status: Pending

Deliverables:

- Expose a deterministic house/feral tendency or strategy input to bytecode.
- Make house-oriented programs prefer formal charging and beds.
- Make feral-oriented programs prefer solar conservation, hiding/resting, and junction tapping when safe.
- Keep both strategies genetically the same domestic-cat body and program family.

Exit criteria:

- House and feral behaviour differences are produced by bytecode decisions.
- The renderer and logs show which strategy is active.
- Both strategies pass survival thresholds.

### Cycle 31: Long-Duration Multi-Seed Survival Matrix

Priority: `S7`

Status: Pending

Deliverables:

- Expand `make cazenv-survival` or add a longer target for 30-day and 100-day simulated runs.
- Run more seeds, including stress seeds with crowded charger placement and difficult junction positions.
- Record min charge, supervisor fallback count, recharge count, solar count, tap count, blocked nav count, and CPU fault count.
- Keep the default short target fast enough for frequent development.

Exit criteria:

- Short target remains suitable for every development cycle.
- Long target can be run before declaring a survival improvement complete.
- No run is considered passing if supervisor fallback count is nonzero.

### Cycle 32: Renderer And Overlay Attribution

Priority: `S4`

Status: Pending

Deliverables:

- Show each droid's current program, `NAV_INTENT`, `NAV_STATUS`, battery, and energy source in the CazEnv overlay.
- Distinguish bytecode-requested recovery from fallback recovery.
- Keep the room readable when 20 droids are present.
- Add optional filters for depleted, low battery, charging, solar, and tapping states.

Exit criteria:

- A visual inspection can tell why a droid is moving toward a charger, solar patch, or junction.
- Strict-mode fallback is visible immediately.
- Overlay does not obscure the room simulation.

### Cycle 33: Failure Artifact Capture

Priority: `S7`

Status: Pending

Deliverables:

- On survival failure, write a compact report with seed, step, droid index, program, charge, nav intent, nav status, energy source, position, and last several recovery decisions.
- Optionally serialize a replay seed and step range.
- Keep artifacts out of source control by default.

Exit criteria:

- A failed run gives enough data to reproduce the failure.
- Developers do not need to watch the whole simulation to understand the first survival break.
- Failure artifacts are deterministic for a fixed build and seed.

### Cycle 34: Strict Bytecode Survival Mode As Default

Priority: `S6`

Status: Pending

Deliverables:

- Make strict bytecode-owned survival the default harness mode.
- Keep any legacy supervisor mode behind an explicit compatibility option.
- Update documentation to define "survival" as bytecode-owned, not merely environment-assisted.
- Remove stale wording that implies CazEnv owns normal return-to-charge decisions.

Exit criteria:

- The default experiment fails if `.caz` programs do not issue valid survival outputs.
- Compatibility mode is clearly labeled and not used for success claims.
- Docs and harness output use the same terminology.

### Cycle 35: Release Gate For Bytecode-Owned CazEnv

Priority: `S7`

Status: Pending

Deliverables:

- Run the CLI regression suite, CazEnv strict survival short run, CazEnv strict survival long run, and Xcode build.
- Review logs for supervisor fallback, CPU faults, zero charge, depleted state, blocked navigation, and invalid port usage.
- Update this plan with measured results.
- Mark the bytecode survival phase complete only if all checks pass.

Exit criteria:

- `make` succeeds.
- All named `.caz` programs assemble and execute.
- CazEnv strict survival passes with zero supervisor fallbacks.
- The long survival matrix passes.
- CazEnv builds in Xcode.
- The plan records the exact commands and final survival metrics.
