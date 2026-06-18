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
- The Caz C runtime remains the behavioural source of truth.
- The bridge header imports `caz_cpu.h`, `caz_droid.h`, and `caz_programs.h`.

## App Icon

The icon is generated from `Artwork/caz-reference.png`, a reference-style Caz image adapted into a rounded macOS app icon. Regenerate the full icon set with:

```sh
swift Tools/GenerateIcon.swift
```
