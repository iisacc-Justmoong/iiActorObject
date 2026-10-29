"""Small independent FBX 7.4 binary reader used by format tests."""
import struct
import zlib


def read_fbx(raw):
    assert raw[:23] == b"Kaydara FBX Binary  \x00\x1a\x00"
    assert struct.unpack_from("<I", raw, 23)[0] == 7400

    def properties(start, count, length):
        end = start + length
        values = []
        for _ in range(count):
            code = chr(raw[start])
            start += 1
            if code in "YCILFD":
                form = {"Y": "h", "C": "?", "I": "i", "L": "q", "F": "f", "D": "d"}[code]
                values.append(struct.unpack_from("<" + form, raw, start)[0])
                start += struct.calcsize(form)
            elif code in "SR":
                size = struct.unpack_from("<I", raw, start)[0]
                start += 4
                value = raw[start:start + size]
                values.append(value.decode("utf-8") if code == "S" else value)
                start += size
            elif code in "fdilbc":
                count, encoding, size = struct.unpack_from("<III", raw, start)
                start += 12
                payload = raw[start:start + size]
                start += size
                if encoding == 1:
                    payload = zlib.decompress(payload)
                else:
                    assert encoding == 0
                form = {"f": "f", "d": "d", "i": "i", "l": "q", "b": "?", "c": "b"}[code]
                values.append(struct.unpack("<" + form * count, payload))
            else:
                raise AssertionError(f"Unknown FBX property {code!r}")
        assert start == end
        return values

    def nodes(start, limit):
        result = []
        while start + 13 <= limit:
            end, count, size, name_size = struct.unpack_from("<IIIB", raw, start)
            if not end:
                break
            assert start < end <= limit
            name = raw[start + 13:start + 13 + name_size].decode("ascii")
            begin = start + 13 + name_size
            assert begin + size <= end
            result.append((name, properties(begin, count, size), nodes(begin + size, end)))
            start = end
        return result

    return nodes(27, len(raw))


def child(node, name):
    matches = [entry for entry in node[2] if entry[0] == name]
    assert len(matches) == 1
    return matches[0]
