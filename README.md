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
./build/caz --list
```

For instruction-level tracing:

```sh
./build/caz --program farmyard-mouser --scenario farmyard --steps 4 --trace
```

## What The Simulator Does

The binary creates three things:

1. A Caz CPU, which is a Z80-shaped VM with 64 KiB of memory, registers `A F B C D E H L`, `PC`, `SP`, flags, `IN`, `OUT`, jumps, comparisons, arithmetic, and `HALT`.
2. A cat droid body, which exposes eyes and ears as input ports and accepts actuator commands through output ports.
3. A world model, which produces household and rural events such as kitchen movement, night parlour rustles, farm machinery, weather, glare, human voices, and prey-like motion.

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

## Included Caz Programs

`curious-patrol` is a general indoor cat loop. It retreats from very loud noises, tracks motion, crouches for prey-like rustles, listens in low light, and otherwise walks with a level tail.

`nap-watch` is a low-energy parlour mode. It keeps the eyelids low, purrs quietly, opens one eye for movement, greets human voices, and startles away from abrupt sound.

`farmyard-mouser` is tuned for rural use. It gives machinery room, treats prey-pattern sounds as a hunting cue, greets human speech, shelters during weather, and narrows its eyes in glare.

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

## Project Layout

```text
.
|-- Makefile
|-- README.md
|-- docs/
|   |-- caz-language.md
|   |-- field-histories.md
|   `-- maker-diagrams.md
`-- src/
    |-- caz_cpu.c
    |-- caz_cpu.h
    |-- caz_droid.c
    |-- caz_droid.h
    |-- caz_programs.c
    |-- caz_programs.h
    `-- main.c
```

## Development Notes

The simulator is meant to be extended in layers:

- Add more Z80 instructions when a Caz program actually needs them.
- Add richer sensor channels without changing the CPU core.
- Add new embedded programs in `src/caz_programs.c`.
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
