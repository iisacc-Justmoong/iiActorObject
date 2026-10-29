#!/usr/bin/env python3
"""Optional application relocation check, requiring an explicit user request."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
from ExecutionResources import PROJECT

prefix, fixture, output = map(Path, sys.argv[1:4])
libdir = Path(sys.argv[4]) if len(sys.argv) > 4 else Path('lib')
bindir = Path(sys.argv[5]) if len(sys.argv) > 5 else Path('bin')
assert not libdir.is_absolute() and not bindir.is_absolute()
assert '..' not in libdir.parts and '..' not in bindir.parts
build = (PROJECT/'build').resolve(strict=True)
assert build in prefix.resolve(strict=True).parents and build in fixture.resolve(strict=True).parents
output = output.resolve(strict=False)
assert output == build or build in output.parents
output.mkdir(parents=True, exist_ok=True)
output = output.resolve(strict=True)
assert output == build or build in output.parents
with TemporaryDirectory(prefix='relocated-sdk-', dir=output) as directory:
    relocated = Path(directory)
    shutil.copytree(prefix/bindir, relocated/bindir, symlinks=True)
    shutil.copytree(prefix/libdir, relocated/libdir, symlinks=True)
    tool = relocated/bindir/('iiActorObjectRigSamples.exe' if os.name == 'nt' else 'iiActorObjectRigSamples')
    result = subprocess.run([sys.executable, str(Path(__file__).with_name('VerifyRigSamplesProjectCli.py')),
                             str(tool), str(fixture), str(relocated/'test-output')], capture_output=True, text=True)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
print('relocated SDK tool and relative library lookup passed; temporary files cleaned')
