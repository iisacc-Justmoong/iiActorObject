"""Check convention-only registrations and reject disguised application runs."""
from VerifyExecutionPolicy import verify_registrations

name = 'iiActorObject.human_interchange_convention.blender'
native = '/build/iiActorObjectHumanInterchangeConventionTests'
verify_registrations([{'name': name, 'command': [native, 'blender']}])
verify_registrations([{'name': name + '_sanitized', 'command': [native, 'blender_sanitized']}])

for test in (
    {'name': name, 'command': ['/Applications/Blender.app/Contents/MacOS/Blender', 'blender']},
    {'name': name, 'command': ['/usr/bin/python3', 'blender']},
    {'name': 'iiActorObject.other.blender', 'command': [native, 'blender']},
    {'name': 'iiActorObject.fixture', 'command': ['/usr/bin/python3', '/build/iiActorObjectRigSamples']},
    {'name': 'iiActorObject.fixture', 'command': ['/usr/bin/python3', '/scripts/VerifyBlender.py']},
):
    try:
        verify_registrations([test])
    except AssertionError:
        continue
    raise AssertionError(f"Unsafe registration was accepted: {test}")
print('Convention-only tests are allowed; application registrations remain forbidden')
