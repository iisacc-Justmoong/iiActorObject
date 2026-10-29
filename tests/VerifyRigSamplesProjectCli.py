#!/usr/bin/env python3
"""Optional application check, enabled only by an explicit user request."""
import json
import os
from pathlib import Path
import subprocess
import sys
from ExecutionResources import PROJECT, execution_workspace

tool, fixture, root = map(Path, sys.argv[1:])
build = (PROJECT/'build').resolve(strict=True)
for executable in (tool, fixture):
    if build not in executable.resolve(strict=True).parents:
        raise ValueError('Test executables must be inside build/')
env = os.environ.copy()
for name in ('DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH', 'LD_LIBRARY_PATH'):
    env.pop(name, None)
def run(executable, *args, good=True):
    result = subprocess.run([str(executable), *map(str, args)], capture_output=True, text=True, env=env)
    assert (result.returncode == 0) == good, (result.args, result.returncode, result.stdout, result.stderr)
    return result
with execution_workspace(root) as workspace:
    resource = workspace.resource_directory
    out = workspace.output_directory
    run(fixture, 'project_fixture', resource)
    source = workspace.link(resource/'source.ii3dproject', 'source.ii3dproject')
    original = source.read_bytes()
    args = ['--baseline', 'raw', 'Turn', '0', '--sample', 'PoseTurn', 'raw', 'Turn', '.75']
    run(tool, source, source, *args, good=False)
    for kind in ('hardlink', 'symlink'):
        alias = out/(kind + '.ii3dproject')
        try:
            if kind == 'hardlink':
                os.link(source, alias)
            else:
                alias.symlink_to(source)
        except OSError:
            continue
        run(tool, source, alias, *args, good=False)
    assert source.read_bytes() == original
    run(tool, source, out/'bad.glb', '--sample', 'Invalid', 'raw', 'MissingControl', '1', good=False)
    assert not (out/'bad.glb').exists()
    run(tool, source, out/'shape.glb', '--manifest', out/'shape.json', *args)
    manifest = json.loads((out/'shape.json').read_text(encoding='utf-8'))
    catalog = manifest['catalogs'][0]
    assert catalog['sourceFormat'] == 'iiActorObject.poseRig.1'
    assert catalog['samples'][0]['morph'] == 'PoseTurn'
    assert catalog['samples'][0]['state']['controls'] == [.75]
    run(tool, source, out/'shape.glb', *args, good=False)
    project = resource/'shape.ii3dproject'
    run(tool, source, project, *args)
    project_link = workspace.link(project, 'shape.ii3dproject')
    run(tool, project_link, out/'shape.fbx', '--sample', 'SecondPose', 'raw', 'Turn', '.5')
    assert (out/'shape.fbx').read_bytes().startswith(b'Kaydara FBX Binary')
print('non-DNA project CLI, retained rig and input alias safety passed; temporary resources cleaned')
