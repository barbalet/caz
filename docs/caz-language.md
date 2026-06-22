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
| `0x30` | `IMU_ROLL` | 0..255 | Normalized body roll, centred on 128. |
| `0x31` | `IMU_PITCH` | 0..255 | Normalized body pitch, centred on 128. |
| `0x32` | `LIFTED` | 0 or 255 | Body has been picked up. |
| `0x33` | `DROPPED` | 0 or 255 | Body is in a dropped or impact reflex event. |
| `0x34` | `BATTERY` | 0..255 | Normalized energy reserve. |
| `0x35` | `TERRAIN` | 0..255 | Coarse terrain class from the simulator scenario. |
| `0x36` | `ENERGY_SOURCE` | 0..3 | Current recovery source: battery, charger, solar, or junction. |
| `0x37` | `CHARGER_BEARING` | 0..255 | Relative bearing to the charging station; `128` is straight ahead. |
| `0x38` | `CHARGER_DISTANCE` | 0..255 | Normalized distance to the charging station; `0` is docked/adjacent. |
| `0x39` | `CHARGER_SLOTS` | 0..8 | Available charger slots. |
| `0x3a` | `JUNCTION_BEARING` | 0..255 | Relative bearing to the nearest usable junction box. |
| `0x3b` | `JUNCTION_DISTANCE` | 0..255 | Normalized distance to the nearest usable junction box. |
| `0x3c` | `SOLAR_LEVEL` | 0..255 | Present solar recovery quality. |

## Survival Contract

`BATTERY` is the normalized charge reserve for a Caz droid. A value of `255` means fully charged, `0` means depleted, and environment simulators should treat values near the low end as a safety condition rather than a normal behaviour choice.

Survival is a bytecode responsibility first and an environment safety net second. A survival-capable program should read `BATTERY`, `CHARGER_*`, `JUNCTION_*`, `SOLAR_LEVEL`, and `CHARGER_SLOTS`, then write `NAV_INTENT` before charge reaches zero.

| Threshold | Rule |
| --- | --- |
| `BATTERY <= 96` | The program must request charger, solar, or junction recovery. |
| Charger selected with slot available | The droid should route to the charging station and occupy one of the eight charging slots. |
| Charger unavailable or obstructed | The program should choose solar conservation or junction tapping if those readings are viable. |
| Charging | Charge rises over time; a full charge from empty takes about 10 minutes. |
| `BATTERY >= 250` | The droid can leave the charger and resume its archived program. |
| `BATTERY == 0` away from recovery | The experiment is considered failed even if the environment can later revive the droid. |

`programs/survival.inc` is the shared survival routine included by most archived behaviors. It requests `NAV_CHARGER` when slots are available, `NAV_SOLAR` when conservation is the safer choice, and `NAV_JUNCTION` when a nearby junction can be tapped. Standalone-energy programs such as `territory-patrol.caz`, `feral-forager.caz`, and `energy-aware-hunter.caz` instead derive the same recovery choices inside their own behavior flow and are tested not to include the shared file. The environment can still enforce last-ditch safety, but a successful long-run experiment should not require that fallback.

CazEnv also gives every Caz a passive solar cell and models opportunistic access to latent electricity. Solar recovery is slow and can revive a depleted droid over long simulations, but depletion counts as a failed survival experiment. More feral-behaving droids can seek wall junction boxes and use claw/tap behavior to recover charge without occupying a formal charging slot.

House and feral behavior are treated as strategy modes of the same domestic-cat body. House-oriented droids favor formal charging and predictable program routes. Feral-oriented droids favor self-sufficient recovery, hiding/resting, solar foraging, and junction tapping before they are forced into normal return-to-charge behavior.

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
| `0x50` | `SKILL` | Built-in skill request: `SKILL_BALANCE`, `SKILL_REST`, `SKILL_SIT`, `SKILL_WALK`, `SKILL_CRAWL`, `SKILL_POUNCE`, `SKILL_SNIFF`, `SKILL_SCRATCH`, `SKILL_GROOM`, `SKILL_STRETCH`, `SKILL_STARTLE`, or `SKILL_RECOVER`. |
| `0x51` | `SKILL_ARG` | Optional byte argument for future skill commands. |
| `0x52` | `SKILL_STATUS` | Current skill status: `SKILL_STATUS_IDLE`, `SKILL_STATUS_READY`, `SKILL_STATUS_RUNNING`, `SKILL_STATUS_BLOCKED`, or `SKILL_STATUS_REFLEX`. |
| `0x53` | `REFLEX_STATE` | Current reflex state: `REFLEX_CLEAR`, `REFLEX_LOW_BATTERY`, `REFLEX_DROPPED`, `REFLEX_LIFTED`, `REFLEX_BALANCE`, or `REFLEX_TERRAIN_CAUTION`. |
| `0x54` | `NAV_INTENT` | Requested navigation strategy: `NAV_WANDER`, `NAV_CHARGER`, `NAV_SOLAR`, or `NAV_JUNCTION`. |
| `0x55` | `NAV_STATUS` | Current navigation result: `NAV_IDLE`, `NAV_RUNNING`, `NAV_BLOCKED`, `NAV_DOCKED`, `NAV_TAPPING`, or `NAV_SOLAR_STATUS`. |
| `0x60` | `JOINT_INDEX` | Select one of 16 normalized joint targets. |
| `0x61` | `JOINT_ANGLE` | Stage a normalized joint target value. |
| `0x62` | `JOINT_COMMIT` | Commit the staged joint value to the selected joint. |
| `0x70` | `POSE_FRAME_INDEX` | Select one of 16 pose-frame values. |
| `0x71` | `POSE_FRAME_VALUE` | Stage a pose-frame value. |
| `0x72` | `POSE_FRAME_FLAGS` | Stage pose-frame flags. |
| `0x73` | `POSE_FRAME_TIME` | Stage pose-frame timing. |
| `0x74` | `POSE_FRAME_COMMIT` | Commit the staged pose-frame value. |

The body layer treats coarse actuator writes as requested commands. Reflexes then compute the effective pose reported by the simulator. Priority is low battery, dropped, lifted, balance, terrain caution, then normal behaviour.

Joint index symbols:

| Value | Symbol |
| --- | --- |
| `0` | `JOINT_HEAD_YAW` |
| `1` | `JOINT_HEAD_PITCH` |
| `2` | `JOINT_LEFT_SHOULDER` |
| `3` | `JOINT_RIGHT_SHOULDER` |
| `4` | `JOINT_LEFT_HIP` |
| `5` | `JOINT_RIGHT_HIP` |
| `6` | `JOINT_TAIL_BASE` |
| `7` | `JOINT_TAIL_TIP` |
| `8` | `JOINT_SPINE_HEIGHT` |
| `9` | `JOINT_SPINE_CURVE` |
| `10` | `JOINT_LEFT_KNEE` |
| `11` | `JOINT_RIGHT_KNEE` |
| `12` | `JOINT_LEFT_ELBOW` |
| `13` | `JOINT_RIGHT_ELBOW` |
| `14` | `JOINT_PAW_SPREAD` |
| `15` | `JOINT_BODY_ROLL` |

`JOINT_ANGLE` and `POSE_FRAME_VALUE` are clamped by the body layer before they become effective joint targets. A pose frame is staged in a 16-slot buffer and copied to the effective target set when `POSE_FRAME_COMMIT` is written.

## Caz Skill Files

The body layer has C-defined skill defaults for `SKILL_BALANCE`, `SKILL_REST`, `SKILL_SIT`, `SKILL_WALK`, `SKILL_CRAWL`, `SKILL_POUNCE`, `SKILL_SNIFF`, `SKILL_SCRATCH`, `SKILL_GROOM`, `SKILL_STRETCH`, `SKILL_STARTLE`, and `SKILL_RECOVER`. At startup the simulator resets to those built-ins, then loads `.cazskill` files from `skills/` or from the path passed with `--skills-dir`.

If the skill directory is missing, startup continues with the built-in skills. If a file is malformed, startup stops and reports the file path, line number, and reason.

Skill files are simple key/value documents. The skill can be inferred from the filename, or written explicitly:

```ini
skill=walk
name=Walk
gait=1
head_yaw=128
ear_pose=1
tail_pose=5
vocal=0
eyelid=172

[frame]
head-yaw=128
head-pitch=128
left-shoulder=102
right-shoulder=154
left-hip=154
right-hip=102
tail-base=146
tail-tip=162
spine-height=136
spine-curve=132
left-knee=116
right-knee=150
left-elbow=150
right-elbow=116
paw-spread=136
body-roll=128
```

The frame section also accepts the compact list form. Values are normalized bytes and are clamped by the body layer's joint limits:

```ini
name=walk
id=4
duration=32
interruptible=true

[frame 0]
time=0
joints=128,128,102,154,154,102,146,162,136,132,116,150,150,116,136,128
```

Metadata keys can use hyphens or underscores. `duration`, `interruptible`, and frame `time` are accepted for forward-compatible tuning files, but the first loader-backed implementation applies a single target frame per skill.

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
