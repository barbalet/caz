# Caz

Caz is a Cat Operating System: a small Z80-inspired virtual machine driving a simulated contemporary house-cat droid. The first version in this repository is deliberately readable. It is not a cycle-perfect Z80 emulator, but it keeps the parts that make an 8-bit animal mind feel tangible: registers, flags, little-endian addresses, jump tables, input ports, output ports, and a looping bytecode program that has to notice the world through eyes and ears before deciding what sort of cat it intends to be.

The simulator is written in C and builds into a single command-line program. It models a cat-sized droid with camera-like eyes, directional microphone ears, a lightweight body controller, tail/ear/head actuators, claws, a vocaliser, and a simple rural or domestic environment. The supplied Caz programs can patrol, stalk, move cautiously, or manage survival through charging, solar recovery, and junction tapping.

The repository also contains CazEnv, the macOS SwiftUI and Metal room simulator for long-run runs with twenty Caz droids sharing beds, cat trees, charger slots, solar energy, and electrical junction boxes.

This project also contains in-universe documentation: a Caz language reference, maker diagrams for the droid body, and early field histories written in the calm, observant tone of a British rural documentary.

## Quick Start

```sh
make
./build/caz
```

Try the other modes:

```sh
./build/caz --program curious-patrol --scenario kitchen --steps 40
./build/caz --program programs/stalk-and-pounce.caz --scenario farmyard --steps 40
./build/caz --program programs/farmyard-caution.caz --scenario hedgerow --steps 40 --sample-every 2
./build/caz --list
```

Run the CazEnv survival checks:

```sh
make cazenv-probes
make cazenv-regression
make cazenv-stress
make cazenv-survival
make cazenv-survival-long
```

For instruction-level tracing:

```sh
./build/caz --program stalk-and-pounce --scenario farmyard --steps 4 --trace
```

## Regression Checklist

Use this short pass before and after body-model changes:

```sh
make
make cazenv-probes
make cazenv-regression
make cazenv-stress
make cazenv-survival
./build/caz --program curious-patrol --scenario kitchen --steps 24 --seed 1 --sample-every 8
./build/caz --program programs/stalk-and-pounce.caz --scenario farmyard --steps 20 --seed 1 --sample-every 5
./build/caz --program programs/farmyard-caution.caz --scenario hedgerow --steps 24 --seed 1 --sample-every 6
./build/caz --program programs/feral-forager.caz --scenario hedgerow --steps 24 --seed 1 --sample-every 6
./build/caz --skills-dir /private/tmp/caz-missing-skills --program programs/stalk-and-pounce.caz --scenario farmyard --steps 4 --seed 1 --sample-every 2
./build/caz --program programs/stalk-and-pounce.caz --scenario farmyard --steps 8 --seed 1 --sample-every 4 --opencat-dry-run
xcodebuild -project cazenv/cazenv.xcodeproj -scheme cazenv -configuration Debug build
```

The compatibility contract is that the original coarse input ports `0x10..0x23` and coarse actuator ports `0x40..0x45` keep their symbols, byte ranges, and user-facing report values. The body layer may add normalized sensors, joint targets, pose frames, skill state, reflex state, and file-backed skill overrides around that contract, but existing `.caz` programs in `programs/` must continue to assemble and run unchanged.

## What The Simulator Does

The command-line binary creates four things:

1. A Caz CPU, which is a Z80-shaped VM with 64 KiB of memory, registers `A F B C D E H L`, `PC`, `SP`, flags, `IN`, `OUT`, jumps, comparisons, arithmetic, and `HALT`.
2. A Caz loader, which reads `.caz` assembly files from `programs/`, assembles them to Z80-style bytecode, and loads them at `0x0100`.
3. A cat droid body, which exposes eyes and ears as input ports and accepts actuator commands through output ports.
4. A world model, which produces household and rural events such as kitchen movement, night parlour rustles, farm machinery, weather, glare, human voices, and prey-like motion.

Each body tick updates the environment, then lets the Caz CPU execute a fixed number of bytecode instructions. The Caz program reads sensor ports and writes actuator ports. The runner prints a line describing what the cat perceived and what posture it chose.

Example output:

```text
tick=0002 farmyard       eyes{luma=139 motion= 56 edge= 98 temp=142} ears{vol= 78 pitch=120 bearing= 19 pattern=weather  } pose{gait=loaf     head=128 ears=swivel   tail=curl      vocal=silent  eyelid=118} state{energy=212 curiosity= 96 comfort=148 xy=(0,0)}
```

## The Cat Droid Model

The reference droid is specified as a contemporary house-cat analogue, not a toy rover. The assumed body is roughly:

- Length: 46 cm nose to haunch, with an additional 28 cm articulated tail.
- Shoulder height: 24 cm.
- Mass: 3.8 kg target, 4.5 kg upper bound with development batteries.
- Locomotion: four quiet leg modules with elastic paw pads.
- Head: two camera eyes, a short-baseline depth estimate, a yaw servo, and an eyelid/aperture actuator.
- Ears: two directional microphone shells mounted on independent swivel servos.
- Tail: expressive counterbalance with nine coarse poses in this simulator.
- Claws: represented through scratch and paw-test behaviours, and used by CazEnv for junction tapping when bytecode requests `NAV_JUNCTION`.
- Solar cell: a low-power recovery surface that lets an isolated droid conserve motion and harvest ambient light.
- Voice: small resonant speaker for purr, meow, chirrup, mrrp, and hiss-like warning tones.

The simulator intentionally keeps the physics simple. The behavioural question comes first: given light, motion, object edge strength, sound volume, pitch, bearing, and recognised pattern, what should the Caz bytecode command next?

## CazEnv Survival Model

CazEnv is the long-run room simulator. It renders and simulates a 30 ft wide, 60 ft long, 15 ft high room containing 20 LowpolyCAT-sized droids, a shared charging station, 12 large cat beds, four cat trees or scratchers, and four wall junction boxes. The charging station is 3 ft wide, 3 ft long, 1 ft high, and exposes eight logical charge pads.

The C core owns room physics, collision, energy accounting, and resource attribution. It should not be the primary survival decision maker in strict runs. Each droid runs `.caz` bytecode through the same loader and VM machinery as the command-line simulator, and recovery is expected to come from bytecode outputs such as `NAV_CHARGER`, `NAV_SOLAR`, and `NAV_JUNCTION`.

Energy sources:

- Battery: normal movement drains charge. Low-battery reflex information is visible to bytecode through `BATTERY`, `ENERGY_SOURCE`, `REFLEX_STATE`, and navigation status ports.
- Charging station: the orderly house-cat recovery path. A droid must request `NAV_CHARGER` and acquire a valid slot before it receives charger gain. A full recharge is modeled at roughly ten minutes.
- Solar: every Caz has a passive solar cell. Requested solar recovery uses `NAV_SOLAR`, low-drain resting or loafing motion, and separate accounting for passive, requested, and fallback solar gain.
- Junction tapping: feral-leaning or emergency recovery can request `NAV_JUNCTION`. The droid approaches a junction box and uses claw-like scratch or paw-test behavior; CazEnv models a physical claw reach so tapping is possible near, but not magically through, a junction.

House-cat and feral-cat behaviour are modeled as strategy tendencies within the same domestic-cat body and program family. A house-oriented Caz prefers formal charging, beds, and orderly waiting near the station. A feral-oriented Caz is more willing to conserve energy, use solar recovery, hide or loiter, and tap junction boxes when the charger is full, distant, or unsafe. The distinction is behavioural adaptation, not a different species.

Archived programs can express survival in two ways. Most use the shared `programs/survival.inc` gate, which checks current energy, charge, charger availability, solar level, junction distance, blocked navigation, and strategy tendency before the program resumes playful or observational behaviour. The standalone-energy programs carry equivalent charger, solar, and junction decisions in their own behavior code and are tested to avoid including `survival.inc`. Survival assignment is tracked separately from renderer-only examples.

CazEnv strict survival gates fail if supervisor fallback hides a missing bytecode decision. The current long gate is:

```sh
make cazenv-survival-long
```

That target runs regression and stress checks first, then runs the 30-day, five-seed matrix across baseline, low-sun, full-charger, distant-junction, and high-obstacle cases. The most recent release gate passed with `matrix-result=PASS failed_cases=0/5 days=30 seeds=5`, zero supervisor fallback, zero CPU faults, zero depletion, and all 20 bytecode runtimes loaded and stepping.

## Caz Memory And Ports

Caz programs are loaded at `0x0100`, matching the traditional CP/M-era habit of leaving low memory for system use. The stack starts at `0xfffe`.

Important input ports:

| Port | Name | Meaning |
| --- | --- | --- |
| `0x10` | `EYE_LUMA` | Overall brightness from 0 to 255. |
| `0x11` | `EYE_MOTION` | Motion energy from frame differences. |
| `0x12` | `EYE_EDGE` | Near-object or edge confidence. |
| `0x13` | `EYE_COLOUR_TEMP` | Warm/cool light estimate. |
| `0x20` | `EAR_VOLUME` | Directional loudness estimate. |
| `0x21` | `EAR_PITCH` | Dominant pitch bucket. |
| `0x22` | `EAR_BEARING` | Approximate sound bearing. |
| `0x23` | `EAR_PATTERN` | Pattern class: silence, prey, human, weather, machine. |

Important output ports:

| Port | Name | Meaning |
| --- | --- | --- |
| `0x40` | `GAIT` | Loaf, walk, crouch, pounce, retreat, or paw-test. |
| `0x41` | `HEAD_YAW` | 0 left, 128 centre, 255 right. |
| `0x42` | `EAR_POSE` | Neutral, scan, forward, flat, swivel. |
| `0x43` | `TAIL_POSE` | Low, curl, wrap, still, question, level, flag, twitch, bottle. |
| `0x44` | `VOCAL` | Silent, mrrp, chirrup, purr, hiss, meow. |
| `0x45` | `EYELID` | Eye aperture. Low values are sleepy; high values are alert. |

The middle-layer body model also names normalized body ranges for IMU, lifted/dropped, battery, terrain, skill state, reflex state, joint target staging, and pose-frame staging. These are visible in the simulator report and available to `.caz` programs through symbolic loader names.

Body and skill ports:

| Port | Name | Meaning |
| --- | --- | --- |
| `0x30` | `IMU_ROLL` | Normalized roll, centred on 128. |
| `0x31` | `IMU_PITCH` | Normalized pitch, centred on 128. |
| `0x32` | `LIFTED` | Non-zero when the body model believes the cat has been picked up. |
| `0x33` | `DROPPED` | Non-zero for a deterministic dropped/impact reflex event. |
| `0x34` | `BATTERY` | Normalized battery/energy reserve. |
| `0x35` | `TERRAIN` | Coarse terrain class from the active scenario. |
| `0x36` | `ENERGY_SOURCE` | `ENERGY_BATTERY`, `ENERGY_CHARGER`, `ENERGY_SOLAR`, or `ENERGY_JUNCTION`. |
| `0x37` | `CHARGER_BEARING` | Approximate bearing to the charging station. |
| `0x38` | `CHARGER_DISTANCE` | Normalized distance to the charging station. |
| `0x39` | `CHARGER_SLOTS` | Free logical charger slots. |
| `0x3a` | `JUNCTION_BEARING` | Approximate bearing to the nearest junction box. |
| `0x3b` | `JUNCTION_DISTANCE` | Normalized distance to the nearest junction box. |
| `0x3c` | `SOLAR_LEVEL` | Ambient solar recovery opportunity. |
| `0x3d` | `STRATEGY_TENDENCY` | House-to-feral tendency; `FERAL_TENDENCY` is an alias. |
| `0x50` | `SKILL` | Active built-in skill request, such as `SKILL_POUNCE` or `SKILL_REST`. |
| `0x51` | `SKILL_ARG` | Optional byte argument for future skill commands. |
| `0x52` | `SKILL_STATUS` | `SKILL_STATUS_IDLE`, `READY`, `RUNNING`, `BLOCKED`, or `REFLEX`. |
| `0x53` | `REFLEX_STATE` | `REFLEX_CLEAR`, `LOW_BATTERY`, `DROPPED`, `LIFTED`, `BALANCE`, or `TERRAIN_CAUTION`. |
| `0x54` | `NAV_INTENT` | Bytecode output: `NAV_WANDER`, `NAV_CHARGER`, `NAV_SOLAR`, or `NAV_JUNCTION`. |
| `0x55` | `NAV_STATUS` | Environment feedback: idle, running, blocked, docked, tapping, or solar. |
| `0x60..0x62` | `JOINT_INDEX`, `JOINT_ANGLE`, `JOINT_COMMIT` | Stage and commit one of 16 normalized joint targets. |
| `0x70..0x74` | `POSE_FRAME_*` | Stage and commit normalized pose-frame values. |

Joint slots are named for `.caz` programs: `JOINT_HEAD_YAW`, `JOINT_HEAD_PITCH`, left/right shoulders, left/right hips, tail base/tip, spine height/curve, left/right knees, left/right elbows, paw spread, and body roll. The body layer clamps each normalized joint to its configured safe range before it becomes an effective simulator target.

The simulator loads C-defined skill defaults first, then reads `.cazskill` files from `skills/` by default. Pass `--skills-dir PATH` to point at another tuning directory. If that directory is missing, the built-in skills remain active; invalid skill files stop startup with a path and line-numbered error.

## Included Caz Programs

The canonical Caz programs live in `programs/` as editable `.caz` source files.

`curious-patrol.caz` is a general indoor cat loop. It retreats from very loud noises, tracks motion, crouches for prey-like rustles, listens in low light, and otherwise walks with a level tail.

`stalk-and-pounce.caz` is a hunting sketch. It stalks prey-like sound with `SKILL_CRAWL`, commits to `SKILL_POUNCE` on strong edge confidence, and falls back to sniffing, caution, recovery, or rest.

`farmyard-caution.caz` is a rural safety routine. It demonstrates machine avoidance, terrain caution, weather sheltering, fatigue, greeting, recovery, and close-object inspection.

`feral-forager.caz` is a standalone-energy feral sketch. It prefers junction tapping and solar foraging, hides from machinery or loud sound, conserves movement when charge is marginal, and only stalks prey when energy is healthy.

`survival.inc` is a shared include rather than a standalone behaviour. Programs call `survival_check` before their main behaviour selection so low-charge recovery stays consistent across the archive.

The archive intentionally excludes standalone utility or fixture programs whose movement rates below 5/10 against actual domestic-cat behavior. The body ports, pose-frame staging, pounce skill, and survival navigation capabilities remain available for the retained cat-like programs.

## How Caz Feels

A Caz program tends to read like a little field notebook:

```asm
listen:
    IN   A,(EAR_PATTERN)
    CP   1              ; prey rustle
    JP   Z,mouse

    IN   A,(EAR_VOLUME)
    CP   190
    JP   NC,alarm

    LD   A,1
    OUT  (GAIT),A       ; walk
    JP   listen
```

The language is spare on purpose. A cat droid should not require a cloud service to decide that a shed door is too loud.

## Documentation

- [Caz language reference](docs/caz-language.md)
- [Maker diagrams](docs/maker-diagrams.md)
- [OpenCat hardware bridge](docs/hardware-bridge.md)
- [Field histories](docs/field-histories.md)
- [CazEnv room simulator](cazenv/README.md)

The docs are written to be useful to two kinds of participant: a programmer extending the VM and a maker imagining the droid as a physical machine.

CazEnv's reusable cat morphology, generated ideal-cat STL, OpenCat-style rig reference, and optional mesh provenance notes live under `cazenv/Assets/`; see [CazEnv room simulator](cazenv/README.md) for the current app surface.

## Project Layout

```text
.
|-- Makefile
|-- README.md
|-- docs/
|   |-- caz-language.md
|   |-- field-histories.md
|   |-- hardware-bridge.md
|   `-- maker-diagrams.md
|-- hardware/
|   `-- opencat-calibration.example
|-- cazenv/
|   |-- Assets/
|   |   |-- Generated/
|   |   |-- Reference/
|   |   `-- ThirdParty/
|   |-- c_core/
|   |-- Tools/
|   |-- cazenv/
|   `-- cazenv.xcodeproj/
|-- programs/
|   |-- curious-patrol.caz
|   |-- farmyard-caution.caz
|   |-- feral-forager.caz
|   |-- stalk-and-pounce.caz
|   `-- survival.inc
|-- skills/
|   |-- balance.cazskill
|   |-- crawl.cazskill
|   |-- pounce.cazskill
|   |-- rest.cazskill
|   |-- scratch.cazskill
|   |-- sit.cazskill
|   |-- sniff.cazskill
|   `-- walk.cazskill
|-- stock/
|   `-- 3d_models/
`-- src/
    |-- caz_body.c
    |-- caz_body.h
    |-- caz_loader.c
    |-- caz_loader.h
    |-- caz_cpu.c
    |-- caz_cpu.h
    |-- caz_droid.c
    |-- caz_droid.h
    |-- caz_opencat.c
    |-- caz_opencat.h
    `-- main.c
```

## Development Notes

The simulator is meant to be extended in layers:

- Add more Z80 instructions when a Caz program actually needs them.
- Add richer sensor channels without changing the CPU core.
- Add new Caz programs as `.caz` files in `programs/`.
- Tune body skills as `.cazskill` files in `skills/`.
- Keep survival behavior bytecode-owned: changes that affect charge, solar, junction tapping, or charger slots should pass `make cazenv-regression`, `make cazenv-stress`, and `make cazenv-survival`.
- Use `make cazenv-survival-long` before claiming long-run CazEnv viability after survival-policy changes.
- Dry-run OpenCat-style hardware output with `--opencat-dry-run`; live serial output requires explicit calibration.
- Extend `src/caz_loader.c` when the language needs another instruction or directive.
- Add scenario generators in `src/caz_droid.c`.
- Replace the simple body model with real kinematics later, while preserving the port contract.

The early rule is: keep the animal legible. When the droid does something odd, the trace should let a participant follow the chain from sensor value, to Caz comparison, to branch, to actuator command.

## Build Targets

```sh
make                         # build build/caz
make run                     # run the curious-patrol demo
make cazenv-probes           # targeted CazEnv ownership/resource probes
make cazenv-regression       # archive, loader, and forced recovery checks
make cazenv-stress           # per-program forced survival scenarios
make cazenv-survival         # strict 14-day/3-seed survival gate
make cazenv-survival-long    # strict 30-day/5-seed matrix gate
make clean                   # remove build output
```

## License

MIT. See [LICENSE](LICENSE).
