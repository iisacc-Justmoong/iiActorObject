"""Check the C++ and Python wire contract without Boost or an application host."""

import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib

executable, fixture_path, output_directory, module_directory = sys.argv[1:]
fixture = pathlib.Path(fixture_path)
sys.path.insert(0, module_directory)
from nscharacter_file import pack_nscharacter_file, unpack_nscharacter_file

actor = fixture.read_bytes()
sheet = '{\n  "표시명": "시험 캐릭터", "creator": {"scale": 1.2500}\n}\n'.encode()


def wire(project, text):
    return struct.pack("<8sIIQQI", b"NSCHAR\r\n", 1, 1, len(project), len(text), zlib.crc32(text)) + project + text


def rejects(operation):
    try:
        operation()
    except ValueError:
        return
    raise AssertionError("Invalid .nscharacter data was accepted")


with tempfile.TemporaryDirectory(prefix="nscharacter-test-", dir=output_directory) as directory:
    path = pathlib.Path(directory) / "export.nscharacter"
    subprocess.run([executable, "write", fixture, path], check=True)
    raw = path.read_bytes()
    assert raw == wire(actor, sheet)
    view = unpack_nscharacter_file(raw)
    assert view.actor_project == actor and view.sheet_json == sheet
    assert view.actor_project.readonly and view.sheet_json.readonly
    assert view.actor_project.obj is raw and view.sheet_json.obj is raw

    path.write_bytes(pack_nscharacter_file(actor, sheet))
    assert path.read_bytes() == raw
    subprocess.run([executable, "read", fixture, path], check=True)

    boost = b"22 serialization::archive 21"
    too_deep = b'{"v":' + b"[" * 63 + b"0" + b"]" * 63 + b"}"
    last_depth = b'{"v":' + b"[" * 62 + b"0" + b"]" * 62 + b"}"
    path.write_bytes(pack_nscharacter_file(actor, last_depth))
    subprocess.run([executable, "inspect", fixture, path], check=True)
    malformed = (
        boost, wire(boost, sheet), raw[:-1], raw + b"\0",
        raw[:8] + b"\x02\0\0\0" + raw[12:],
        raw[:16] + b"\xff" * 8 + raw[24:],
        raw[:-1] + b" ",  # Still legal JSON, with an invalid checksum.
        wire(actor, b"[]"), wire(actor, b'{"x": "\xff"}'),
        wire(actor, br'{"x": 0, "\u0078": 1}'), wire(actor, br'{"x":"\ud800"}'),
        wire(actor, b'{"x": 1e999}'), wire(actor, b'{"x": 1e-999}'),
        wire(actor, b'{"x": 18446744073709551616}'), wire(actor, too_deep),
    )
    for candidate in malformed:
        rejects(lambda: unpack_nscharacter_file(candidate))
        path.write_bytes(candidate)
        result = subprocess.run([executable, "inspect", fixture, path], capture_output=True)
        assert result.returncode == 1, result.stderr

    rejects(lambda: pack_nscharacter_file(boost, sheet))
    rejects(lambda: pack_nscharacter_file(actor, b"[]"))
    rejects(lambda: pack_nscharacter_file(actor, sheet, max_actor_bytes=len(actor) - 1))
    rejects(lambda: unpack_nscharacter_file(raw, max_sheet_bytes=len(sheet) - 1))
    assert unpack_nscharacter_file(raw, max_actor_bytes=len(actor), max_sheet_bytes=len(sheet)).sheet_json == sheet

print("Independent .nscharacter wire, C++/Python interoperability and rejection checks passed")
