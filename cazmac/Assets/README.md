# CazMac Asset Sources

This folder separates rigging inputs, optional third-party references, and generated local assets for the CazMac body renderer.

## Pipeline

1. Keep source references under `ThirdParty/` with provenance notes beside them.
2. Convert the OpenCat-style URDF reference with:

```sh
swift cazmac/Tools/GenerateRig.swift \
  cazmac/Assets/ThirdParty/OpenCat/simple-opencat-reference.urdf \
  cazmac/cazmac/CazRig.generated.swift
```

3. The generated Swift rig is compiled into CazMac and used by the Metal renderer.
4. Optional Nybble-derived mesh pieces must stay disabled until their source, orientation, scale, and license are recorded.
5. The procedural fallback body must continue to render when optional meshes are disabled.

The current repository intentionally does not bundle third-party STL geometry. The rig file is a local reference model shaped to match the Caz 16-DOF body map and to keep the renderer independent from unlicensed mesh assets.
