"""Read and write the independent .nscharacter envelope; actor restoration is an SDK operation."""

import json
import math
import struct
import zlib
from typing import NamedTuple

_HEADER = struct.Struct("<8sIIQQI")
_SIGNATURE = b"NSCHAR\r\n"
_VERSION = 1
_ACTOR_ENCODING = 1
_ACTOR_HEADER = struct.Struct("<8sIQI")
_ACTOR_SIGNATURE = b"iiAOPRJ\0"
_ACTOR_BYTES = 256 * 1024 * 1024
_SHEET_BYTES = 4 * 1024 * 1024


class NSCharacterFileView(NamedTuple):
    actor_project: memoryview
    sheet_json: memoryview


def _view(data):
    return memoryview(data).cast("B").toreadonly()


def _check_actor(actor):
    if len(actor) < _ACTOR_HEADER.size:
        raise ValueError("Missing native .ii3dproject actor")
    signature, version, payload_bytes, _ = _ACTOR_HEADER.unpack_from(actor)
    if signature != _ACTOR_SIGNATURE or version == 0 or payload_bytes != len(actor) - _ACTOR_HEADER.size:
        raise ValueError("Invalid native .ii3dproject actor envelope")


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate character sheet key")
        result[key] = value
    return result


def _integer(text):
    if len(text.lstrip("-")) > 20:
        raise ValueError("Character sheet integer out of range")
    value = int(text)
    if not -(1 << 63) <= value < 1 << 64:
        raise ValueError("Character sheet integer out of range")
    return value


def _real(text):
    value = float(text)
    significand = text.lower().partition("e")[0]
    if not math.isfinite(value) or (value == 0 and any("1" <= ch <= "9" for ch in significand)):
        raise ValueError("Character sheet real out of finite binary64 range")
    return value


def _invalid_constant(_):
    raise ValueError("Character sheet requires finite JSON numbers")


def _check_sheet(sheet):
    try:
        root = json.loads(bytes(sheet).decode("utf-8"), object_pairs_hook=_unique_object,
                          parse_int=_integer, parse_float=_real, parse_constant=_invalid_constant)
    except (UnicodeError, RecursionError) as error:
        raise ValueError("Invalid UTF-8 character sheet") from error
    if not isinstance(root, dict):
        raise ValueError("Character sheet must be a JSON object")

    items = 0

    def item():
        nonlocal items
        items += 1
        if items > 100_000:
            raise ValueError("Character sheet item limit exceeded")

    def check_string(value):
        if len(value.encode("utf-8")) > 1024 * 1024:
            raise ValueError("Character sheet string byte limit exceeded")

    def visit(value, depth):
        item()
        if depth > 64:
            raise ValueError("Character sheet nesting limit exceeded")
        if isinstance(value, dict):
            for key, child in value.items():
                item()
                check_string(key)
                visit(child, depth + 1)
        elif isinstance(value, list):
            for child in value:
                visit(child, depth + 1)
        elif isinstance(value, str):
            check_string(value)

    visit(root, 1)


def pack_nscharacter_file(actor_project, sheet_json, *, max_actor_bytes=_ACTOR_BYTES,
                          max_sheet_bytes=_SHEET_BYTES):
    """Pack native project and original UTF-8 JSON bytes without re-serializing either."""
    actor, sheet = _view(actor_project), _view(sheet_json)
    if len(actor) > max_actor_bytes or len(sheet) > max_sheet_bytes:
        raise ValueError(".nscharacter section byte limit exceeded")
    _check_actor(actor)
    _check_sheet(sheet)
    header = _HEADER.pack(_SIGNATURE, _VERSION, _ACTOR_ENCODING, len(actor), len(sheet), zlib.crc32(sheet))
    return b"".join((header, actor, sheet))


def unpack_nscharacter_file(data, *, max_actor_bytes=_ACTOR_BYTES, max_sheet_bytes=_SHEET_BYTES):
    """Return read-only views; keep the backing bytes unmodified while using them."""
    raw = _view(data)
    if len(raw) < _HEADER.size:
        raise ValueError("Incomplete .nscharacter header")
    signature, version, encoding, actor_bytes, sheet_bytes, sheet_crc = _HEADER.unpack_from(raw)
    if signature != _SIGNATURE:
        raise ValueError("Invalid .nscharacter signature")
    if version != _VERSION or encoding != _ACTOR_ENCODING:
        raise ValueError("Unsupported .nscharacter version or actor encoding")
    if actor_bytes + sheet_bytes != len(raw) - _HEADER.size:
        raise ValueError("Inconsistent .nscharacter section lengths")
    if actor_bytes > max_actor_bytes or sheet_bytes > max_sheet_bytes:
        raise ValueError(".nscharacter section byte limit exceeded")
    actor_end = _HEADER.size + actor_bytes
    actor, sheet = raw[_HEADER.size:actor_end], raw[actor_end:]
    if zlib.crc32(sheet) != sheet_crc:
        raise ValueError("Corrupt .nscharacter sheet checksum")
    _check_actor(actor)
    _check_sheet(sheet)
    return NSCharacterFileView(actor, sheet)
