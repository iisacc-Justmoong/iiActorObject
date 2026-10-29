#!/usr/bin/env python3
"""Check CTest registration without launching user-facing applications."""
import json
import hashlib
from pathlib import Path
import subprocess
import sys
from ExecutionResources import RESOURCES, execution_workspace

def verify_registrations(tests):
    applications = {'iiactorobjectrigsamples', 'iiactorobjectposesnapshot', 'iiactorobjectcore3d',
                    'iiactorobjectcharacterize', 'iiactorobjecttemplatecharacter',
                    'iiactorobjectcharacterauthoring', 'iiactorobjectdnacharacter'}
    # These are native unit-test modes for bone naming conventions. They do
    # not launch Blender; accepting the exact executable and mode avoids a
    # broad exception for application checks merely named as conventions.
    conventions = {
        'iiactorobject.human_interchange_convention.blender': 'blender',
        'iiactorobject.human_interchange_convention.blender_sanitized': 'blender_sanitized',
    }
    for test in tests:
        name = test['name'].lower()
        command = test.get('command', [])
        bases = [argument.replace('\\', '/').rsplit('/', 1)[-1].lower().removesuffix('.exe')
                 for argument in command]
        convention = (name in conventions and len(command) == 2 and
                      bases[0] == 'iiactorobjecthumaninterchangeconventiontests' and
                      command[1].lower() == conventions[name])
        assert not name.endswith(('.cli', '.project_cli', '.example', '.example_source', '.native_example', '.example_safety')), name
        assert ('blender' not in name or convention) and 'unreal' not in name and 'charactercreator' not in name, name
        for index, base in enumerate(bases):
            allowed_mode = convention and index == 1
            assert base not in applications and ('blender' not in base or allowed_mode), (name, base)


def main():
    build = Path(sys.argv[1])
    assert hashlib.sha256((RESOURCES/'fixtures/dna/independent.dna').read_bytes()).hexdigest() == '48318a9de7b3ca1020deb44d28cced561adc9e7e1e63d3c4f7dd487553d3046b'
    cache = (build/'CMakeCache.txt').read_text(encoding='utf-8')
    assert 'IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION:BOOL=OFF' in cache
    plan = json.loads(subprocess.check_output(['ctest', '--test-dir', str(build), '-N', '--show-only=json-v1'], text=True))
    verify_registrations(plan['tests'])
    with execution_workspace(build) as workspace:
        resource_directory, output_directory = workspace.resource_directory, workspace.output_directory
        source = resource_directory/'fixture.txt'
        source.write_text('resource', encoding='utf-8')
        link = workspace.link(source, 'fixture.txt')
        assert source.samefile(link) and link.read_text(encoding='utf-8') == 'resource'
        outside = output_directory/'outside.txt'
        outside.write_text('outside', encoding='utf-8')
        for item, name in ((outside, 'outside-link'), (source, '../escape')):
            try:
                workspace.link(item, name)
            except ValueError:
                pass
            else:
                raise AssertionError('Invalid execution resource was accepted')
    assert not resource_directory.exists() and not output_directory.exists()
    try:
        with execution_workspace(build) as workspace:
            failed_resource, failed_output = workspace.resource_directory, workspace.output_directory
            raise RuntimeError('test cleanup')
    except RuntimeError:
        pass
    assert not failed_resource.exists() and not failed_output.exists()
    try:
        with execution_workspace(RESOURCES):
            raise AssertionError('Non-build output root was accepted')
    except ValueError:
        pass
    print('Application execution validation is disabled; resource links and cleanup passed.')


if __name__ == '__main__':
    main()
