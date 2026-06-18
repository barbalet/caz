# Caz

Caz is a Cat Operating System: a small Z80-inspired virtual machine driving a simulated contemporary house-cat droid. The first version in this repository is deliberately readable. It is not a cycle-perfect Z80 emulator, but it keeps the parts that make an 8-bit animal mind feel tangible: registers, flags, little-endian addresses, jump tables, input ports, output ports, and a looping bytecode program that has to notice the world through eyes and ears before deciding what sort of cat it intends to be.

The simulator is written in C and builds into a single command-line program. It models a cat-sized droid with camera-like eyes, directional microphone ears, a lightweight body controller, tail/ear/head actuators, a vocaliser, and a simple rural or domestic environment. The supplied Caz programs can patrol, doze, or work as a farmyard mouser.

This project also contains in-universe documentation: a Caz language reference, maker diagrams for the droid body, and early field histories written in the calm, observant tone of a British rural documentary.

## Quick Start

```sh
make
./build/caz
```

Try the other modes:

```sh
./build/caz --program curious-patrol --scenario kitchen --steps 40
./build/caz --program nap-watch --scenario night-parlour --steps 40
./build/caz --program farmyard-mouser --scenario hedgerow --steps 60 --sample-every 2
./build/caz --program programs/farmyard-mouser.caz --scenario farmyard --steps 40
./build/caz --program programs/loaf-and-groom.caz --scenario night-parlour --steps 40
./build/caz --program programs/stalk-and-pounce.caz --scenario farmyard --steps 40
./build/caz --list
```

For instruction-level tracing:

```sh
./build/caz --program farmyard-mouser --scenario farmyard --steps 4 --trace
```

## Regression Checklist

Use this short pass before and after body-model changes:

```sh
make
./build/caz --program farmyard-mouser --scenario farmyard --steps 24 --seed 1 --sample-every 8
./build/caz --program curious-patrol --scenario kitchen --steps 24 --seed 1 --sample-every 8
./build/caz --program nap-watch --scenario night-parlour --steps 24 --seed 1 --sample-every 8
./build/caz --program programs/skill-pounce.caz --scenario farmyard --steps 70 --seed 1 --sample-every 20
./build/caz --program programs/skill-pounce.caz --scenario hedgerow --steps 10 --seed 1 --sample-every 3
./build/caz --program programs/skill-cycle.caz --scenario farmyard --steps 16 --seed 1 --sample-every 2
./build/caz --program programs/pose-frame.caz --scenario kitchen --steps 10 --seed 1 --sample-every 2
./build/caz --program programs/loaf-and-groom.caz --scenario night-parlour --steps 20 --seed 1 --sample-every 5
./build/caz --program programs/stalk-and-pounce.caz --scenario farmyard --steps 20 --seed 1 --sample-every 5
./build/caz --program programs/farmyard-caution.caz --scenario hedgerow --steps 24 --seed 1 --sample-every 6
./build/caz --program programs/greeting-play.caz --scenario kitchen --steps 20 --seed 1 --sample-every 5
./build/caz --skills-dir /private/tmp/caz-missing-skills --program programs/skill-pounce.caz --scenario farmyard --steps 4 --seed 1 --sample-every 2
xcodebuild -project cazmac/cazmac.xcodeproj -scheme cazmac -configuration Debug -derivedDataPath /private/tmp/cazmac-derived-data CODE_SIGNING_ALLOWED=NO build
```

The compatibility contract is that the original coarse input ports `0x10..0x23` and coarse actuator ports `0x40..0x45` keep their symbols, byte ranges, and user-facing report values. The body layer may add normalized sensors, joint targets, pose frames, skill state, reflex state, and file-backed skill overrides around that contract, but existing `.caz` programs in `programs/` must continue to assemble and run unchanged.

## What The Simulator Does

The binary creates three things:

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
- Voice: small resonant speaker for purr, meow, chirrup, mrrp, and hiss-like warning tones.

The simulator intentionally keeps the physics simple. The behavioural question comes first: given light, motion, object edge strength, sound volume, pitch, bearing, and recognised pattern, what should the Caz bytecode command next?

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
| `0x50` | `SKILL` | Active built-in skill request, such as `SKILL_POUNCE` or `SKILL_REST`. |
| `0x51` | `SKILL_ARG` | Optional byte argument for future skill commands. |
| `0x52` | `SKILL_STATUS` | `SKILL_STATUS_IDLE`, `READY`, `RUNNING`, `BLOCKED`, or `REFLEX`. |
| `0x53` | `REFLEX_STATE` | `REFLEX_CLEAR`, `LOW_BATTERY`, `DROPPED`, `LIFTED`, `BALANCE`, or `TERRAIN_CAUTION`. |
| `0x60..0x62` | `JOINT_INDEX`, `JOINT_ANGLE`, `JOINT_COMMIT` | Stage and commit one of 16 normalized joint targets. |
| `0x70..0x74` | `POSE_FRAME_*` | Stage and commit normalized pose-frame values. |

Joint slots are named for `.caz` programs: `JOINT_HEAD_YAW`, `JOINT_HEAD_PITCH`, left/right shoulders, left/right hips, tail base/tip, spine height/curve, left/right knees, left/right elbows, paw spread, and body roll. The body layer clamps each normalized joint to its configured safe range before it becomes an effective simulator target.

The simulator loads C-defined skill defaults first, then reads `.cazskill` files from `skills/` by default. Pass `--skills-dir PATH` to point at another tuning directory. If that directory is missing, the built-in skills remain active; invalid skill files stop startup with a path and line-numbered error.

## Included Caz Programs

The canonical Caz programs live in `programs/` as editable `.caz` source files.

`curious-patrol.caz` is a general indoor cat loop. It retreats from very loud noises, tracks motion, crouches for prey-like rustles, listens in low light, and otherwise walks with a level tail.

`nap-watch.caz` is a low-energy parlour mode. It keeps the eyelids low, purrs quietly, opens one eye for movement, greets human voices, and startles away from abrupt sound.

`farmyard-mouser.caz` is tuned for rural use. It gives machinery room, treats prey-pattern sounds as a hunting cue, greets human speech, shelters during weather, and narrows its eyes in glare.

`loaf-and-groom.caz` is a quiet indoor routine. It chooses rest, grooming, stretching, greeting, startle, recovery, and fatigue behaviours from body and room signals.

`stalk-and-pounce.caz` is a hunting sketch. It stalks prey-like sound with `SKILL_CRAWL`, commits to `SKILL_POUNCE` on strong edge confidence, and falls back to sniffing, caution, recovery, or rest.

`farmyard-caution.caz` is a rural safety routine. It demonstrates machine avoidance, terrain caution, weather sheltering, fatigue, greeting, recovery, and close-object inspection.

`greeting-play.caz` is a sociable kitchen sketch. It greets human voices, plays with high motion, investigates uncertain objects, startles from abrupt sound, and settles when the room calms.

`skill-pounce.caz` is a small fixture rather than a built-in behaviour. It calls `SKILL_POUNCE` by symbol, stages one joint and pose-frame value, and is useful for checking normal pounce, terrain caution, dropped, and low-battery reflex paths.

`skill-cycle.caz` cycles rest, sit, walk, crawl, pounce, sniff, and scratch skills so the console report and CazMac renderer show distinct body primitives.

`pose-frame.caz` buffers all 16 normalized body slots, commits them as one pose frame, and intentionally writes one unsafe head-yaw value to demonstrate body-layer clamping.

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
- [Field histories](docs/field-histories.md)

The docs are written to be useful to two kinds of participant: a programmer extending the VM and a maker imagining the droid as a physical machine.

CazMac's rig source, generated fallback body, and optional mesh provenance notes live under `cazmac/Assets/`; see [CazMac README](cazmac/README.md) for the conversion command.

## Project Layout

```text
.
|-- Makefile
|-- README.md
|-- docs/
|   |-- caz-language.md
|   |-- field-histories.md
|   `-- maker-diagrams.md
|-- cazmac/
|   |-- Assets/
|   |-- Tools/
|   |-- cazmac/
|   |   |-- CazRig.generated.swift
|   |   |-- CatDroidRenderer.swift
|   |   `-- CazRuntime.swift
|   `-- cazmac.xcodeproj/
|-- programs/
|   |-- curious-patrol.caz
|   |-- farmyard-mouser.caz
|   |-- farmyard-caution.caz
|   |-- greeting-play.caz
|   |-- loaf-and-groom.caz
|   |-- nap-watch.caz
|   |-- pose-frame.caz
|   |-- skill-cycle.caz
|   |-- skill-pounce.caz
|   `-- stalk-and-pounce.caz
|-- skills/
|   |-- balance.cazskill
|   |-- crawl.cazskill
|   |-- pounce.cazskill
|   |-- rest.cazskill
|   |-- scratch.cazskill
|   |-- sit.cazskill
|   |-- sniff.cazskill
|   `-- walk.cazskill
`-- src/
    |-- caz_body.c
    |-- caz_body.h
    |-- caz_loader.c
    |-- caz_loader.h
    |-- caz_cpu.c
    |-- caz_cpu.h
    |-- caz_droid.c
    |-- caz_droid.h
    `-- main.c
```

## Development Notes

The simulator is meant to be extended in layers:

- Add more Z80 instructions when a Caz program actually needs them.
- Add richer sensor channels without changing the CPU core.
- Add new Caz programs as `.caz` files in `programs/`.
- Tune body skills as `.cazskill` files in `skills/`.
- Extend `src/caz_loader.c` when the language needs another instruction or directive.
- Add scenario generators in `src/caz_droid.c`.
- Replace the simple body model with real kinematics later, while preserving the port contract.

The early rule is: keep the animal legible. When the droid does something odd, the trace should let a participant follow the chain from sensor value, to Caz comparison, to branch, to actuator command.

## Build Targets

```sh
make        # build build/caz
make run    # run the farmyard mouser demo
make clean  # remove build output
```

## License

MIT. See [LICENSE](LICENSE).
