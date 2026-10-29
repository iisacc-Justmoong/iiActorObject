"""Verify responsibility directories without changing the installed header API."""
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve() / "src"
if (root / "iiActorObject").exists():
    raise AssertionError("The source tree repeats the project name")

representative = (
    "Core/Transform.h",
    "Runtime/Actor.cpp",
    "Geometry/MeshGeometry.cpp",
    "Rig/Skeleton.cpp",
    "Project/Object3DProjectArchive.h",
    "Runtime/Object3DEngine.cpp",
    "Interchange/FBX/Object3DFbxBinary.cpp",
    "Interchange/GLB/Object3DGlbBuffer.cpp",
    "NSCharacter/CharacterAsset.h",
)
for name in representative:
    if not (root / name).is_file():
        raise AssertionError(f"Missing responsibility source: {name}")

sources = []
for entry in root.iterdir():
    if entry.name == "third_party":
        continue
    candidates = entry.rglob("*") if entry.is_dir() else (entry,)
    sources.extend(p for p in candidates if p.is_file() and p.suffix in (".cpp", ".h"))
by_stem = {}
for path in sources:
    by_stem.setdefault(path.stem, {}).setdefault(path.suffix, []).append(path)
for stem, pair in by_stem.items():
    if len(pair.get(".cpp", ())) == len(pair.get(".h", ())) == 1:
        if pair[".cpp"][0].parent != pair[".h"][0].parent:
            raise AssertionError(f"Interface and implementation are separated: {stem}")
