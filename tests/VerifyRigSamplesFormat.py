#!/usr/bin/env python3
"""Independently inspect the native, GLB and binary FBX sampling envelopes."""
import json
from pathlib import Path
import subprocess
import struct
import sys
import zlib

root = Path(sys.argv[1])
manifest = json.loads((root / 'samples.json').read_text(encoding='utf-8'))
assert manifest['schemaVersion'] == 1
catalog = manifest['catalogs'][0]
assert catalog['sourceFormat'] == 'test-driver'
assert catalog['samples'][0]['morph'] == 'Quote"Smile'
assert catalog['baseline']['controls'] == [.3, .25]
assert catalog['samples'][0]['state']['controls'] == [.4, .1]
assert len(catalog['samples'][0]['state']['animatedMaps']) == 1
assert [bone['name'] for bone in catalog['sourceBones']] == ['root', 'tip', 'link']
project = (root / 'samples.ii3dproject').read_bytes()
magic, version, length, crc = struct.unpack_from('<8sIQI', project)
assert magic == b'iiAOPRJ\0' and version == 14 and length == len(project)-24
assert zlib.crc32(project[24:]) == crc
assert b'Quote"Smile' in project and b'wrinkle' in project
raw = (root / 'samples.glb').read_bytes()
magic, version, size = struct.unpack_from('<4sII', raw)
assert magic == b'glTF' and version == 2 and size == len(raw)
json_size, json_kind = struct.unpack_from('<I4s', raw, 12)
assert json_kind == b'JSON'
gltf = json.loads(raw[20:20+json_size].decode('utf-8'))
extras = [n.get('extras', {}).get('iiObject3D', {}).get('rigSamples') for n in gltf['nodes']]
assert manifest in extras
assert any('Quote"Smile' in m.get('extras', {}).get('targetNames', []) for m in gltf['meshes'])
fbx = (root / 'samples.fbx').read_bytes()
assert fbx[:23] == b'Kaydara FBX Binary  \x00\x1a\x00' and struct.unpack_from('<I', fbx, 23)[0] == 7400
found = []
nodes_seen = []
metadata_nodes = []
data_nodes = []
def nodes(start, limit, inside=False):
    while start + 13 <= limit:
        end, count, size, name_size = struct.unpack_from('<IIIB', fbx, start)
        if end == 0:
            return start + 13
        assert start < end <= limit and start + 13 + name_size + size <= end
        name = fbx[start+13:start+13+name_size].decode('ascii')
        nodes_seen.append((start, end))
        if name == 'iiActorObjectRigSamples':
            metadata_nodes.append((start, end))
        properties = start + 13 + name_size
        children = properties + size
        if inside and name == 'Data':
            data_nodes.append((start, end))
            assert count == 1 and fbx[properties:properties+1] == b'R'
            payload_size = struct.unpack_from('<I', fbx, properties+1)[0]
            found.append(fbx[properties+5:properties+5+payload_size])
            assert payload_size + 5 == size
        if children < end:
            nodes(children, end, inside or name == 'iiActorObjectRigSamples')
        start = end
    return start
nodes(27, len(fbx))
assert len(found) == 1
blob = found[0]
magic, version, length, crc = struct.unpack_from('<8sIQI', blob)
assert magic == b'iiAORSM\0' and version == 1 and length == len(blob)-24
assert zlib.crc32(blob[24:]) == crc
assert b'Quote"Smile' in blob and b'wrinkle' in blob
if len(sys.argv) > 2:
    # Duplicate one complete custom node while preserving valid absolute FBX
    # offsets and the original checksummed payload. Ambiguous provenance must
    # be rejected instead of silently choosing the first sibling.
    assert len(metadata_nodes) == len(data_nodes) == 1
    executable = sys.argv[2]
    assert subprocess.run([executable, 'fbx_file', str(root/'samples.fbx')], capture_output=True).returncode == 0
    for name, (begin, insertion) in [('block', metadata_nodes[0]), ('data', data_nodes[0])]:
        delta = insertion - begin
        duplicate = bytearray(fbx[:insertion] + fbx[begin:insertion] + fbx[insertion:])
        for start, end in nodes_seen:
            shifted_start = start + (delta if start >= insertion else 0)
            shifted_end = end + (delta if end > insertion or start >= insertion else 0)
            struct.pack_into('<I', duplicate, shifted_start, shifted_end)
            if start >= begin and end <= insertion:
                struct.pack_into('<I', duplicate, insertion + start - begin, end + delta)
        bad = root / ('duplicate-rig-samples-' + name + '.fbx')
        bad.write_bytes(duplicate)
        assert subprocess.run([executable, 'fbx_file', str(bad)], capture_output=True).returncode != 0, 'duplicate FBX provenance ' + name + ' was accepted'
print('independent project, GLB and FBX metadata passed')
