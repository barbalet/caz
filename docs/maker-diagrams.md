# Maker Diagrams For The Caz Cat Droid

These diagrams are intentionally maker-style rather than manufacturing drawings. They describe a plausible reference droid that can be simulated now and built later in stages.

## Reference Dimensions

```text
Side view, nominal adult house-cat scale

        camera brow
          ___
   ear  /     \  ear
     \ /  o o  \ /
      |   ^     |             tail cable spine
      |  ---    |____________________
      |         |                    \__
      |  head   |   flexible torso      \___
      |_________|___________________________\~~
          |  |       |  |          |  |
        forelegs   service bay   hindlegs

 nose to haunch: 460 mm
 shoulder height: 240 mm
 target mass: 3.8 kg
 tail length: 280 mm
```

## Top-Level Electronics

```text
                  +----------------------+
                  |      Caz CPU VM      |
                  |  Z80-style bytecode  |
                  +----------+-----------+
                             |
                             | memory-mapped intent via ports
                             |
        +--------------------+--------------------+
        |                                         |
        v                                         v
+---------------+                         +---------------+
| Sensor Bridge |                         | Actuator Bus  |
+-------+-------+                         +-------+-------+
        |                                         |
        | input ports                             | output ports
        |                                         |
 +------+------+------+                  +-------+------+------+
 | eyes | ears | aux  |                  | gait | head | tail |
 +------+------+------+                  +------+------+------+
```

The simulator collapses the sensor bridge and actuator bus into C callbacks. A hardware build would place firmware between real devices and the Caz bytecode VM.

## Head Assembly

```text
Front plate

       left ear servo        right ear servo
             \                   /
              \                 /
           +---\---------------/---+
           |    \             /    |
           |   [mic]       [mic]   |
           |                       |
           |   (cam)       (cam)   |
           |     \         /       |
           |      depth baseline   |
           |                       |
           |       speaker         |
           +-----------+-----------+
                       |
                    yaw servo
                       |
                 neck slip ring
```

Recommended prototype parts:

| Module | Prototype choice | Caz port |
| --- | --- | --- |
| Eyes | Two low-latency camera modules | `EYE_LUMA`, `EYE_MOTION`, `EYE_EDGE`, `EYE_COLOUR_TEMP` |
| Ears | Two MEMS microphones in printed ear shells | `EAR_VOLUME`, `EAR_PITCH`, `EAR_BEARING`, `EAR_PATTERN` |
| Head yaw | Quiet metal-geared servo or smart actuator | `HEAD_YAW` |
| Eyelids | Micro servo or voice-coil shutter | `EYELID` |
| Voice | Small speaker in chest or head | `VOCAL` |

## Body Control Stack

```text
Layered body firmware

+----------------------------------------------------+
| Caz behaviour loop                                 |
| reads eyes/ears, writes gait/head/ear/tail/vocal   |
+---------------------------+------------------------+
                            |
+---------------------------v------------------------+
| Motion interpreter                                  |
| turns GAIT values into leg trajectories             |
+---------------------------+------------------------+
                            |
+---------------------------v------------------------+
| Servo safety layer                                  |
| joint limits, current limits, thermal backoff       |
+---------------------------+------------------------+
                            |
+---------------------------v------------------------+
| Actuators                                           |
| four legs, head yaw, ears, eyelids, tail, speaker   |
+----------------------------------------------------+
```

The simulator currently stops at the top layer. It reports the chosen gait and updates a simple position estimate, but it does not solve leg kinematics.

## Leg Module Sketch

```text
One leg, outside view

      shoulder yaw
          o
          |
       ---+--- carbon side plate
          |
       hip pitch
          o
          |
        femur
          |
       knee pitch
          o
          |
        tibia
          |
       compliant paw pad
        _______
       /_______\
```

Development guidance:

- Keep each paw compliant. The droid should sound like a cat crossing a wooden floor, not a bench tool.
- Put current limits in the lowest firmware layer.
- Make the gait interpreter refuse impossible commands rather than trusting bytecode blindly.
- Treat pounce as a short controlled hop in early prototypes, not a full animal leap.

## Port Wiring Plan

```text
Inputs to Caz

0x10 EYE_LUMA          camera exposure + histogram
0x11 EYE_MOTION        frame difference energy
0x12 EYE_EDGE          depth/edge confidence
0x13 EYE_COLOUR_TEMP   colour balance estimate
0x20 EAR_VOLUME        directional loudness
0x21 EAR_PITCH         dominant pitch bucket
0x22 EAR_BEARING       left/right time and level difference
0x23 EAR_PATTERN       tiny classifier: prey, human, weather, machine

Outputs from Caz

0x40 GAIT              loaf, walk, crouch, pounce, retreat, paw-test
0x41 HEAD_YAW          0..255
0x42 EAR_POSE          neutral, scan, forward, flat, swivel
0x43 TAIL_POSE         low, curl, wrap, still, question, level, flag, twitch, bottle
0x44 VOCAL             silent, mrrp, chirrup, purr, hiss, meow
0x45 EYELID            0..255
```

## Battery And Service Bay

```text
Bottom access

        +-----------------------------------+
        | foreleg mounts     foreleg mounts |
        |                                   |
        |  +-----------------------------+  |
        |  | battery sled, latched       |  |
        |  +-----------------------------+  |
        |                                   |
        |  +-----------------------------+  |
        |  | compute + power regulators  |  |
        |  +-----------------------------+  |
        |                                   |
        | hindleg mounts     hindleg mounts |
        +-----------------------------------+
```

Service recommendations:

- Put the battery in a sled that cannot be opened by a child without a tool.
- Keep all sharp linkages behind fur, fabric, or printed covers.
- Provide a hardware motor-disconnect switch.
- Use a visible charging state, but do not make the eyes the only safety indicator.

## Prototype Stages

Stage 1: table simulator.

- Build and run this repository.
- Tune Caz programs against scenarios.
- Add trace-driven tests for branch decisions.

Stage 2: head-on-a-stand.

- Connect cameras, microphones, head yaw, ears, eyelids, and speaker.
- Keep `GAIT` as a logged value only.
- Confirm people can infer the droid mood from ears, eyes, and head.

Stage 3: rolling mule.

- Place the head on a simple low rover base.
- Map `GAIT` to loaf, walk, retreat, and paw-test.
- Do not implement pounce yet.

Stage 4: legged prototype.

- Add compliant legs and a conservative gait interpreter.
- Keep the Caz bytecode unchanged.
- Expand the actuator firmware below the port layer.

Stage 5: rural trial.

- Start in a dry barn aisle.
- Add weather and machine pattern detection.
- Limit speed around livestock, children, and real cats.
