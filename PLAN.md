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

## Current Development Stage

Active phase: `CazEnv Bytecode Survival`

Last completed cycle: `Cycle 35: Release Gate For Bytecode-Owned CazEnv`

Current cycle: `None - bytecode survival phase complete in this plan`

Next development action: no remaining planned development cycle. If a new miss is discovered, add it to the Audit-Caught Misses table and create a new corrective cycle before claiming broader release readiness.

Dependency note: Cycles 33, 42, 34, and 35 are complete. A-11 and A-12 are fixed in the current gates, strict bytecode-owned survival is the default harness mode, and Cycle 35 passed the long release gate: `make cazenv-survival-long` reports `matrix-result=PASS failed_cases=0/5 days=30 seeds=5`, with zero supervisor fallback, zero CPU faults, zero depletion, and no failed matrix cases. `make`, `git diff --check`, and the Xcode build also pass.

Progress rule: when the current cycle's exit criteria pass, change that cycle's status to `Done`, update this section to name the next cycle, and mark the next cycle's status as `Current`. If no planned cycle remains, set the current cycle to `None` and keep any future work behind a new audit or phase entry.

Integrity rule: a cycle marked `Done` can be reopened. If later work shows that an exit criterion was incomplete, too weak, or misleading, update that cycle to `Reopened`, add an entry to the Progress Integrity Ledger, and create or adjust a later corrective cycle.

Audit rule: when an audit catches a miss, add it to the Audit-Caught Misses table with evidence and the exact cycle or cycles where it has been added back. Audit-added corrective cycles may appear after the original sequence; if they say they must run before an earlier pending cycle, that dependency takes precedence over simple numeric order.

## Progress Integrity

Progress must be tracked honestly. A completed cycle means the listed exit criteria passed in the current workspace at the time it was marked, not that the design is permanently correct or release-ready.

Status vocabulary:

- `Pending`: planned but not started.
- `Current`: the cycle that should be worked next.
- `Done`: implemented and verified against the cycle's current exit criteria.
- `Reopened`: previously marked done, but later evidence showed missing or incorrect work.
- `Blocked`: cannot proceed without a decision, dependency, or external input.
- `Superseded`: replaced by a later plan entry; keep a note explaining the replacement.

When a misstep is found:

1. Add an entry to the Progress Integrity Ledger with the date, affected cycle, evidence, and correction.
2. Reopen the original cycle if its own exit criteria were not actually met.
3. If the original exit criteria were met but proved too weak, leave the old cycle `Done` and add a new corrective cycle or amend a pending cycle.
4. Record verification commands and measured results, not just intent.

### Progress Integrity Ledger

| Date | Cycle | Finding | Action |
| --- | --- | --- | --- |
| 2026-06-21 | Cycle 12 | The survival harness now passes, but the pass is still environment-supervisor assisted. It is not proof of bytecode-owned survival. | Keep Cycle 12 `Done` because it established the experiment contract and current harness. Track bytecode ownership explicitly in Cycles 13-24 and make strict bytecode-owned survival the default in Cycle 34. |
| 2026-06-21 | Cycle 12 | `Done` reflects the current workspace state. The CazEnv project and `return-to-charge.caz` are still new workspace changes until they are committed or otherwise accepted. | Treat Cycle 12 as implemented in the workspace, not as a release checkpoint. Cycle 35 must record final commands and metrics before the bytecode survival phase is complete. |
| 2026-06-21 | Cycle 14 | Per-droid VM runtimes are assigned and visible, but bytecode images are not loaded yet: `make cazenv-survival` reports `assigned=20 loaded=0 stepping=0 instructions=0`. | Keep Cycle 14 `Done` as runtime-state scaffolding. Cycle 15 remains responsible for actual `.caz` program loading. |
| 2026-06-21 | Cycle 15 | CazEnv now loads bytecode images for all 20 droids. `make cazenv-survival` reports `assigned=20 loaded=20 stepping=0 faulted=0 instructions=0`. | Mark Cycle 15 `Done`. This proves program loading, not bytecode-owned survival; VM stepping remains Cycle 18. |
| 2026-06-21 | Cycle 16 | CazEnv survival/environment ports now return bounded deterministic values. Survival logs show `sample-ports droid=0 battery=97 charger=(bearing=15 distance=113 slots=8) junction=(bearing=128 distance=30) solar=89 energy=0 nav=0/0`. | Mark Cycle 16 `Done` for charger, junction, solar, battery, energy, and nav adapter work. Richer eye, ear, terrain, IMU, lifted, dropped, and reflex sensing remains Cycle 17. |
| 2026-06-21 | Cycle 17 | CazEnv body and behaviour ports now return changing context-derived values. `make cazenv-survival` logs eye, ear, body, terrain, reflex, and survival port samples at start and after simulation. | Mark Cycle 17 `Done`. Terrain remains a low ordinal caution signal because archived programs use `TERRAIN >= 4`; broad object proximity is exposed through `EYE_EDGE`. |
| 2026-06-21 | Cycle 18 | CazEnv now enables loaded VM runtimes with a fixed budget of one bytecode instruction per droid per environment step. A 14-day/3-seed run completed with `stepping=20 halted=0 faulted=0` and seed-0 final `instructions=120960000`. | Mark Cycle 18 `Done`. This proves bounded bytecode execution and deterministic tick order, but the C supervisor still owns movement and energy decisions until Cycles 19-20 bridge VM outputs into behaviour. |
| 2026-06-21 | Cycle 19 | Bytecode output latches now drive normal interpreted motion. Survival logs show `sample-output droid=0 ... gait=4 ... motion=(mode=1 speed=1.25 ...)`, and the renderer consumes bytecode head, tail, scratch/tap, and reflex state. | Mark Cycle 19 `Done`. Normal movement no longer depends on hardcoded per-program speed profiles, though supervisor recovery still has fallback speed overrides until Cycle 24. |
| 2026-06-21 | Cycle 20 | `NAV_INTENT` now maps to charger, solar, and junction goals. The nav probe shows `return-to-charge.caz` producing bytecode-caused recovery: `nav=3 status=4 cause=1`. The default survival run still reports `cause: bytecode-nav=2 supervisor=18`. | Mark Cycle 20 `Done` for bytecode-requested NAV goal execution. Do not treat this as strict bytecode-owned survival; Cycle 24 must remove/fail supervisor fallback and Cycle 25 must add survival prologues to non-recovery programs. |
| 2026-06-21 | Cycle 21 | Charger docking now requires bytecode-caused `NAV_CHARGER`. The charger probe docks with `nav=1 status=3 cause=1`, and the full-charger probe blocks/loiters with `nav=1 status=2 cause=1` instead of silently docking. The default 14-day run now fails because supervisor-caused returns can no longer occupy charger slots. | Mark Cycle 21 `Done` for charger docking ownership. Keep the default long-run failure visible as expected evidence that Cycles 24-25 still need to remove fallback reliance and add survival prologues. |
| 2026-06-21 | Cycle 22 | Requested solar recovery is now separate from passive solar. The solar probe reaches `nav=2 status=5 cause=1` and records requested gain greater than passive gain, but the default long-run still records `requested=0.000` and fails all three seeds. | Mark Cycle 22 `Done` for bytecode-owned solar recovery semantics. Do not claim long-term survival until bytecode programs request recovery themselves across all behaviours. |
| 2026-06-21 | Cycle 18 | The recorded `make cazenv-survival` pass was true before Cycle 21/22 tightened resource ownership. The same target now fails after passing targeted probes, so old PASS notes are historical evidence, not current survival status. | Keep Cycle 18 `Done` for VM tick order. Add Miss A-01 and Cycle 36 so probe success and long-run survival failure are separated cleanly. |
| 2026-06-21 | Cycle 21 | The full-charger probe proves one blocked bytecode droid, but it does not prove multi-droid queue fairness, slot release/reassignment, or starvation-free queue handoff. | Keep Cycle 21 `Done` for ownership semantics. Add Miss A-06 and Cycle 39; strengthen Cycle 29. |
| 2026-06-21 | Cycle 15 | CazEnv loads the 10-entry CazEnv program table, while `programs/` currently contains 11 `.caz` files and the CLI named-program registry exposes only four. | Keep Cycle 15 `Done` for CazEnv runtime loading. Add Miss A-03 and Cycle 37; strengthen Cycle 27. |
| 2026-06-21 | Cycle 23 | Junction tap gain now requires bytecode `NAV_JUNCTION`; the junction probe reports `nav=3 status=4 cause=1` with `nav_tap=0.004222` and `fallback_tap=0.000000`. Missing-junction and non-bytecode tap probes both block without junction energy. | Mark Cycle 23 `Done`. Keep broader recovery-gain attribution in Cycle 38 because Cycle 23 only adds tap-specific attribution. |
| 2026-06-21 | Cycle 36 | The harness now has explicit `probes`, `short-compat`, `short-strict`, `long-compat`, and `long-strict` modes. `make cazenv-probes` passes, while `short-compat` records the known first failure at seed `0x0ca7e000`, step `19207`, droid `0`, program `curious-patrol`. | Mark Cycle 36 `Done`. Cycle 24 may now use the strict/compatibility split as a gate, while the long-run survival failure remains unresolved. |
| 2026-06-21 | Cycle 24 | CazEnv now exposes supervisor fallback event counters for charger returns, solar forage, junction tap attempts, charger loiter, speed overrides, and gait overrides. `short-compat 1 1` reports `fallback-events: charger=131051 solar=84647 junction=6 loiter=130446 speed=583581 gait=583755 total=1513486`, while `short-strict 1 1` reports the same total with `strict=FAIL`. | Mark Cycle 24 `Done`. Compatibility fallback remains available but counted; strict mode fails on fallback use, so compatibility output is not a survival success claim. |
| 2026-06-21 | Cycle 37 | The loader now owns the shared 11-program registry. `make cazenv-probes` reports `program-registry total=11 survival=8 demo=3 assignable=8 path_only=3 archive_files=11`; `skill-cycle`, `pose-frame`, and `skill-pounce` are demo/path-only for survival planning. Excluding demos moves the return-to-charge probe target from old droid `9` to droid `7`. | Mark Cycle 37 `Done`. Cycle 25 may now add survival prologues against a visible archive/participant split instead of CazEnv's former hidden 10-program table. |
| 2026-06-21 | Cycle 25 | All 11 archived `.caz` programs now call the shared `survival_check` gate. `make cazenv-regression` forced low battery on every registered program and each emitted bytecode recovery `NAV_INTENT` with no CPU fault or halt. | Mark Cycle 25 `Done`. The short survival run still fails, but no archived program is merely idling or pouncing through forced low battery without requesting recovery. |
| 2026-06-21 | Cycle 26 | The assembler now expands quoted `INCLUDE` directives before assembly; `programs/survival.inc` is shared by all archived programs and bundled into CazEnv's Xcode resources. A bad include reports `/private/tmp/caz_bad_include.caz:1: could not include missing.inc: ...`. | Mark Cycle 26 `Done`. Includes keep the CPU bytecode unchanged and preserve existing programs. |
| 2026-06-21 | Cycle 27 | `make cazenv-regression` now runs the registry check, a CLI-style `CazDroid` CPU execution check for all 11 programs, and a CazEnv forced-low-battery adapter check for all 11 programs. It reports `regression-result=PASS programs=11`. | Mark Cycle 27 `Done`. Survival targets now depend on `cazenv-regression` so the experiment cannot skip archive/prologue checks. |
| 2026-06-21 | Cycle 28 | CazEnv now blocks candidate movement into walls, fixtures, and other droids, feeds blocked state into `TERRAIN`/`EYE_EDGE`, exposes obstacle/block counts in snapshots, and reports obstacle totals in survival summaries. `make cazenv-probes` reports `obstacle-probe blocked=6 obstacle=2 terrain=4 edge=238 nav_status=2`. | Mark Cycle 28 `Done`. The new short survival first failure is bytecode-owned junction recovery blocked/depleted, so Cycle 29 should focus on queue/route strategy rather than basic recovery intent. |
| 2026-06-21 | Cycle 39 | The probe suite now includes a deterministic full-charger queue with eight occupied slots, a low-charge house waiter, high-sun feral alternate recovery, slot release, slot reassignment, duplicate-slot validation, and no gain while the charger is full. `make cazenv-probes` reports `charger-queue-probe acquired_step=19 ... invalid_slots=0 gain_while_full=0`. | Mark Cycle 39 `Done`. This closes the A-06 queue lifecycle prerequisite before Cycle 29 is declared complete. |
| 2026-06-21 | Cycle 29 | `programs/survival.inc` now has explicit no-slot charger logic, low-drain bytecode charger waiting using `SKILL_REST`/`GAIT_LOAF`, alternate solar/junction selection under congestion, and bytecode-owned "keep charging" behavior until dock release. | Mark Cycle 29 `Done`. Full charger and queue scenarios pass targeted probes, but broad short survival still fails from a junction/tap route miss rather than charger queue behavior. |
| 2026-06-21 | Cycle 30 | CazEnv and CLI droids now expose `STRATEGY_TENDENCY`/`FERAL_TENDENCY` to bytecode. The shared survival prologue branches house droids toward formal charger waiting and feral droids toward solar/junction recovery under pressure. `strategy-probe` reports house `nav=1` and feral `nav=2`, with both above zero charge. | Mark Cycle 30 `Done` for deterministic bytecode strategy split and forced-scenario survival. Keep global survival failure visible for Cycle 40/Cycle 41 rather than treating this as long-term viability. |
| 2026-06-21 | Cycle 38 | Recovery accounting now separates charger gain, passive solar, requested solar, fallback solar, tap, bytecode tap, and fallback tap in seed summaries and first-failure reports. Strict mode fails if supervisor events or fallback gain are nonzero. Probes cover low-sun requested solar, non-bytecode solar denial, non-bytecode charger denial, and non-bytecode tap denial. | Mark Cycle 38 `Done`. The short strict run still fails, but not because fallback gain is hidden; the first failure shows zero charger/fallback gain and a blocked bytecode junction recovery. |
| 2026-06-21 | Cycle 40 | After Cycles 29/30/38/39, `short-compat 1 1` still fails at seed `0x0ca7e000`, step `28452`, droid `0`, program `curious-patrol`: output `nav=3 status=2 cause=1`, charger slots `8`, junction distance `1.54`, and no tap gain. | Add Miss A-10. Cycle 40 must include this as a forced stress scenario, and Cycle 41 must correct junction reach/blocked tap recovery before Cycle 31 long-run success claims. |
| 2026-06-21 | Cycle 40 | Added a per-program stress matrix covering open charger, full charger, critical junction, low sun, high sun, obstructed route, and A-10 near-junction reach for all eight survival participants. `make cazenv-stress` reports `stress-result=PASS survival_programs=8 scenarios_per_program=7 excluded_demo=3`. | Mark Cycle 40 `Done`. Demo/path-only programs remain excluded and explicitly reported. |
| 2026-06-21 | Cycle 41 | A-10 initially failed in the stress matrix for seven programs. CazEnv now gives bytecode-owned `NAV_JUNCTION` recovery a physical crawl approach outside claw reach, records blocked junction counts, and isolates the A-10 probe path from unrelated fixtures. | Mark Cycle 41 `Done`. `make cazenv-stress` now proves A-10 tap gain with `nav_tap > 0`, zero fallback tap, and no first short-run failure at the recorded 1.54 ft no-gain shape. |
| 2026-06-21 | Cycle 31 | Added `--mode matrix` plus `make cazenv-survival-matrix` for baseline, low-sun, full-charger, distant-junction, and high-obstacle cases. `build/cazenv-survival --mode matrix 1 1` runs all five cases and fails them honestly with `matrix-result=FAIL failed_cases=5/5 days=1 seeds=1`. | Mark Cycle 31 `Done` for matrix machinery and reporting only. Add Miss A-11 and Cycle 42; do not claim long-run survival success. |
| 2026-06-21 | Cycle 32 | The Swift runtime and overlay now expose charger/passive/requested/fallback solar/tap gains, blocked movement, blocked junction counts, nav cause, and filters for low, charging, solar, tap, empty, fallback, and blocked droids. | Mark Cycle 32 `Done`. `xcodebuild -project cazenv/cazenv.xcodeproj -scheme cazenv -configuration Debug build` succeeds after the overlay return fix. |
| 2026-06-21 | Cycle 31 | The new matrix and the one-day compatibility run now expose a different first failure: seed `0x0ca7e000`, droid `12`, `stalk-and-pounce`, bytecode `NAV_JUNCTION` or `NAV_CHARGER`, depleted before reaching a distant or obstructed recovery target. Example baseline matrix failure: step `28958`, junction distance `8.44`, obstacle `20`, zero tap/fallback gain. | Add Miss A-11. Cycle 42 must address route/energy feasibility before Cycle 34 strict default or Cycle 35 release claims. Cycle 33 should persist this as an artifact rather than only console output. |
| 2026-06-21 | Cycle 33 | First-failure artifacts are now written under `build/cazenv-failures/` whenever survival fails, with deterministic names including mode, matrix case, seed, droid, and step. The A-11 investigation produced files such as `short-strict-baseline-seed-0ca7e000-droid-05-step-47271.txt`. | Mark Cycle 33 `Done`. The artifacts are diagnostic output and remain out of source control by default. |
| 2026-06-21 | Cycle 42 | During A-11 work, a new entry-vector miss was found: programs began by executing `survival.inc` at the reset vector and could `RET` without a matching `CALL`. A trace showed `return-to-charge` falling from `RET` into `pc=0000 sp=0000`; long CLI runs showed corrupted `PC=f1b5 SP=018a`. | Add Miss A-12. Fix it inside Cycle 42 by adding explicit `JP loop` or `JP setup` entries before `INCLUDE "survival.inc"` in every archived program. |
| 2026-06-21 | Cycle 42 | A-11 is corrected for the current matrix: bytecode recovery keeps moving under low-battery charger/junction recovery, critical blocked recovery can hand off to bytecode `NAV_SOLAR`, and the VM slice is high enough for timely recovery handoffs. | Mark Cycle 42 `Done`. `make cazenv-stress`, `make cazenv-probes`, and `make cazenv-regression` pass. `build/cazenv-survival --mode short-compat 1 1`, `--mode short-strict 1 1`, and `--mode matrix 1 1` all pass; matrix reports `failed_cases=0/5`. |
| 2026-06-21 | Cycle 34 | Strict bytecode-owned survival is now the default harness mode. Bare `build/cazenv-survival 1 1` reports `mode=short-strict fallback=fail-on-use` and passes. `make cazenv-survival` now depends on the strict target and passes the 14-day/3-seed strict run with zero supervisor fallback. | Mark Cycle 34 `Done`. Compatibility remains explicit through `--mode short-compat` and `make cazenv-survival-compat`; it is not the default success claim. |
| 2026-06-21 | Cycle 35 | Release-gate work is complete. `make cazenv-survival-long` passes with `matrix-result=PASS failed_cases=0/5 days=30 seeds=5`; every matrix case reports `survival-result=PASS failed_seeds=0/5 mode=matrix fallback=fail-on-use`, with no supervisor fallback, CPU faults, zero charge, final-zero, or depleted droids. | Mark Cycle 35 `Done`, set the current cycle to `None`, and treat the bytecode survival phase as complete unless a later audit adds a new corrective cycle. |

### Audit-Caught Misses

| Miss ID | Caught In Audit | Evidence | Added Back In |
| --- | --- | --- | --- |
| A-01 | Harness semantics are blurred. | `make cazenv-survival` now passes targeted nav/charger/solar probes, then fails the 14-day/3-seed survival run with `failed_seeds=3/3`. A single failing target is awkward for per-cycle verification. | Cycle 36 splits probe, smoke, strict, and long-run modes; Cycle 33 records first-failure artifacts. |
| A-02 | Supervisor demotion was ordered too aggressively. | Cycle 21/22 ownership changes correctly removed hidden docking/solar benefits, but long-run survival fails before the archived programs have bytecode recovery policy. | Cycle 24 is changed to an attribution and compatibility gate; Cycle 34 remains the point where strict bytecode survival becomes default. |
| A-03 | Program archive and registries are inconsistent. | `programs/` has 11 `.caz` files; CazEnv assigns 10 names; the CLI named-program registry exposes four. | Cycle 37 reconciles the archive, CLI registry, and CazEnv assignment table; Cycle 27 must run every archived program by path or name. |
| A-04 | Low-battery checks are not the same as survival recovery. | Only `return-to-charge.caz` emits `NAV_CHARGER`, `NAV_SOLAR`, or `NAV_JUNCTION`; several other programs merely rest or fatigue on low battery. | Cycle 25 now requires real recovery outputs or proven positive-energy conservation for every archived/CazEnv-assigned program; Cycle 27 adds forced low-battery regression checks. |
| A-05 | Junction tapping is not yet fully bytecode-owned. | `caz_env_step` still has a supervisor path into `CAZ_ENV_DROID_TAP_JUNCTION`, and tap gain is accumulated as one total without bytecode-vs-fallback attribution. | Cycle 23 exit criteria now require non-bytecode tap denial or strict fallback accounting; Cycle 38 adds recovery-gain attribution. |
| A-06 | Charger queue verification is too narrow. | The current full-charger probe covers one blocked droid and `CHARGER_SLOTS == 0`, not slot lifecycle, release, fairness, or a 20-droid queue. | Cycle 29 is strengthened; Cycle 39 adds charger slot lifecycle and queue soak tests. |
| A-07 | Solar recovery has only a sunny single-droid proof. | The solar probe proves requested gain beats passive gain, but the default run records `requested=0.000`; no night/low-sun stress probe exists yet. | Cycle 31 gets night/low-sun matrix requirements; Cycle 38 tracks requested/passive/fallback gains per recovery mode. |
| A-08 | Failure reporting is too aggregate for iterative survival work. | The harness reports seed-level failure counts but not the first failing droid, step, program, charge, nav intent, or last decisions. | Cycle 33 is strengthened and Cycle 36 makes first-failure capture part of the short development loop. |
| A-09 | C hardcoded program metadata is still mixed with bytecode behavior. | CazEnv still has a C `program_table` with default speed/gait and its own subset of program names; fallback modes still set speed/gait. | Cycle 24 limits supervisor behavior to explicit compatibility/fallback accounting; Cycle 37 moves program identity toward a shared registry. |
| A-10 | Bytecode junction recovery can stall just outside usable tap range. | After queue/strategy/accounting work, `short-compat 1 1` still fails at seed `0x0ca7e000`, step `28452`, droid `0`, with bytecode `NAV_JUNCTION`, `status=BLOCKED`, junction distance `1.54`, and no tap gain. | Cycle 40 must add this as a forced stress scenario; Cycle 41 fixes junction approach, tap reach, or alternate recovery handoff before Cycle 31 long-matrix success claims. |
| A-11 | Bytecode recovery intent can still choose an infeasible distant or obstructed recovery target too late. | After A-10 is fixed, `short-compat 1 1` and `matrix 1 1` fail at seed `0x0ca7e000`, droid `12`, `stalk-and-pounce`: baseline matrix step `28958`, `NAV_JUNCTION`, junction distance `8.44`, obstacle `20`, depleted with zero charger/tap/fallback gain. Full-charger, distant-junction, and high-obstacle cases expose the same class with charger/junction targets too far away. | Cycle 33 must persist this first-failure artifact; Cycle 42 fixes route/energy feasibility before Cycle 34 strict default or Cycle 35 release claims. |
| A-12 | Program reset vectors entered the shared survival subroutine instead of the program entry label. | Because `INCLUDE "survival.inc"` was the first executable text in each `.caz` file, high-charge programs could execute `survival_check` directly and hit `RET` with no caller. CLI trace showed `pc=0100` executing the include, then `RET` to `pc=0000`; long runs later showed corrupted PC/SP and CazEnv CPU faults. | Cycle 42 adds explicit entry jumps before the include in all archived programs and verifies no CPU faults in regression, stress, short strict, and one-day matrix gates. Cycle 35 must keep checking CPU faults in the long gate. |

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

Status: Done

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

Status: Done

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

## Next Phase: CazEnv Bytecode Survival

This phase turns CazEnv from an environment-supervised simulator into a bytecode-governed survival experiment. Seven cycles would hide too many integration risks. The original work was split into 24 chronological cycles so each cycle could be run, measured, and reverted independently if it weakened long-term survival. The 2026-06-21 audit added corrective cycles after Cycle 35 and dependency notes for misses that should be fixed before some pending original cycles.

The target control flow is:

```text
CazEnv room state
    -> per-droid Caz VM input ports
    -> .caz bytecode survival and behaviour policy
    -> NAV_INTENT, SKILL, GAIT, HEAD_YAW, and pose outputs
    -> CazEnv physics, charger, solar, junction, and renderer
    -> survival experiment metrics
```

The C environment may keep hard safety assertions, but it should stop being the primary decision maker. A successful experiment must show that recovery transitions came from `.caz` output through ports such as `NAV_INTENT`, not from hidden C thresholds.

### Bytecode Survival Priorities

| Priority | Element | Reason |
| --- | --- | --- |
| `S0` | Repeatable experiment and failure criteria. | Every change must prove it has not reduced survival. |
| `S1` | Shared VM integration. | CazEnv needs the same bytecode machinery as the CLI and CazMac. |
| `S2` | Per-droid runtime state. | Twenty droids need independent CPUs, programs, outputs, and metrics. |
| `S3` | Environment port adapter. | Bytecode can only own survival if it can sense chargers, solar, junctions, slots, obstacles, and charge. |
| `S4` | Actuator and navigation intent bridge. | CazEnv must obey bytecode outputs through physics rather than hardcoded policy branches. |
| `S5` | Program survival prologues. | Every behaviour program must check energy before playful, house-cat, or feral-cat actions. |
| `S6` | Supervisor demotion. | C hardcoded recovery should become a measured fallback or test failure, not normal operation. |
| `S7` | Long-run confidence. | Survival claims need many seeds, longer durations, and regression artifacts. |

## CazEnv Bytecode Survival Development Cycles

### Cycle 12: Survival Experiment Contract

Priority: `S0`

Status: Done

Deliverables:

- Add a repeatable CazEnv survival harness.
- Make the harness fail when any droid reaches zero charge, enters depleted state, or ends at zero charge.
- Add `make cazenv-survival` as the standard command for the experiment.
- Add survival and navigation language ports: `ENERGY_SOURCE`, `CHARGER_BEARING`, `CHARGER_DISTANCE`, `CHARGER_SLOTS`, `JUNCTION_BEARING`, `JUNCTION_DISTANCE`, `SOLAR_LEVEL`, `NAV_INTENT`, and `NAV_STATUS`.
- Update `programs/return-to-charge.caz` into a reference survival policy.
- Fix the known zero-charge accounting hole in the current CazEnv C supervisor.

Exit criteria:

- `make` succeeds.
- `build/caz --program return-to-charge --steps 8` exercises the new symbols.
- `make cazenv-survival` passes the current 14-day, 3-seed, 20-droid run.
- CazEnv still builds in Xcode.

### Cycle 13: Shared VM Build Integration For CazEnv

Priority: `S1`

Status: Done

Deliverables:

- Add the shared Caz VM, loader, body, and droid source files to the CazEnv Xcode target or a common static library target.
- Avoid copying VM code into CazEnv-specific files.
- Keep CazEnv's room simulation in `cazenv/c_core` and VM/language mechanics in `src/`.
- Add a small compile-time adapter boundary so CazEnv can use the VM without depending on CLI-only `main.c`.

Exit criteria:

- CazEnv links against the shared VM sources.
- The CLI still builds with `make`.
- No duplicate VM implementation exists under `cazenv/`.
- Xcode build succeeds.

### Cycle 14: Per-Droid Bytecode Runtime State

Priority: `S2`

Status: Done

Deliverables:

- Extend each CazEnv droid with a `CazCpu` instance or an equivalent owned runtime wrapper.
- Add assigned program image metadata per droid; actual `.caz` image loading remains Cycle 15.
- Store VM output latch state per droid: `NAV_INTENT`, `SKILL`, `GAIT`, `HEAD_YAW`, `EAR_POSE`, `TAIL_POSE`, `VOCAL`, `EYELID`, and selected joint or pose-frame outputs.
- Preserve deterministic initialization from the CazEnv seed.

Exit criteria:

- All 20 droids can be initialized with independent bytecode runtime state.
- A debug snapshot can report each droid's assigned bytecode program and VM instruction count.
- CazEnv still runs if VM stepping is disabled by a temporary feature flag.

### Cycle 15: Program Loading In CazEnv

Priority: `S2`

Status: Done

Deliverables:

- Load `.caz` programs from the app bundle or repository path in the same naming scheme as the CLI.
- Assign archived programs to the 20 droids deterministically.
- Load `return-to-charge.caz` as an actual bytecode routine rather than just a C profile name.
- Surface load failures with useful program names and paths.

Exit criteria:

- Every CazEnv droid has a loaded bytecode image.
- Missing or invalid program files fail visibly rather than silently falling back to C profiles.
- The program assignment table no longer needs to encode speed and gait as the source of behaviour truth.

Verification:

- `make cazenv-survival` reports `assigned=20 loaded=20 stepping=0 faulted=0 instructions=0` and `survival-result=PASS failed_seeds=0/3`.
- `xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build` succeeds.

### Cycle 16: Environment Sensor Port Adapter

Priority: `S3`

Status: Done

Deliverables:

- Implement CazEnv VM read callbacks for existing body ports and new survival ports.
- Map `BATTERY` to normalized droid charge.
- Map charger bearing, distance, and slots from actual charger geometry and occupancy.
- Map junction bearing and distance from nearest usable junction box.
- Map `SOLAR_LEVEL` from the room's daylight/ambient-light model.
- Map `ENERGY_SOURCE` from actual current recovery state.

Exit criteria:

- A `.caz` program can read CazEnv-specific charger, junction, solar, and battery values.
- Port values are deterministic for a fixed seed and step count.
- Values are bounded to `0..255` and documented when saturated.

Verification:

- `make cazenv-survival` prints sample survival ports with bounded values: `battery=97`, `charger=(bearing=15 distance=113 slots=8)`, `junction=(bearing=128 distance=30)`, `solar=89`, `energy=0`, `nav=0/0`.
- `build/caz --program return-to-charge --steps 8 --sample-every 4` loads and runs the bytecode path.

### Cycle 17: Body And Behaviour Sensor Port Adapter

Priority: `S3`

Status: Done

Deliverables:

- Provide CazEnv readings for eye, ear, terrain, IMU, lifted, dropped, and reflex ports.
- Derive eye and ear values from room activity, nearby droids, fixtures, and simple noise models.
- Derive terrain and collision caution from beds, trees, charger, walls, and other droids.
- Keep readings simple and deterministic before adding richer perception.

Exit criteria:

- Existing archived behaviour programs can run in CazEnv without seeing only constant `0xff` fallback values.
- Motion, edge, sound, and terrain readings change with room context.
- Sensor generation can be inspected in survival logs.

Verification:

- `make cazenv-survival` logs body/perception ports such as `eye=(luma=86 motion=175 edge=111 colour=128)`, `ear=(volume=75 pitch=113 bearing=231 pattern=0)`, and `body=(roll=121 pitch=123 lifted=0 dropped=0 terrain=1 reflex=0)`.
- The same run later logs changed values for droid 0, including `eye=(luma=89 motion=87 edge=39 colour=128)` and `body=(roll=124 pitch=137 lifted=0 dropped=0 terrain=1 reflex=0)`.

### Cycle 18: Bytecode Instruction Budget And Tick Order

Priority: `S2`

Status: Done

Deliverables:

- Define a fixed instruction budget per simulation tick per droid.
- Step each droid's VM before applying movement and energy updates.
- Decide how halted or faulted CPUs are represented in survival metrics.
- Ensure VM stepping cost remains acceptable with 20 droids.

Exit criteria:

- CazEnv can run all 20 VMs for a normal frame without obvious slowdown.
- CPU faults or halted programs are visible in the harness and app overlay.
- Repeated runs with the same seed produce the same result.

Verification:

- `make cazenv-survival` completes the default 14-day/3-seed harness with `survival-result=PASS failed_seeds=0/3`; seed 0 ends with `stepping=20 halted=0 faulted=0 instructions=120960000`.
- Two repeated `build/cazenv-survival 1 1` runs produce identical metrics, including `instructions=8640000` and `survival-result=PASS failed_seeds=0/1`.
- `xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build` succeeds and the app overlay distinguishes `run`, `halt`, `fault`, `off`, and unloaded VM states.

### Cycle 19: Output Port Bridge To CazEnv Motion

Priority: `S4`

Status: Done

Deliverables:

- Convert bytecode `GAIT`, `SKILL`, and `HEAD_YAW` outputs into movement targets or steering.
- Keep the body middle layer's reflex outputs visible to the renderer.
- Preserve CazMac-compatible meanings for coarse outputs.
- Add a debug mode that shows raw output latches beside interpreted motion state.

Exit criteria:

- A simple `.caz` program can cause visible CazEnv movement through output ports alone.
- Movement stops or changes when the program changes `GAIT` or `SKILL`.
- C hardcoded program speed profiles are no longer needed for normal behaviour.

Verification:

- `make cazenv-survival` logs raw output latches beside interpreted motion, including `sample-output droid=0 ... gait=4 ... head=64 ... motion=(mode=1 speed=1.25 ...)`.
- CazEnv snapshots and the Swift overlay now expose `NAV_INTENT`, `NAV_STATUS`, nav cause, raw `GAIT`, `SKILL`, `HEAD_YAW`, `EAR_POSE`, `TAIL_POSE`, `VOCAL`, `EYELID`, reflex state, and nav transition count.
- The renderer uses bytecode head yaw and tail pose, shows scratch/tap claws from bytecode skill/nav status, and tints active reflex states.

### Cycle 20: NAV_INTENT Bridge To Room Goals

Priority: `S4`

Status: Done

Deliverables:

- Implement `NAV_WANDER`, `NAV_CHARGER`, `NAV_SOLAR`, and `NAV_JUNCTION` as bytecode-requested room goals.
- Convert requested goals into targets while still obeying physics, walls, occupancy, and fixture collisions.
- Set `NAV_STATUS` to running, blocked, docked, tapping, solar, or idle based on the executed result.
- Log each transition with its cause: bytecode output, fallback, or failure.

Exit criteria:

- `return-to-charge.caz` can drive a droid toward charging, solar, or junction recovery in CazEnv.
- Bytecode-requested recovery goals take precedence over environment fallback.
- Recovery logs show `NAV_INTENT` as the primary cause.

Verification:

- `make cazenv-survival` runs a `return-to-charge.caz` nav probe that ends with `output=(nav=3 status=4 cause=1 gait=5 skill=8 ...)`, proving bytecode-requested junction recovery reaches tapping status.
- The default 14-day/3-seed survival run passes with `survival-result=PASS failed_seeds=0/3`, `stepping=20 halted=0 faulted=0 instructions=120960000` for seed 0.
- The same run reports `nav: charger=2 solar=0 junction=0 cause: bytecode-nav=2 supervisor=18`, so most recovery is still supervisor fallback. This is expected until Cycles 24-25 and must not be mistaken for strict bytecode-owned survival.

### Cycle 21: Charger Docking Under Bytecode Control

Priority: `S4`

Status: Done

Deliverables:

- Make charger docking happen only after a droid requests `NAV_CHARGER` and reaches the charger.
- Report available slots through `CHARGER_SLOTS`.
- Set `NAV_STATUS=DOCKED` while charging.
- Handle full charger occupancy as blocked or loitering without hidden teleporting.

Exit criteria:

- Droids can queue or loiter when all eight slots are occupied.
- Bytecode can observe slot availability and choose a different strategy.
- Charging from empty still takes about 10 simulated minutes.

Verification:

- `make cazenv-survival` runs a charger probe that ends with `charger-probe sample-output droid=9 ... output=(nav=1 status=3 cause=1 ...) motion=(mode=3 speed=0.00 ...)`, proving bytecode `NAV_CHARGER` can dock and charge.
- The same command runs a full-charger probe that ends with `full-charger-probe sample-output droid=9 ... output=(nav=1 status=2 cause=1 ...) motion=(mode=2 speed=0.35 ...)`, proving full occupancy blocks and loiters instead of granting a hidden charger slot.
- Charging rate remains `dt_seconds / 600.0f`, so empty-to-full charging is still about 600 simulated seconds, or 10 simulated minutes.
- The default 14-day/3-seed run currently fails with `survival-result=FAIL failed_seeds=3/3` because supervisor-caused returns can approach the charger but no longer silently dock. This is an honest expected regression toward bytecode ownership, not a completed survival claim.

### Cycle 22: Solar Recovery Under Bytecode Control

Priority: `S4`

Status: Done

Deliverables:

- Make enhanced solar recovery happen only after `NAV_SOLAR`.
- Keep passive solar as a very small environmental background value if needed, but measure it separately.
- Stop motion or reduce drain when the bytecode chooses solar conservation.
- Set `NAV_STATUS=SOLAR` while solar recovery is active.

Exit criteria:

- A droid can survive by intentionally entering solar recovery.
- The harness distinguishes passive solar gain from program-requested solar recovery.
- Solar recovery cannot hide a zero-charge event.

Verification:

- `make cazenv-survival` runs a solar probe that ends with `solar-probe sample-output droid=9 ... output=(nav=2 status=5 cause=1 ...) motion=(mode=5 speed=0.00 ...)`, proving bytecode `NAV_SOLAR` enters solar recovery.
- The same probe reports `solar-probe-gains passive=0.000185 requested=0.004105 charge_delta=0.003623`, proving requested recovery is measured separately and is stronger than passive solar.
- The default long-run summary now reports separate totals such as `gains: solar=46.859 passive=46.859 requested=0.000 tap=1632.971`, so passive solar is visible and cannot be confused with program-requested recovery.
- The default 14-day/3-seed run still fails with zero-charge/depleted droids, so solar recovery does not hide survival failure.

### Cycle 23: Junction Tapping Under Bytecode Control

Priority: `S4`

Status: Done

Audit misses addressed: `A-05`

Deliverables:

- Make junction tapping happen only after `NAV_JUNCTION`.
- Require a reachable junction and appropriate distance before granting tap gain.
- Set `NAV_STATUS=TAPPING` while energy is recovered from a junction.
- Keep claw/scratch animation tied to bytecode skill or nav status.
- Deny tap gain when the droid is in a junction mode for any cause other than bytecode `NAV_JUNCTION`, or count that gain explicitly as fallback so strict mode can fail it.
- Add a targeted non-bytecode tapping probe that proves supervisor or stale-mode tapping cannot receive untracked junction energy.

Exit criteria:

- A droid can intentionally route to and tap a junction.
- Failed or unreachable junction attempts report blocked status.
- Tap energy is credited only when the droid is near a junction.
- Tap energy is credited only for current bytecode `NAV_JUNCTION`, unless the run is explicitly in compatibility mode and reports fallback tap gain separately.
- The survival harness prints enough tap attribution to distinguish bytecode-requested junction energy from supervisor/fallback junction energy.

Verification:

- `make cazenv-probes` passes with `probe-result=PASS`.
- The junction probe reports `junction-probe sample-output droid=9 ... output=(nav=3 status=4 cause=1 ...)` and `junction-probe-gains tap=0.004222 nav_tap=0.004222 fallback_tap=0.000000`, proving bytecode-owned `NAV_JUNCTION` grants tap energy.
- The missing-junction probe reports `output=(nav=3 status=2 cause=3 ...)`, proving unavailable junction attempts block as failure.
- The non-bytecode tap probe reports `output=(nav=0 status=2 cause=2 ...)` and `non-bytecode-tap-gains tap=0.000000 nav_tap=0.000000 fallback_tap=0.000000 energy=0`, proving fallback/stale tap mode does not receive untracked junction energy.

### Cycle 24: Supervisor Attribution Gate Before Demotion

Priority: `S6`

Status: Done

Audit misses addressed: `A-02`, `A-09`

Deliverables:

- Add explicit compatibility and strict modes for CazEnv recovery behavior.
- Count every supervisor/fallback recovery decision by mode: charger, solar, junction, loiter, and any speed/gait override.
- Keep C code responsible for physics, resource accounting, actuator execution, and hard safety assertions.
- In strict mode, make fallback recovery a test failure even if it prevents zero charge.
- In compatibility mode, keep legacy fallback available only as a labeled diagnostic path while bytecode programs are being upgraded.
- Do not remove the remaining fallback branches silently before Cycle 25 and Cycle 27 prove the programs can issue their own recovery outputs.

Exit criteria:

- A strict run fails if droids survive only because of hidden C supervisor decisions.
- Compatibility runs report supervisor recoveries separately and cannot be used for success claims.
- The default development target makes its mode clear in its first output line.
- CazEnv still prevents undefined states such as invalid charge, invalid slots, or out-of-room coordinates.

Verification:

- `make cazenv-probes` passes with `program-registry total=11 survival=8 demo=3 assignable=8 path_only=3 archive_files=11` followed by `probe-result=PASS`.
- `build/cazenv-survival --mode short-compat 1 1` reports `cazenv harness mode=short-compat fallback=allowed-counted ...` and fails honestly with supervisor attribution: `fallback-events: charger=131051 solar=84647 junction=6 loiter=130446 speed=583581 gait=583755 total=1513486 result=FAIL`.
- `build/cazenv-survival --mode short-strict 1 1` reports `cazenv harness mode=short-strict fallback=fail-on-use ...`, includes the same fallback event breakdown, and adds `strict=FAIL`, proving strict mode fails on fallback use.
- `make` succeeds.
- `xcodebuild -project cazenv/cazenv.xcodeproj -scheme cazenv -configuration Debug build` succeeds.

### Cycle 25: Program Survival Prologue Pattern

Priority: `S5`

Status: Done

Audit misses addressed: `A-04`

Deliverables:

- Define a standard survival prologue for `.caz` programs.
- Apply the prologue to all archived behaviour programs and every program assigned by CazEnv, or introduce assembler include support for shared prologue code.
- Ensure every program checks `BATTERY` before high-drain behaviours.
- Branch to charger, solar, or junction logic before playful or exploratory routines.
- Treat a low-battery `REST`, `fatigue`, or `loaf` branch as insufficient unless it also requests `NAV_SOLAR` with positive requested solar gain or another measured positive-energy recovery path.
- Preserve each program's house-cat or feral-cat personality after the shared survival decision has run.

Exit criteria:

- Every `.caz` file in `programs/` and every CazEnv-assigned program has an energy check path that can emit `NAV_CHARGER`, `NAV_SOLAR`, or `NAV_JUNCTION` when appropriate.
- No archived program can pounce, stalk, retreat, wander, or merely idle indefinitely while below the survival threshold without measured positive energy recovery.
- The prologue remains readable in assembly source.
- Forced low-battery probes show each program choosing a recovery output rather than relying on supervisor fallback.

Verification:

- Added `programs/survival.inc` with a readable `survival_check` gate and recovery branches for charger, solar, and junction.
- All 11 archived `.caz` files include `survival.inc` and call `survival_check` before normal behaviour or demo loops.
- `make cazenv-regression` forced low battery on all 11 registered programs and every program emitted `NAV_CHARGER`, `NAV_SOLAR`, or `NAV_JUNCTION` with `cause=1`.
- The short survival experiment still fails, but the seed-0 first failure now shows bytecode recovery output: `output=(nav=3 status=2 cause=1)`.

### Cycle 26: Shared Assembly Include Or Macro Support

Priority: `S5`

Status: Done

Deliverables:

- Add minimal assembler support for shared includes or macros if duplicated prologues become brittle.
- Keep the CPU bytecode unchanged.
- Document include resolution and error reporting.
- Move common survival policy into a shared source fragment only after the direct version is proven.

Exit criteria:

- Program source can reuse survival logic without copy-paste drift.
- Bad include paths report a line-numbered assembler error.
- Existing programs without includes still assemble.

Verification:

- `INCLUDE "survival.inc"` expands before assembly without changing the CPU instruction set or bytecode format.
- A missing include reports a source and line: `/private/tmp/caz_bad_include.caz:1: could not include missing.inc: could not open /private/tmp/missing.inc`.
- `make`, `make cazenv-probes`, and `make cazenv-regression` all assemble the include-backed programs successfully.
- CazEnv's Xcode project now bundles `survival.inc`; `xcodebuild -project cazenv/cazenv.xcodeproj -scheme cazenv -configuration Debug build` copies it into app resources and succeeds.

### Cycle 27: Bytecode-Owned Behaviour Regression Suite

Priority: `S0`

Status: Done

Audit misses addressed: `A-03`, `A-04`

Deliverables:

- Add a command or script that runs every named `.caz` program through the CLI and CazEnv bytecode adapter.
- Run every `.caz` file in `programs/`, including files that are not yet exposed through the CLI named-program registry.
- Check for assembler errors, CPU faults, invalid ports, and halted programs.
- Record expected minimum survival outputs from `return-to-charge.caz`.
- Record forced low-battery recovery outputs for each archived/CazEnv-assigned program.
- Add regression output compact enough for repeated development cycles.

Exit criteria:

- One command verifies that all archived programs assemble and execute by path or by shared registry name.
- A failed program reports the source file and failure mode.
- The suite fails if a program has a battery path that never emits a recovery `NAV_INTENT` under forced low charge.
- The survival experiment depends on this suite or runs it first.

Verification:

- Added `make cazenv-regression`.
- `make cazenv-regression` runs the registry/archive check, CLI-style `CazDroid` CPU execution for all 11 programs, and CazEnv forced-low-battery checks for all 11 programs.
- The suite reports `regression-result=PASS programs=11`.
- `cazenv-survival-compat`, `cazenv-survival-strict`, and `cazenv-survival-long` now depend on `cazenv-regression`.

### Cycle 28: Collision And Obstacle Feedback

Priority: `S3`

Status: Done

Deliverables:

- Add simple collision/obstacle sensing for walls, beds, trees, charger, junctions, and other droids.
- Feed obstacle state into `EYE_EDGE`, `TERRAIN`, or a new documented port if existing ports are too overloaded.
- Let bytecode respond before CazEnv clamps or redirects movement.
- Log blocked movement events.

Exit criteria:

- Programs can detect that a chosen route is blocked.
- The harness reports whether survival failures came from navigation blockage.
- Droids no longer pass through major fixtures during recovery routes.

Verification:

- CazEnv now blocks candidate movement into room bounds, fixtures, and other droids while allowing intentional charger docking and junction tapping.
- Blocked state feeds `TERRAIN` and `EYE_EDGE`, and CazEnv snapshots expose `obstacle_state` and `blocked_movement_count`.
- `make cazenv-probes` reports `obstacle-probe blocked=6 obstacle=2 terrain=4 edge=238 nav_status=2` and passes.
- Short survival summaries now include obstacle totals, for example `obstacles: droids=20 blocked=3251603`, and first-failure detail includes `obstacle` and `blocked` fields.

### Cycle 29: Charger Queue Strategy In Bytecode

Priority: `S5`

Status: Done

Audit misses addressed: `A-06`

Deliverables:

- Add bytecode logic for low charge when `CHARGER_SLOTS == 0`.
- Choose solar or junction instead of waiting blindly when the queue is risky.
- Introduce a low-drain loiter/rest strategy near the charger when waiting is safe.
- Keep this logic visible in source, not hidden in C.
- Add a multi-droid full-charger scenario that checks slot release, reassignment, loiter radius, and starvation risk over time.

Exit criteria:

- Full charger scenarios do not cause zero-charge failures.
- Logs show bytecode choosing alternate recovery when slots are unavailable.
- The queue behaviour remains stable with 20 droids.
- A droid blocked from charging can later acquire a released slot through bytecode `NAV_CHARGER`, not through hidden reassignment.

Verification:

- `programs/survival.inc` now handles `CHARGER_SLOTS == 0` in bytecode, including low-drain `SKILL_REST`/`GAIT_LOAF` charger waiting, alternate solar/junction selection, and explicit keep-charging behavior while `ENERGY_SOURCE == ENERGY_CHARGER`.
- `make cazenv-probes` reports full charger waiting as bytecode-owned: `full-charger-probe ... output=(nav=1 status=2 cause=1 gait=0 skill=2 ...)`.
- `make cazenv-probes` also reports alternate recovery under congestion through the queue soak: `charger-queue-probe ... alternates=878 invalid_slots=0 gain_while_full=0`.
- Short survival still fails, but first failure now has charger queue ruled out: charger slots are `8`, charger/fallback gain is `0.000000`, and the active issue is bytecode `NAV_JUNCTION` blocked near a junction.

### Cycle 30: House And Feral Strategy Split In Bytecode

Priority: `S5`

Status: Done

Deliverables:

- Expose a deterministic house/feral tendency or strategy input to bytecode.
- Make house-oriented programs prefer formal charging and beds.
- Make feral-oriented programs prefer solar conservation, hiding/resting, and junction tapping when safe.
- Keep both strategies genetically the same domestic-cat body and program family.

Exit criteria:

- House and feral behaviour differences are produced by bytecode decisions.
- The renderer and logs show which strategy is active.
- Both strategies pass survival thresholds.

Verification:

- Added bytecode-readable `STRATEGY_TENDENCY`/`FERAL_TENDENCY`, backed by deterministic CLI scenario values and CazEnv's per-droid `feral` value.
- The shared survival prologue branches house-oriented droids toward formal charger wait/charge behavior and feral-oriented droids toward solar/junction recovery when congestion or risk makes that preferable.
- `make cazenv-probes` reports `strategy-probe house(nav=1 status=2 strategy=25 skill=2 gait=0 charge=0.298) feral(nav=2 status=5 strategy=229 skill=2 gait=0 charge=0.303)`.
- This proves the forced strategy split and above-zero survival in that scenario. It is not a long-term survival claim; the short-run failure remains tracked as A-10.

### Cycle 31: Long-Duration Multi-Seed Survival Matrix

Priority: `S7`

Status: Done

Audit misses addressed: `A-07`

Deliverables:

- Expand `make cazenv-survival` or add a longer target for 30-day and 100-day simulated runs.
- Run more seeds, including stress seeds with crowded charger placement and difficult junction positions.
- Include night/low-sun, full-charger, distant-junction, and high-obstacle stress cases.
- Record min charge, supervisor fallback count, recharge count, solar count, tap count, blocked nav count, and CPU fault count.
- Record passive solar, bytecode-requested solar, bytecode-requested tap, and fallback recovery gain separately.
- Keep the default short target fast enough for frequent development.

Exit criteria:

- Short target remains suitable for every development cycle.
- Long target can be run before declaring a survival improvement complete.
- No run is considered passing if supervisor fallback count is nonzero.

Verification:

- Added `--mode matrix`/`long-matrix` and `make cazenv-survival-matrix`; `make cazenv-survival-long` now delegates to the matrix target.
- Matrix cases currently run: `baseline`, `low-sun`, `full-charger`, `distant-junction`, and `high-obstacle`.
- `build/cazenv-survival --mode matrix 1 1` runs every case and reports `matrix-result=FAIL failed_cases=5/5 days=1 seeds=1`.
- This cycle is complete as a matrix/reporting cycle, not as a survival success claim. The failed cases are tracked as A-11/Cycle 42.

### Cycle 32: Renderer And Overlay Attribution

Priority: `S4`

Status: Done

Deliverables:

- Show each droid's current program, `NAV_INTENT`, `NAV_STATUS`, battery, and energy source in the CazEnv overlay.
- Distinguish bytecode-requested recovery from fallback recovery.
- Keep the room readable when 20 droids are present.
- Add optional filters for depleted, low battery, charging, solar, and tapping states.

Exit criteria:

- A visual inspection can tell why a droid is moving toward a charger, solar patch, or junction.
- Strict-mode fallback is visible immediately.
- Overlay does not obscure the room simulation.

Verification:

- The Swift snapshot now exposes charger gain, passive/requested/fallback solar gain, requested/fallback tap gain, obstacle state, blocked movement count, and blocked junction count.
- The overlay adds filter controls for all, low, charging, solar, tap, empty, fallback, and blocked droids, plus gain attribution and blocked/junction-blocked counters in each visible row.
- `xcodebuild -project cazenv/cazenv.xcodeproj -scheme cazenv -configuration Debug build` succeeds.

### Cycle 33: Failure Artifact Capture

Priority: `S7`

Status: Done

Audit misses addressed: `A-01`, `A-08`

Deliverables:

- On survival failure, write a compact report with seed, step, droid index, program, charge, nav intent, nav status, energy source, position, and last several recovery decisions.
- Capture the first zero-charge or depleted event, not just end-of-run aggregate metrics.
- Include bytecode output latches, nav cause, charger slots, nearest charger/junction distances, passive solar gain, requested solar gain, tap gain, and supervisor fallback count for the failing droid.
- Optionally serialize a replay seed and step range.
- Keep artifacts out of source control by default.

Exit criteria:

- A failed run gives enough data to reproduce the failure.
- Developers do not need to watch the whole simulation to understand the first survival break.
- Failure artifacts are deterministic for a fixed build and seed.

Verification:

- Survival failures now call `write_failure_artifact()` and write deterministic text reports under `build/cazenv-failures/`.
- Artifact names include mode, matrix case, seed, droid, and step, for example `short-strict-baseline-seed-0ca7e000-droid-05-step-47271.txt`.
- Artifacts include the same first-failure and recent-trace details printed to the console.

### Cycle 34: Strict Bytecode Survival Mode As Default

Priority: `S6`

Status: Done

Audit misses addressed: `A-02`

Deliverables:

- Make strict bytecode-owned survival the default harness mode.
- Keep any legacy supervisor mode behind an explicit compatibility option.
- Update documentation to define "survival" as bytecode-owned, not merely environment-assisted.
- Remove stale wording that implies CazEnv owns normal return-to-charge decisions.
- Require Cycle 24 attribution, Cycle 25 program recovery outputs, Cycle 27 regression checks, and all audit-added corrective cycles that name Cycle 34 as a dependency before changing the default success claim.

Exit criteria:

- The default experiment fails if `.caz` programs do not issue valid survival outputs.
- Compatibility mode is clearly labeled and not used for success claims.
- Docs and harness output use the same terminology.

Verification:

- `build/cazenv-survival 1 1` reports `cazenv harness mode=short-strict fallback=fail-on-use ...` and passes with `survival-result=PASS failed_seeds=0/1`.
- `make cazenv-survival` now runs `cazenv-survival-strict`, not compatibility mode.
- `make cazenv-survival` passes the 14-day/3-seed strict run with `survival-result=PASS failed_seeds=0/3 mode=short-strict fallback=fail-on-use`.
- The strict run reports zero supervisor fallback events and `strict=ok` for all three seeds.

### Cycle 35: Release Gate For Bytecode-Owned CazEnv

Priority: `S7`

Status: Done

Deliverables:

- Run the CLI regression suite, CazEnv strict survival short run, CazEnv strict survival long run, and Xcode build.
- Review logs for supervisor fallback, CPU faults, zero charge, depleted state, blocked navigation, and invalid port usage.
- Confirm every audit-caught miss in the Audit-Caught Misses table has either been closed by its named cycle or deliberately reopened with a new corrective cycle.
- Update this plan with measured results.
- Mark the bytecode survival phase complete only if all checks pass.

Exit criteria:

- `make` succeeds.
- All survival participant `.caz` programs assemble, execute, and pass forced recovery probes.
- CazEnv strict survival passes with zero supervisor fallbacks.
- The long survival matrix passes.
- CazEnv builds in Xcode.
- The plan records the exact commands and final survival metrics.

Verification:

- `make cazenv-survival-long` passes; it runs the regression and stress gates, then the 30-day/5-seed matrix.
- The long matrix reports `matrix-result=PASS failed_cases=0/5 days=30 seeds=5`.
- Baseline, low-sun, full-charger, distant-junction, and high-obstacle matrix cases each report `survival-result=PASS failed_seeds=0/5 mode=matrix fallback=fail-on-use`.
- The long matrix reports zero supervisor fallback events, zero fallback gain, zero CPU faults, zero zero-charge/depleted/final-zero droids, and `bytecode-runtimes assigned=20 loaded=20 stepping=20 halted=0 faulted=0`.
- `make` passes.
- `git diff --check` passes.
- `make cazenv-survival` passes the default strict 14-day/3-seed gate.
- `xcodebuild -project cazenv/cazenv.xcodeproj -scheme cazenv -configuration Debug build` passes.

### Cycle 36: Harness Mode Split And First-Failure Baseline

Priority: `S0`

Status: Done

Audit misses addressed: `A-01`, `A-08`

Must run before: Cycle 24 strict-mode enforcement is used as a development gate; Cycle 33 artifact capture.

Deliverables:

- Split the current monolithic survival command into explicit probe, short compatibility, short strict, and long matrix modes or targets.
- Keep the targeted nav, charger, full-charger, and solar probes runnable as a passing development check even when long-run survival is expected to fail.
- Make the first line of each harness mode state whether supervisor/fallback recovery is allowed, counted, or failing.
- Add first-failure capture for the short run: seed, step, droid, program, charge, nav intent, nav status, nav cause, energy source, charger slots, nearest charger/junction distances, passive solar gain, requested solar gain, tap gain, and recent recovery decisions.
- Update `PLAN.md` verification wording so historical passes are not mistaken for current strict survival.

Exit criteria:

- One command can run the targeted probes and pass without requiring the 14-day survival claim to pass.
- One command can run the current short survival experiment and report the known failure with first-failure detail.
- Strict and compatibility outputs are visibly different.
- `make` and the Xcode build still succeed.

Verification:

- `make cazenv-probes` runs only the targeted probes and passes with `cazenv harness mode=probes fallback=targeted-probes ...` followed by `probe-result=PASS`.
- `build/cazenv-survival --mode short-compat 14 3` reports `cazenv harness mode=short-compat fallback=allowed-counted ...`, then fails honestly with `survival-result=FAIL failed_seeds=3/3 mode=short-compat fallback=allowed-counted`.
- The same compatibility run records first failure detail: `first-failure seed=0x0ca7e000 step=19207 droid=0 program=curious-patrol ... output=(nav=0 status=2 cause=2) ... gains=(passive=0.012452 requested_solar=0.000000 tap=0.000000 nav_tap=0.000000 fallback_tap=0.000000)`.
- `build/cazenv-survival --mode short-strict 1 1` reports `cazenv harness mode=short-strict fallback=fail-on-use ...`, includes `strict=FAIL`, and fails with `survival-result=FAIL failed_seeds=1/1 mode=short-strict fallback=fail-on-use`, proving strict and compatibility output are visibly distinct.
- `make`, `git diff --check`, and `xcodebuild -project cazenv/cazenv.xcodeproj -target cazenv -configuration Debug CODE_SIGNING_ALLOWED=NO build` all succeed.

### Cycle 37: Program Registry And Archive Parity

Priority: `S2`

Status: Done

Audit misses addressed: `A-03`, `A-04`, `A-09`

Must run before: Cycle 25 is declared complete; Cycle 27 becomes the regression gate.

Deliverables:

- Create one shared source of truth for archived program names, descriptions, and file paths, or generate the CazEnv and CLI registries from the same list.
- Reconcile the 11 files in `programs/` with CazEnv assignment and CLI named-program listing.
- Decide whether demo-only programs such as pose-frame or skill-pounce are assigned to survival droids, excluded from survival, or wrapped with survival prologues.
- Remove CazEnv-only program identity drift where possible; keep any default speed/gait hints as runtime hints, not the behavior source of truth.
- Make missing program files, duplicate names, and registry/archive mismatches fail visibly.

Exit criteria:

- The CLI can list and load every archived program by a documented name or the regression suite can deliberately classify path-only programs.
- CazEnv program assignment is derived from or checked against the same registry.
- No `.caz` source file is silently ignored by survival/regression planning.
- The plan records which programs are survival participants and which are renderer/pose demos.

Verification:

- `build/caz --list` lists all 11 archived programs by documented name.
- Survival/CazEnv participants: `curious-patrol`, `nap-watch`, `farmyard-mouser`, `loaf-and-groom`, `stalk-and-pounce`, `farmyard-caution`, `greeting-play`, and `return-to-charge`.
- Demo/path-only programs excluded from CazEnv survival assignment until wrapped or given survival prologues: `skill-cycle`, `pose-frame`, and `skill-pounce`.
- `make cazenv-probes` runs the archive parity check and reports `program-registry total=11 survival=8 demo=3 assignable=8 path_only=3 archive_files=11`.
- CazEnv no longer owns a private 10-program table; assignment is derived from loader metadata, and missing files, duplicate names, or archive/registry mismatches fail the probe visibly.
- Because demo programs are no longer assigned to survival droids, `return-to-charge` probe coverage moved from old droid `9` to droid `7`; the probes now report `program=return-to-charge` at droid `7`.

### Cycle 38: Recovery Gain Attribution And Resource Accounting

Priority: `S4`

Status: Done

Audit misses addressed: `A-05`, `A-07`

Must run before: Cycle 31 long matrix; Cycle 34 strict default.

Deliverables:

- Split recovery gain counters by source: passive solar, bytecode solar, fallback solar, bytecode junction tap, fallback junction tap, charger gain, and any compatibility-only rescue.
- Ensure `NAV_STATUS=SOLAR`, `NAV_STATUS=TAPPING`, and `NAV_STATUS=DOCKED` cannot imply bytecode ownership unless `nav_cause` is bytecode and the current `NAV_INTENT` matches.
- Add probes for low sun, no enhanced solar without `NAV_SOLAR`, no tap gain without `NAV_JUNCTION`, and no charger gain without a bytecode-owned docked slot.
- Report per-seed and aggregate recovery gains without mixing passive environmental background with intentional program recovery.

Exit criteria:

- Strict runs fail if fallback recovery gain is nonzero.
- Passive solar gain cannot mask a zero-charge or depleted event.
- Bytecode-requested and fallback-requested recovery gains are separately visible in logs and failure artifacts.
- Targeted probes prove resource accounting for charger, solar, and junction recovery independently.

Verification:

- CazEnv now records and reports charger gain, passive solar, bytecode-requested solar, fallback solar, total tap, bytecode tap, and fallback tap separately in seed summaries and first-failure reports.
- Strict mode now fails if supervisor fallback events or fallback recovery gain are nonzero.
- `make cazenv-probes` reports `low-sun-solar-probe ... requested=0.001536 fallback=0.000000`, `non-bytecode-solar-probe ... requested=0.000000 fallback=0.000000`, `non-bytecode-charger-probe ... charger_gain=0.000000`, and the existing non-bytecode tap denial still reports zero tap gain.
- `build/cazenv-survival --mode short-strict 1 1` still fails, but the first-failure gains are explicit: `charger=0.000000`, `requested_solar=0.000000`, `fallback_solar=0.000000`, `nav_tap=0.000000`, `fallback_tap=0.000000`.

### Cycle 39: Charger Slot Lifecycle And Queue Soak

Priority: `S5`

Status: Done

Audit misses addressed: `A-06`

Must run before: Cycle 29 is declared complete; Cycle 31 long matrix.

Deliverables:

- Add a deterministic multi-droid scenario with all eight charger slots full, multiple low-charge waiters, and at least one slot release.
- Verify slot ownership, release, reassignment, and no duplicate slot occupancy across many steps.
- Track queue wait time, loiter distance, blocked status duration, and whether low-charge droids switch to solar or junction when waiting is unsafe.
- Keep queue behavior controlled by bytecode `CHARGER_SLOTS` and `NAV_CHARGER`, not hidden C reassignment.

Exit criteria:

- A released slot can be acquired by a bytecode-requesting droid.
- No droid receives charger gain while `CHARGER_SLOTS == 0` unless it already owns a valid slot.
- Queue waiters either survive by alternate recovery or fail with a clear first-failure artifact.
- The 20-droid queue scenario is deterministic for a fixed seed.

Verification:

- Added `charge_slots_valid` and deterministic queue helpers to the probe harness.
- `make cazenv-probes` reports `charger-queue-probe acquired_step=19 waiter_slot=0 waiter_charge=0.593 min_waiter=0.300 charger_gain=0.293336 gain_at_full=0.279669 alternates=878 invalid_slots=0 gain_while_full=0 free_slots=7`.
- The released slot is acquired by a bytecode `NAV_CHARGER` waiter, no duplicate slot state is observed, and no waiter receives charger gain while the charger is full.
- This closes the A-06 prerequisite before Cycle 29 is declared complete.

### Cycle 40: Strategy Stress Probes Before Long Survival Claims

Priority: `S7`

Status: Done

Audit misses addressed: `A-04`, `A-07`, `A-08`, `A-10`

Must run before: Cycle 31 long matrix is used for a success claim; Cycle 35 release gate.

Deliverables:

- Add forced-scenario probes for each survival participant program: low battery with open charger, low battery with full charger, critical battery near junction, low sun, high sun, and obstructed route.
- Include the current A-10 failure shape: bytecode `NAV_JUNCTION`, blocked status, near-junction distance around 1.5 ft, and no tap gain.
- Require each program to produce a valid recovery `NAV_INTENT` or a proven positive-energy conservation path before high-drain behavior resumes.
- Include house and feral tendencies in stress probes so Cycle 30 differences are measured under survival pressure.
- Emit compact per-program pass/fail lines suitable for repeated development cycles.

Exit criteria:

- Every survival participant program passes the forced recovery matrix.
- Failures name the program, scenario, expected recovery output, actual output, and first unsafe behavior.
- No long-run survival pass can be claimed until the stress probe suite passes.
- The plan records any intentionally excluded demo programs and why they are not survival participants.

Verification:

- Added `--mode stress` and `make cazenv-stress`.
- The stress matrix covers open charger, full charger, critical junction, low sun, high sun, obstructed route, and A-10 near-junction reach.
- `make cazenv-stress` reports `stress-result=PASS survival_programs=8 scenarios_per_program=7 excluded_demo=3`.
- Excluded demo/path-only programs are reported as `excluded-demo`: `skill-cycle`, `pose-frame`, and `skill-pounce`.

### Cycle 41: Junction Reach And Blocked Tap Recovery

Priority: `S6`

Status: Done

Audit misses addressed: `A-10`

Must run before: Cycle 31 long matrix is used for a success claim; Cycle 35 release gate.

Deliverables:

- Reproduce the A-10 failure as a targeted deterministic probe: bytecode `NAV_JUNCTION`, blocked or stalled near a junction, distance around 1.5 ft, and no tap gain.
- Decide whether the correct fix is claw/tap reach, final approach motion, obstacle avoidance around wall junction boxes, or bytecode alternate recovery after repeated blocked tap attempts.
- Keep any changed tap reach or route tolerance physically plausible for a domestic-cat-sized droid with claws.
- Add logs for blocked junction recovery duration and the recovery handoff chosen after repeated blocked junction attempts.

Exit criteria:

- The A-10 targeted probe passes without supervisor gain or fallback tap gain.
- A droid near a junction can either acquire bytecode `NAV_JUNCTION` tap gain or switch to another bytecode recovery path before zero charge.
- Short survival no longer fails first on the recorded A-10 junction reach shape.

Verification:

- A-10 is now a deterministic stress scenario for every survival participant.
- CazEnv records `blocked_junction_count` and gives bytecode-owned `NAV_JUNCTION` recovery a physical crawl approach while preserving bytecode ownership of the recovery decision and tap gain.
- The A-10 probe path is isolated from unrelated charger/tree/bed fixtures so it tests the recorded near-junction miss directly.
- `make cazenv-stress` passes; each A-10 line reports `nav=3`, `status=4`, `cause=1`, positive `tap`, and `junction_blocked=0`.
- `build/cazenv-survival --mode short-compat 1 1` still fails, but the first failure is now A-11: droid `12`, `stalk-and-pounce`, a distant/obstructed recovery target, not the recorded 1.54 ft no-gain A-10 shape.

### Cycle 42: Route And Energy Feasibility For Recovery Targets

Priority: `S7`

Status: Done

Audit misses addressed: `A-11`, `A-12`

Must run before: Cycle 34 strict default; Cycle 35 release gate; any Cycle 31 long-matrix success claim.

Deliverables:

- Persist the A-11 first-failure artifact from Cycle 33 and reproduce it as a deterministic forced scenario.
- Teach bytecode recovery policy to reject or abandon recovery targets that are too distant for current charge and route blockage.
- Add route/energy feasibility ports or derived thresholds so `.caz` code can choose charger, solar, junction, or low-drain waiting before depletion.
- Add an emergency bytecode-owned handoff when `NAV_CHARGER` or `NAV_JUNCTION` remains blocked for too long.
- Keep fallback gain at zero in strict success cases; compatibility fallback may remain counted but must not mask A-11.

Exit criteria:

- The A-11 forced scenario passes without depletion and without fallback gain.
- `build/cazenv-survival --mode short-compat 1 1` no longer fails first on droid `12` with a distant/obstructed recovery target.
- `build/cazenv-survival --mode matrix 1 1` improves from the recorded `failed_cases=5/5`; any remaining failures are captured as distinct, named misses.
- The plan records measured charge, target distance, blocked counts, and selected bytecode recovery handoff.

Verification:

- Added an A-11 forced stress scenario; `make cazenv-stress` reports `stress-result=PASS survival_programs=8 scenarios_per_program=8 excluded_demo=3`.
- Added explicit entry jumps before `survival.inc` in all archived programs so reset starts at the intended program loop or setup label.
- `make cazenv-probes` and `make cazenv-regression` pass with no bytecode CPU faults.
- `build/cazenv-survival --mode short-compat 1 1` passes with `failed_seeds=0/1`.
- `build/cazenv-survival --mode short-strict 1 1` passes with `failed_seeds=0/1`, zero fallback events, and `strict=ok`.
- `build/cazenv-survival --mode matrix 1 1` passes all five cases with `matrix-result=PASS failed_cases=0/5 days=1 seeds=1`.
