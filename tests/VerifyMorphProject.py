"""Read the v21 morph metadata independently and exercise CRC-valid corruption."""
import math
import pathlib
import struct
import subprocess
import sys
import zlib

executable, filename = sys.argv[1:]
path = pathlib.Path(filename)
raw = path.read_bytes()
magic, version, size, checksum = struct.unpack_from("<8sIQI", raw)
assert magic == b"iiAOPRJ\0" and version == 21 and size == len(raw) - 24
payload = raw[24:]
assert zlib.crc32(payload) == checksum

def string(value):
    encoded = value.encode("utf-8")
    return struct.pack("<Q", len(encoded)) + encoded

# The new table is the final version-21 graph field. Its known baseline and
# channel catalog identify its start without relying on the SDK decoder.
prefix = (struct.pack("<QBQ", 1, 1, 2) + string("height") + struct.pack("<d", .25) +
          string("body_width") + struct.pack("<d", .2) + struct.pack("<BBdQQQ", 1, 1, 1e-12, 8, 200000, 4096) + struct.pack("<Q", 2) +
          string("height") + string("rig_height_control"))
assert payload.count(prefix) == 1
start = payload.index(prefix)
offset = start

def take(fmt):
    global offset
    size = struct.calcsize(fmt)
    assert offset + size <= len(payload)
    result = struct.unpack_from(fmt, payload, offset)[0]
    offset += size
    return result

def text():
    global offset
    length_at = offset
    count = take("<Q")
    assert count <= 4096 and offset + count <= len(payload)
    value = payload[offset:offset+count].decode("utf-8")
    offset += count
    return value, length_at, offset

assert take("<Q") == 1
baseline_flag = offset
assert take("<B") == 1 and take("<Q") == 2
baseline = []
for _ in range(2):
    name, _, _ = text()
    value_at = offset
    value = take("<d")
    baseline.append((name, value, value_at))
assert [(n, v) for n, v, _ in baseline] == [("height", .25), ("body_width", .2)]
baseline_end = offset
settings_flag = offset
assert take("<B") == 1
replace_at = offset; assert take("<B") == 1
epsilon_at = offset; assert take("<d") == 1e-12
samples_at = offset; assert take("<Q") == 8
visits_at = offset; assert take("<Q") == 200000
deltas_at = offset; assert take("<Q") == 4096
assert take("<Q") == 2
channels = []
for _ in range(2):
    source, source_start, source_end = text()
    name, name_start, name_end = text()
    base_at = offset; base = take("<d")
    end_at = offset; end = take("<d")
    mesh_count = take("<Q"); meshes = [text() for _ in range(mesh_count)]
    joint_count = take("<Q"); joints = [text() for _ in range(joint_count)]
    joint_id_count = take("<Q"); joint_ids = []
    for _ in range(joint_id_count):
        id_at = offset; joint_ids.append((take("<Q"), id_at))
    deltas = take("<Q")
    channels.append(dict(source=source, source_start=source_start, source_end=source_end,
                         name=name, name_start=name_start, name_end=name_end,
                         baseline=base, base_at=base_at, endpoint=end, end_at=end_at,
                         meshes=meshes, joints=joints, joint_ids=joint_ids, deltas=deltas))
assert offset == len(payload)
assert [(c["source"], c["name"], c["baseline"], c["endpoint"]) for c in channels] == [
    ("height", "rig_height_control", .25, .8), ("height", "rig_height_down", .25, 0.)]
for channel in channels:
    assert [entry[0] for entry in channel["meshes"]] == ["Garment", "OBJ"]
    assert [entry[0] for entry in channel["joints"]] == ["root", "child"]
    assert [entry[0] for entry in channel["joint_ids"]] == [0, 1] and channel["deltas"] > 0

# One rig references the first saved asset, the first morph table and the first
# source template; it has no driven joints. Other backend records cannot use
# the optional template reference.
rig_marker = struct.pack("<QQQQQBQ", 1, 0, 0, 0, 2, 1, 0)
assert payload.count(rig_marker) == 1
rig_at = payload.index(rig_marker) + 8
assert rig_at < start

def envelope(data, file_version=21):
    return struct.pack("<8sIQI", magic, file_version, len(data), zlib.crc32(data)) + data

def changed_number(position, fmt, value):
    data = bytearray(payload)
    struct.pack_into(fmt, data, position, value)
    return envelope(data)

def replace_text(start_at, end_at, value):
    data = bytearray(payload)
    data[start_at:end_at] = string(value)
    return envelope(data)

malformed = [
    envelope(payload, 22),
    envelope(payload, 20),
    changed_number(rig_at + 24, "<Q", 3),       # unknown backend
    changed_number(rig_at + 24, "<Q", 1),       # pose backend cannot carry a template
    changed_number(rig_at + 8, "<Q", 999),      # absent morph program
    changed_number(rig_at + 33, "<Q", 999),     # absent source template
    changed_number(baseline_flag, "<B", 2),
    changed_number(settings_flag, "<B", 2),
    changed_number(replace_at, "<B", 2),
    changed_number(epsilon_at, "<d", -1),
    changed_number(samples_at, "<Q", 1),
    changed_number(visits_at, "<Q", 0),
    changed_number(deltas_at, "<Q", 1),
    changed_number(baseline[0][2], "<d", float("nan")),
    changed_number(baseline[0][2], "<d", .3),   # channel baseline contradicts inputs
    changed_number(channels[0]["end_at"], "<d", 1.5), # outside template target domain
    changed_number(channels[0]["joint_ids"][0][1], "<Q", 999), # unknown source joint ID
    changed_number(channels[0]["joint_ids"][1][1], "<Q", 0), # duplicate source joint ID
    replace_text(channels[1]["name_start"], channels[1]["name_end"], channels[0]["name"]),
    replace_text(channels[0]["meshes"][1][1], channels[0]["meshes"][1][2], "Garment"),
    replace_text(channels[0]["meshes"][0][1], channels[0]["meshes"][0][2], "Xarment"),
    envelope(payload + b"\0"),
    raw[:-1],
]
swapped_joints = bytearray(payload)
struct.pack_into("<Q", swapped_joints, channels[0]["joint_ids"][0][1], 1)
struct.pack_into("<Q", swapped_joints, channels[0]["joint_ids"][1][1], 0)
malformed.append(envelope(swapped_joints))
without_baseline = bytearray(payload)
without_baseline[baseline_flag] = 0
del without_baseline[baseline_flag + 1:baseline_end]
unknown_input = bytearray(payload)
unknown_input[channels[0]["source_start"]:channels[0]["source_end"]] = string("heighx")
unknown_input[baseline_flag] = 0
del unknown_input[baseline_flag + 1:baseline_end]
malformed.append(envelope(unknown_input))
no_baseline_path = path.parent / "MorphNoBaseline.ii3dproject"
no_baseline_path.write_bytes(envelope(without_baseline))
no_baseline = subprocess.run([executable, "file", str(no_baseline_path)], capture_output=True, text=True)
assert no_baseline.returncode == 0, no_baseline.stderr
valid = subprocess.run([executable, "file", str(path)], capture_output=True, text=True)
assert valid.returncode == 0, valid.stderr
for index, data in enumerate(malformed):
    bad = path.parent / f"MorphMalformed-{index}.ii3dproject"
    bad.write_bytes(data)
    result = subprocess.run([executable, "file", str(bad)], capture_output=True, text=True)
    assert result.returncode == 1, (index, result.returncode, result.stderr)
    assert "object.valid()" not in result.stderr and "next.id()" not in result.stderr, result.stderr
print(f"Independent v21 morph table, source-template reference and {len(malformed)} rollback checks passed")
