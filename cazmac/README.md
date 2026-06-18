# CazMac

CazMac is a macOS SwiftUI and Metal prototype shell for Caz. It links the existing C runtime from `../src`, runs the Z80-style Caz bytecode programs, and renders a cat droid whose body pose follows the VM output ports.

The left-side translucent overlay shows live Caz state, current instruction disassembly, recent VM instructions, and the high-level program listing. It is intentionally styled as a green alpha-blended machine-vision layer.

## Open In Xcode

```sh
open cazmac.xcodeproj
```

The app target is `cazmac`.

## Command-Line Build

From the repository root:

```sh
xcodebuild -project cazmac/cazmac.xcodeproj -target cazmac -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

## Runtime Shape

- SwiftUI owns the development surface and controls.
- Metal renders the droid, sensor rays, body pose, tail, ears, eyes, and farmyard floor.
- The renderer uses `CazRig.generated.swift`, produced from `Assets/ThirdParty/OpenCat/simple-opencat-reference.urdf`, for link sizes, joint origins, axes, and the Caz 16-DOF joint map.
- The Caz C runtime remains the behavioural source of truth.
- CazMac snapshots include active skill, reflex state, normalized body sensors, and all 16 body joint values.
- The bridge header imports `caz_cpu.h`, `caz_droid.h`, and `caz_loader.h`.
- The `.caz` files from `../programs` are included as app resources and loaded by the C loader.

## Rig And Asset Pipeline

The asset pipeline lives under `Assets/`:

- `ThirdParty/OpenCat/simple-opencat-reference.urdf` is the local OpenCat-style reference rig used for pipeline development.
- `Tools/GenerateRig.swift` converts the URDF into `cazmac/CazRig.generated.swift`.
- `Assets/Generated/nybble-optional-meshes.json` records optional Nybble attachment candidates, disabled until source and license notes are pinned down.
- The procedural rig renderer is the fallback body and does not depend on STL files.

Regenerate the rig with:

```sh
swift Tools/GenerateRig.swift Assets/ThirdParty/OpenCat/simple-opencat-reference.urdf cazmac/CazRig.generated.swift
```

## App Icon

The icon is generated from `Artwork/caz-reference.png`, a reference-style Caz image adapted into a rounded macOS app icon. Regenerate the full icon set with:

```sh
swift Tools/GenerateIcon.swift
```
