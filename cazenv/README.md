# CazEnv

CazEnv is a macOS SwiftUI and Metal room simulator for multiple Caz droids. It renders a 30 ft wide, 60 ft long, 15 ft high room with randomized cat furniture, a shared charging station, electrical junction boxes, and 20 LowpolyCAT-based droids running archived `.caz` bytecode.

## Build

```sh
xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

## Environment

- Room: 30 ft wide, 60 ft long, 15 ft high.
- Charging station: 3 ft wide, 3 ft long, 1 ft high, with eight logical charge pads.
- Cat beds: 12 large floor beds, modeled as 2.0 ft x 1.65 ft bolstered ovals.
- Junction boxes: four wall-mounted 1 ft x 1 ft x 1 ft electrical boxes, with two near the floor and two centered 4 ft above the floor. They are tap targets for droids using claw-like scratch and paw-test recovery behavior.
- Cat furniture: four randomized archetypes:
  - Tall tower: about 6 ft high, based on large multi-cat trees.
  - Mid tree: about 42 in high with condo, posts, and upper platform.
  - Compact condo: about 20 in high.
  - Scratch post: about 32 in high with a 16 in square base.

The furniture proportions were based on public product-guide dimensions:

- People: Frisco 42-in cat tree listed as 42 x 23.5 x 21.5 in, and Amazon Basics compact condo listed as 20 x 16 x 16 in.
- The Spruce Pets: Frisco 72-in tree described as a 6 ft tower; SmartCat Ultimate Scratching Post described as 32 in tall with a 16 in square base.

## Asset Pipeline

CazEnv uses `stock/3d_models/LowpolyCAT_fixed.stl` as the runtime droid mesh. Reusable morphology and rig references live under `cazenv/Assets/`:

- `Generated/ideal-domestic-cat-target.stl`: the 9/10 domestic-cat morphology target generated from the stock-image tuning pass.
- `Generated/ideal-domestic-cat-target-preview.png`: orthographic preview of that target mesh.
- `Generated/CazRig.generated.swift`: preserved OpenCat-style 16-DOF rig reference; it is not compiled into CazEnv by default.
- `Generated/nybble-optional-meshes.json`: disabled optional mesh attachment manifest pending source and license records.
- `ThirdParty/OpenCat/simple-opencat-reference.urdf`: local rigging source for `Tools/GenerateRig.swift`.
- `Reference/caz-reference.png`: legacy visual reference image retained for CazEnv-era morphology work.

Regenerate the domestic-cat target with:

```sh
python3 cazenv/Tools/GenerateIdealCatSTL.py
```

Regenerate the preserved rig reference with:

```sh
swift cazenv/Tools/GenerateRig.swift \
  cazenv/Assets/ThirdParty/OpenCat/simple-opencat-reference.urdf \
  cazenv/Assets/Generated/CazRig.generated.swift
```

## Charging Model

The C core owns charge as a normalized 0.0 to 1.0 value. Droids drain energy while moving, and recharge at the station over roughly 10 minutes from empty to full. The station allows eight droids to charge at once. In strict survival runs, charger docking and gain require bytecode `NAV_CHARGER`; hidden supervisor recovery is counted as failure rather than success.

Every Caz also has a passive solar cell. Solar recovery is slow, but it gives isolated droids a path back from danger during long simulations. Droids with stronger feral tendency can request `NAV_SOLAR` to conserve motion while harvesting ambient light. Passive, requested, and fallback solar gains are tracked separately.

More feral droids can also request `NAV_JUNCTION` and route to the nearest junction box. The rendered droid shows claw overlays during this state. Junction tapping is treated as opportunistic latent-electricity access: faster than solar, slower and less orderly than the charging station, and physically limited by claw reach rather than arbitrary distance.

## Survival Experiment

Long-run survival is treated as an experiment that must be rerun after changes to Caz programs, language ports, charge physics, or room behavior.

```sh
make cazenv-probes
make cazenv-regression
make cazenv-stress
make cazenv-movement
make cazenv-survival
make cazenv-survival-long
```

The strict harness runs 20 droids for 14 simulated days across three deterministic seeds. The long target runs a 30-day, five-seed matrix across baseline, low-sun, full-charger, distant-junction, and high-obstacle cases. Runs fail if any droid reaches zero charge, enters `depleted`, ends at zero charge, faults its CPU, or receives supervisor fallback recovery. A passing strict run means the current archived `.caz` bytecode emitted the recovery decisions through survival/navigation ports.

## Movement Rating

CazEnv tracks movement separately from gait commands. Each droid accumulates actual x/z floor displacement in feet, active translation cycles, commanded-but-stalled cycles, spin-without-translation cycles, and the longest continuous translation streak. The app reports a 0-10 movement rating that rewards average displacement and continuous walking distance, then penalizes commands that do not move the droid across the floor.

Use `make cazenv-movement` to rate the archived `.caz` programs across five deterministic CazEnv seeds. The report is intended to catch programs that look busy through gait, head, or yaw changes but do not travel meaningful distance through the room.

The movement-optimized standalone programs are `long-room-prowl.caz`, `sunward-forager.caz`, and `perimeter-stalker.caz`. They deliberately avoid `survival.inc`; each one carries its own charger, solar, and junction recovery paths so CazEnv can test movement quality and energy responsibility as a single behavior.

## House and Feral Behavior

CazEnv models house/feral behavior as a survival tendency rather than a species distinction. Wikipedia describes domestic cats as solitary hunters with claws, strong senses, and flexible social behavior, while feral cats are unowned domestic cats living freely outdoors, generally avoiding human contact and relying on survival behavior around human settlements.

In CazEnv:

- House-oriented droids prefer the charging station and conserve around beds or program routes.
- Feral-oriented droids are more likely to forage by solar and tap junction boxes before returning to the station.
- All droids remain the same LowpolyCAT-based domestic-cat body plan.

## Program Profiles

The app bundles the existing `.caz` archive and assigns each droid one of the survival-participant programs. Each assigned droid has its own VM runtime, loaded bytecode image, output latches, and navigation state. The C environment core owns physics and resource accounting, but recovery intent should come from bytecode `NAV_INTENT` values rather than hidden C profiles.

`programs/survival.inc` is the shared prologue used by part of the archive. It chooses charger, solar, junction, or low-drain waiting paths from `BATTERY`, `ENERGY_SOURCE`, `CHARGER_SLOTS`, `SOLAR_LEVEL`, `JUNCTION_DISTANCE`, `NAV_STATUS`, and `STRATEGY_TENDENCY`. `feral-forager.caz`, `long-room-prowl.caz`, `sunward-forager.caz`, and `perimeter-stalker.caz` do not include that shared file; their charger, solar, and junction choices are embedded directly in their behavioral control flow and covered by the same strict survival harness.

## Camera

- Two-finger scroll: pan along the room floor.
- Pinch: zoom in/out, including outside the room.
- Rotate gesture: orbit around the room.
- Command-scroll: orbit yaw and pitch.
- Option-scroll: zoom.
- Left drag: pan.
- Right drag: orbit.
