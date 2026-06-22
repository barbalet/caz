# OpenCat Hardware Bridge

The OpenCat bridge is an optional command-line backend that translates the Caz body layer into OpenCat-style serial commands. It is disabled by default. The command-line simulator and CazEnv continue to use the same body state whether the bridge exists or not.

Start with dry-run output:

```sh
./build/caz --program programs/stalk-and-pounce.caz --scenario farmyard --steps 8 --seed 1 --sample-every 4 --opencat-dry-run
```

Dry-run mode prints the skill command and the selected servo move command that would be sent. It never opens a serial device:

```text
opencat tick=0001 model=nybble mode=dry-run status=ready reflex=clear skill_cmd=krest
opencat tick=0001 model=nybble mode=dry-run joint_cmd=m 0 128 1 128 ...
```

Live output is intentionally gated:

```sh
./build/caz \
  --program programs/stalk-and-pounce.caz \
  --scenario farmyard \
  --steps 8 \
  --opencat-live \
  --opencat-serial /dev/tty.usbserial-XXXX \
  --opencat-calibration hardware/opencat-calibration.example
```

Do not use the example file as a real calibration. Copy it, measure the actual joint limits with the robot supported and servos power-limited, then narrow each range before expanding motion. Live mode refuses to start without both `--opencat-serial` and `--opencat-calibration`.

## Calibration File

Calibration files are line-based `key=value` documents:

```text
model=nybble
rate_limit_ticks=6
joint.0=0,80,176
joint.1=1,88,168
```

`model` accepts `nybble`, `bittle`, or `caz-droid`. `rate_limit_ticks` limits how often joint commands can be emitted. Each `joint.N` entry maps a Caz joint slot to an OpenCat servo index and normalized Caz byte range:

```text
joint.<caz_joint_index>=<servo_index>,<minimum>,<maximum>
```

Only enabled joints are emitted. Any enabled joint whose current target is outside its calibrated range is blocked from the command. If every enabled joint is out of range, dry-run mode reports that all joints were blocked.

## Model Differences

Nybble dry-run maps head yaw, head pitch, shoulders, hips, knees, and elbows. Tail, spine, paw spread, and body roll stay inside the Caz body layer until a real calibration file maps them.

Bittle dry-run maps the leg subset only. It leaves the Caz head, tail, spine, paw spread, and body roll slots unbound because those motions differ by build and are easy to over-assume.

`caz-droid` dry-run maps all 16 Caz body slots for future in-house hardware. Treat it as a simulator convention until physical servo geometry exists.

## Safety Notes

Validate dry-run output before selecting a serial device. Keep the bridge disabled for ordinary simulator and CazEnv work.

Use live mode only with a supported robot on a clear bench, unloaded legs, power limiting, and a calibration file measured for that specific build. Servo indices and safe ranges can differ between Nybble, Bittle, and Caz-derived hardware.

Reflexes remain below the program and above the bridge. Low battery, dropped, lifted, balance, and terrain-caution states alter the effective body targets before any hardware-bound command is emitted.
