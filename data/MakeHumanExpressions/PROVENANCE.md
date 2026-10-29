# CC0 face pose units

The unmodified face BVH and frame-map JSON are from the official MakeHuman
repository at commit `a8bc2d54ff0ac92e78ff71431b1023eda42bf482`, under
`makehuman/data/poseunits/face-poseunits.bvh` and `face-poseunits.json`.
The JSON explicitly identifies the data as CC0. Original license statements are
included. `UPSTREAM-SHA256.json` records exact byte hashes and the source Git blobs.
Only the local filenames are shortened, keeping the original JSON motion filename.
No application code, shader program or Python module is copied.

The dataset contains 163 named joints and 60 frames: Rest and 59 pose units.
The independent C++ adapter converts source Z-up to SDK Y-up, forms world-rotation
differences relative to Rest and rebinds them to the target's current bind frames.
`catalog.json` is an iiActorObject-authored index. Compiled pose programs and baked
snapshot morphs are SDK output, not a reconstruction of the original application.
The source unit names do not establish ARKit or production MetaHuman equivalence.

The preset explicitly removes neutral-relative local rotations at or below
1e-6 radians before forming world differences. This filters numerical body-joint
noise in the six-decimal BVH data; the generic reader preserves every value and
the generic pose-program builder defaults to zero tolerance.
