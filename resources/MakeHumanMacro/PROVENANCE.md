# MakeHuman CC0 morphology data

These 348 unmodified graphical target files come from the official MakeHuman
repository at commit `a8bc2d54ff0ac92e78ff71431b1023eda42bf482`:
`makehuman/data/targets/macrodetails/`.

Families: 24 population/sex/age bases, 72 muscle/mass corrections, 144 height
corrections and 108 proportion corrections. The original files explicitly state
CC0; `LICENSE.ASSETS.md` and `LICENSE.md` retain the upstream license distinction.
No application code, Python module or shader program is copied.

`UPSTREAM-SHA256.json` records source paths, byte counts, Git blob SHA-1 values
and content SHA-256 values. Each downloaded target was checked against the pinned
Git tree before publication. `catalog.json` is an iiActorObject-authored index.

The independent C++ adapter assigns tensor-product curves, bakes the default
neutral combination and uses the core's neutral-subtracted drivers. Its age
anchors (0, 10, 25, 90 years), centered controls and population stick-breaking
parameterization are explicit iiActorObject semantics, not a claim to reproduce
the MakeHuman application's original modifier implementation. These basis labels
name source graphic assets; they are not measurements or classifications of a person.

The original `data/MakeHuman` and `data/MakeHumanFace` packages remain separate.
