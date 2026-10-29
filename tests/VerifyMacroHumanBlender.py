"""Reuse the full authored face/skin/image checks for adult and child macro shapes."""
from pathlib import Path
import json
import runpy
import sys

directory, assets = map(Path, sys.argv[sys.argv.index("--") + 1:])
script = Path(__file__).with_name("VerifyFaceAssetsBlender.py")
results = []
for label in ("adult", "child"):
    sys.argv = [str(script), "--", str(directory / label), str(assets)]
    runpy.run_path(str(script), run_name="__main__")
    results.append({"shape": label, **json.loads((directory / label / "BlenderFaceAssetsVerification.json").read_text())})
(directory / "BlenderMacroVerification.json").write_text(json.dumps(results, indent=2) + "\n")
