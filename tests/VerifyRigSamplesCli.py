#!/usr/bin/env python3
"""Optional application check, enabled only by an explicit user request."""
import json
from pathlib import Path
import subprocess
import sys
from ExecutionResources import PROJECT, execution_workspace

cli, dna, root = map(Path, sys.argv[1:])
if (PROJECT/'build').resolve(strict=True) not in cli.resolve(strict=True).parents:
    raise ValueError('The test executable must be inside build/')
def run(*args, good=True):
    result = subprocess.run([str(cli), *map(str, args)], capture_output=True, text=True)
    assert (result.returncode == 0) == good, (result.args, result.returncode, result.stdout, result.stderr)
    return result
with execution_workspace(root) as workspace:
    out = workspace.output_directory
    dna = workspace.link(dna, 'independent.dna')
    baseline = ['--baseline', 'raw', 'smile', '.25', '--sample', 'SampleSmile', 'raw', 'smile', '.5']
    run(dna, out/'bad.glb', '--sample', 'Wrong', 'raw', 'unknown', '1', good=False)
    assert not (out/'bad.glb').exists()
    run(dna, out/'nan.glb', '--sample', 'Wrong', 'raw', 'smile', 'nan', good=False)
    assert not (out/'nan.glb').exists()
    run(dna, dna, *baseline, good=False)
    run(dna, out/'sample.glb', '--manifest', out/'sample.json', *baseline)
    manifest = json.loads((out/'sample.json').read_text())
    assert manifest['catalogs'][0]['samples'][0]['morph'] == 'SampleSmile'
    assert abs(manifest['catalogs'][0]['samples'][0]['state']['animatedMaps'][0]-.25)<1e-6
    run(dna, out/'sample.glb', *baseline, good=False)
    project = workspace.resource_directory/'sample.ii3dproject'
    run(dna, project, *baseline)
    project_link = workspace.link(project, 'sample.ii3dproject')
    run(project_link, out/'sample.fbx', '--baseline', 'raw', 'smile', '.25', '--sample', 'Second', 'raw', 'smile', '.4')
    assert (out/'sample.fbx').stat().st_size > 100
print('native agent CLI safety and retained project rig passed; temporary resources cleaned')
