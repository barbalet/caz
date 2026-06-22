# Ideal Domestic Cat Target STL

Generated from `cazenv/Tools/GenerateIdealCatSTL.py` as a morphology target for a future 3D Caz droid body.

The shape is informed by the `stock/` domestic-cat references: long low trunk, separate shoulder and haunch volumes, compact forward-looking head, triangular ears, short muzzle, planted paws, and a full cat-length tail.

## Stock-Image Tuning Cues

- Side walking references: long body, arched back, tucked abdomen, leg columns under the shoulder and haunch rather than at the extreme ends.
- Sitting references: distinct haunch mass, compact neck-to-head transition, and upright triangular ears.
- Lying references: smooth continuous torso volume and a narrower waist from front-to-back than the earlier procedural rig implied.
- Head-on references: narrow chest, cheek/muzzle pads, short nose, almond eye placement, and paws grouped under the body line.

This is not the current CazEnv runtime mesh. It is an ideal target asset for future 3D cat-like body work.

Subjective morphology target score: roughly 9/10 for a textureless procedural STL. The remaining gap to a living-cat likeness is mostly fur, coat pattern, and pose-aware muscle deformation rather than the base body proportions.

## Generated Files

- `ideal-domestic-cat-target.stl`: review mesh, standing on all fours, head looking forward.
- `ideal-domestic-cat-target-preview.png`: simple orthographic preview generated from the STL triangles.

## Mesh Stats

- Triangles: 67764
- Bounds: x -0.956..0.723, y -0.131..0.129, z 0.006..0.668
