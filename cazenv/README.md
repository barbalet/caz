# CazEnv

CazEnv is a macOS SwiftUI and Metal room simulator for multiple Caz droids. It renders a 30 ft wide, 60 ft long, 15 ft high room with randomized cat furniture, a shared charging station, and 20 LowpolyCAT-based droids.

## Build

```sh
xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

## Environment

- Room: 30 ft wide, 60 ft long, 15 ft high.
- Charging station: 3 ft wide, 3 ft long, 1 ft high, with eight logical charge pads.
- Cat beds: 12 large floor beds, modeled as 2.0 ft x 1.65 ft bolstered ovals.
- Junction boxes: four wall-mounted 1 ft x 1 ft x 1 ft electrical boxes, with two near the floor and two centered 4 ft above the floor.
- Cat furniture: four randomized archetypes:
  - Tall tower: about 6 ft high, based on large multi-cat trees.
  - Mid tree: about 42 in high with condo, posts, and upper platform.
  - Compact condo: about 20 in high.
  - Scratch post: about 32 in high with a 16 in square base.

The furniture proportions were based on public product-guide dimensions:

- People: Frisco 42-in cat tree listed as 42 x 23.5 x 21.5 in, and Amazon Basics compact condo listed as 20 x 16 x 16 in.
- The Spruce Pets: Frisco 72-in tree described as a 6 ft tower; SmartCat Ultimate Scratching Post described as 32 in tall with a 16 in square base.

## Charging Model

The C core owns charge as a normalized 0.0 to 1.0 value. Droids drain energy while moving, enter return-to-charge mode below 22%, and recharge at the station over roughly 10 minutes from empty to full. The station allows eight droids to charge at once; low-charge droids wait and loiter near the station if no slot is free.

Every Caz also has a passive solar cell. Solar recovery is slow, but it gives depleted or isolated droids a path back from failure during long simulations. Droids with stronger feral tendency can enter `solar` mode to conserve motion while harvesting ambient light.

More feral droids can also enter `tap` mode and route to the nearest junction box. The rendered droid shows claw overlays during this state. Junction tapping is treated as opportunistic latent-electricity access: faster than solar, slower and less orderly than the charging station.

## Survival Experiment

Long-run survival is treated as an experiment that must be rerun after changes to Caz programs, language ports, charge physics, or room behavior.

```sh
make cazenv-survival
```

The harness runs 20 droids for 14 simulated days across three deterministic seeds. The run fails if any droid reaches zero charge, enters `depleted`, or ends at zero charge. A passing run means only that the current environment policy survived this test matrix; it does not prove the archived `.caz` programs are fully autonomous until CazEnv executes bytecode through the survival/navigation ports.

## House and Feral Behavior

CazEnv models house/feral behavior as a survival tendency rather than a species distinction. Wikipedia describes domestic cats as solitary hunters with claws, strong senses, and flexible social behavior, while feral cats are unowned domestic cats living freely outdoors, generally avoiding human contact and relying on survival behavior around human settlements.

In CazEnv:

- House-oriented droids prefer the charging station and conserve around beds or program routes.
- Feral-oriented droids are more likely to forage by solar and tap junction boxes before returning to the station.
- All droids remain the same LowpolyCAT-based domestic-cat body plan.

## Program Profiles

The app bundles the existing `.caz` archive and assigns each droid one of those program names. The C environment core currently maps those program assignments to movement profiles so the room can run 20 droids efficiently while keeping `cazenv` separate from the existing command-line VM. `return-to-charge` is treated as a safety override profile whenever charge falls below the configured threshold.

## Camera

- Two-finger scroll: pan along the room floor.
- Pinch: zoom in/out, including outside the room.
- Rotate gesture: orbit around the room.
- Command-scroll: orbit yaw and pitch.
- Option-scroll: zoom.
- Left drag: pan.
- Right drag: orbit.
