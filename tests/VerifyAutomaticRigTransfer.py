"""Independently inspect automatic rig output for a retopologized human."""
import pathlib
import sys
from VerifyTemplateRigTransfer import fbx, glb

fbx_path, glb_path = map(pathlib.Path, sys.argv[1:])
visible, rows = fbx(fbx_path)
vertices = glb(glb_path)
assert len(visible) > 20_000 and len(vertices) > 20_000
assert all(rows[p] for p in visible)
print(f"Independent automatic rig: {len(visible)} FBX points and {len(vertices)} GLB vertices")
