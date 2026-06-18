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
