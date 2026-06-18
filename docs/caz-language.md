# Caz Language Reference

Caz is the small assembly language used by the Cat Operating System. It is based on the parts of the Z80 instruction set that are useful for a compact embodied controller: load a byte, compare it, branch, read a port, write a port, and keep going. The present simulator implements a practical subset rather than every official Z80 opcode.

The design principle is simple: a Caz program should be explainable while the droid is moving. If the cat walks toward a sound, the reason should be visible as a few instructions reading an ear port, comparing a threshold, and writing a gait command.

## Machine Model

| Feature | Value |
| --- | --- |
| Address space | 64 KiB |
| Reset vector | `0x0100` |
| Stack start | `0xfffe` |
| Endianness | Little-endian |
| Primary accumulator | `A` |
| Flags register | `F` |
| General registers | `B C D E H L` |
| Register pairs | `BC DE HL SP PC` |
| I/O model | 8-bit port number, 8-bit value |

The `HL` pair can address memory with `(HL)`. Ports are separate from memory and represent body hardware.

## Flags

The simulator tracks the familiar Z80-style flags:

| Flag | Bit | Meaning |
| --- | --- | --- |
| `S` | `0x80` | Sign bit of the result. |
| `Z` | `0x40` | Result is zero, or comparison was equal. |
| `H` | `0x10` | Half carry or half borrow. |
| `PV` | `0x04` | Parity/overflow, depending on instruction. |
| `N` | `0x02` | Last arithmetic operation was subtraction. |
| `C` | `0x01` | Carry or borrow. |

The most common Caz idiom is `CP n` followed by a conditional jump:

```asm
IN   A,(EAR_VOLUME)
CP   190
JP   NC,too_loud
```

After `CP 190`, `NC` means `A >= 190`, because no borrow was needed.

## Implemented Instruction Subset

The C implementation currently supports:

| Instruction | Opcode family | Notes |
| --- | --- | --- |
| `NOP` | `00` | No operation. |
| `HALT` | `76` | Stop the CPU. Demo programs normally loop forever. |
| `LD r,n` | `06/0e/16/1e/26/2e/36/3e` | Load immediate byte into register or `(HL)`. |
| `LD r,r` | `40..7f` except `76` | Register and `(HL)` moves. |
| `LD rr,nn` | `01/11/21/31` | Load `BC`, `DE`, `HL`, or `SP`. |
| `LD A,(nn)` | `3a` | Read memory into `A`. |
| `LD (nn),A` | `32` | Write `A` to memory. |
| `INC r` | `04/0c/14/1c/24/2c/34/3c` | Increment register or `(HL)`. |
| `DEC r` | `05/0d/15/1d/25/2d/35/3d` | Decrement register or `(HL)`. |
| `ADD A,r` | `80..87` | Add register or `(HL)` to `A`. |
| `ADD A,n` | `c6` | Add immediate to `A`. |
| `SUB r` | `90..97` | Subtract register or `(HL)` from `A`. |
| `SUB n` | `d6` | Subtract immediate from `A`. |
| `AND r`, `AND n` | `a0..a7`, `e6` | Logical and. |
| `XOR r`, `XOR n` | `a8..af`, `ee` | Logical exclusive-or. |
| `OR r`, `OR n` | `b0..b7`, `f6` | Logical or. |
| `CP r`, `CP n` | `b8..bf`, `fe` | Compare without changing `A`. |
| `JP nn` | `c3` | Absolute jump. |
| `JP cc,nn` | `c2/ca/d2/da/e2/ea/f2/fa` | Conditional absolute jump. |
| `JR e` | `18` | Relative jump. |
| `JR cc,e` | `20/28/30/38` | Relative jump on `NZ`, `Z`, `NC`, `C`. |
| `CALL nn` | `cd` | Push return address and jump. |
| `RET` | `c9` | Pop return address. |
| `IN A,(n)` | `db` | Read body/environment port into `A`. |
| `OUT (n),A` | `d3` | Write `A` to an actuator port. |
| `DI`, `EI` | `f3`, `fb` | Accepted as no-ops for now. |

## Sensor Ports

All sensor values are unsigned bytes. Low-level firmware would normally smooth and calibrate them before presenting them to Caz.

| Port | Symbol | Range | Description |
| --- | --- | --- | --- |
| `0x10` | `EYE_LUMA` | 0..255 | Overall scene brightness. |
| `0x11` | `EYE_MOTION` | 0..255 | Frame-difference motion energy. |
| `0x12` | `EYE_EDGE` | 0..255 | Near edge or object confidence. |
| `0x13` | `EYE_COLOUR_TEMP` | 0..255 | Warm/cool lighting estimate. |
| `0x20` | `EAR_VOLUME` | 0..255 | Directional sound pressure estimate. |
| `0x21` | `EAR_PITCH` | 0..255 | Dominant pitch bucket. |
| `0x22` | `EAR_BEARING` | 0..255 | Approximate source bearing. |
| `0x23` | `EAR_PATTERN` | 0..255 | Recognised sound class. |

`EAR_PATTERN` values used by the simulator:

| Value | Meaning |
| --- | --- |
| `0` | Silence or no recognised pattern. |
| `1` | Prey-like rustle. |
| `2` | Human voice. |
| `3` | Weather, rain, or wind. |
| `4` | Machine or tractor. |
| `5` | Unknown. |

## Actuator Ports

| Port | Symbol | Values |
| --- | --- | --- |
| `0x40` | `GAIT` | `0` loaf, `1` walk, `2` crouch, `3` pounce, `4` retreat, `5` paw-test. |
| `0x41` | `HEAD_YAW` | `0` hard left, `128` centre, `255` hard right. |
| `0x42` | `EAR_POSE` | `0` neutral, `1` scan, `2` forward, `3` flat, `4` swivel. |
| `0x43` | `TAIL_POSE` | `0` low, `1` curl, `2` wrap, `3` still, `4` question, `5` level, `6` flag, `7` twitch, `8` bottle. |
| `0x44` | `VOCAL` | `0` silent, `1` mrrp, `2` chirrup, `3` purr, `4` hiss, `5` meow. |
| `0x45` | `EYELID` | Low is sleepy, high is alert. |

## Caz Assembly Style

The current programs live in `programs/*.caz` and are assembled by `src/caz_loader.c` at runtime. The assembly style is:

```asm
; Farmyard mouser sketch
loop:
    IN   A,(EAR_PATTERN)
    CP   4
    JP   Z,machine

    IN   A,(EAR_PATTERN)
    CP   1
    JP   Z,prey

    LD   A,1
    OUT  (GAIT),A
    LD   A,132
    OUT  (HEAD_YAW),A
    JP   loop

machine:
    LD   A,4
    OUT  (GAIT),A
    LD   A,3
    OUT  (EAR_POSE),A
    LD   A,4
    OUT  (VOCAL),A
    JP   loop

prey:
    LD   A,2
    OUT  (GAIT),A
    LD   A,2
    OUT  (EAR_POSE),A
    JP   loop
```

## Behaviour Idioms

Threshold branch:

```asm
IN   A,(EYE_MOTION)
CP   150
JP   NC,track
```

Class match:

```asm
IN   A,(EAR_PATTERN)
CP   2
JP   Z,greet_human
```

Posture command:

```asm
LD   A,3
OUT  (GAIT),A       ; pounce
LD   A,168
OUT  (HEAD_YAW),A   ; right of centre
LD   A,7
OUT  (TAIL_POSE),A  ; twitch
```

Sleepy watch:

```asm
LD   A,0
OUT  (GAIT),A
LD   A,58
OUT  (EYELID),A
LD   A,3
OUT  (VOCAL),A
```

## Adding Instructions

Add opcodes in `src/caz_cpu.c` where the dispatch table decodes the instruction families. Keep new instructions small and add them only when a real Caz routine needs them. The VM is meant to be an embodied controller, not a museum-complete CPU emulator.

## Adding A Program

1. Create a `.caz` file in `programs/`.
2. Add `; name:` and `; description:` metadata comments at the top.
3. Use labels, sensor ports, symbolic actuator values, and ordinary Caz instructions.
4. Run it directly with `./build/caz --program programs/your-program.caz`.
5. If it should be a named built-in option, add it to `CazProgramKind` and the small name table in `src/caz_loader.c`.
6. Run the program with at least two scenarios so thresholds are not tuned to a single world.
