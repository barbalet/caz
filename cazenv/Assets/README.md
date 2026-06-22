# CazEnv Asset Sources

This folder separates CazEnv morphology targets, rigging inputs, optional third-party references, and generated local assets. These files are durable sources and review artifacts, not all runtime resources.

## Pipeline

1. Keep source references under `ThirdParty/` with provenance notes beside them.
2. Convert the OpenCat-style URDF reference with:

```sh
swift cazenv/Tools/GenerateRig.swift \
  cazenv/Assets/ThirdParty/OpenCat/simple-opencat-reference.urdf \
  cazenv/Assets/Generated/CazRig.generated.swift
```

3. The generated Swift rig is preserved as a 16-DOF reference for CazEnv morphology and future renderer work.
4. Optional Nybble-derived mesh pieces must stay disabled until their source, orientation, scale, and license are recorded.
5. CazEnv's runtime droid mesh remains `stock/3d_models/LowpolyCAT_fixed.stl` unless the renderer is deliberately changed.

The current repository intentionally does not bundle third-party STL geometry. The rig file is a local reference model shaped to match the Caz 16-DOF body map and to keep CazEnv independent from unlicensed mesh assets.

## Generated Domestic Cat Target

`Generated/ideal-domestic-cat-target.stl` and its preview are the stock-image-informed morphology target for a future cat-like Caz body. Regenerate them with:

```sh
python3 cazenv/Tools/GenerateIdealCatSTL.py
```
