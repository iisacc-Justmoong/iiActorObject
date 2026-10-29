"""독립적인 GLB 경계 검사이다. C++ 인코더를 불러오거나 런타임에 포함하지 않는다."""
import json
from pathlib import Path
import struct
import sys


def read_glb(path):
    data = Path(path).read_bytes()
    magic, version, total = struct.unpack_from("<III", data)
    assert magic == 0x46546C67 and version == 2 and total == len(data)
    size, kind = struct.unpack_from("<II", data, 12)
    assert kind == 0x4E4F534A and size % 4 == 0
    document = json.loads(data[20:20 + size])
    offset = 20 + size
    length, kind = struct.unpack_from("<II", data, offset)
    assert kind == 0x004E4942 and offset + 8 + length == total
    binary = data[offset + 8:]
    assert document["buffers"][0]["byteLength"] <= length
    return document, binary


def accessor(document, binary, index):
    a = document["accessors"][index]
    arity = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    char = {5123: "H", 5125: "I", 5126: "f"}[a["componentType"]]
    step = struct.calcsize("<" + char * arity)
    result = [[0] * arity for _ in range(a["count"])]
    if "bufferView" in a:
        view = document["bufferViews"][a["bufferView"]]
        start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = view.get("byteStride", step)
        assert (a["count"] - 1) * stride + step + a.get("byteOffset", 0) <= view["byteLength"]
        result = [list(struct.unpack_from("<" + char * arity, binary, start + i * stride)) for i in range(a["count"])]
    if "sparse" in a:
        s = a["sparse"]
        iv = document["bufferViews"][s["indices"]["bufferView"]]
        vv = document["bufferViews"][s["values"]["bufferView"]]
        ids = struct.unpack_from("<" + "I" * s["count"], binary, iv.get("byteOffset", 0))
        assert list(ids) == sorted(set(ids)) and max(ids, default=-1) < len(result)
        for i, vertex in enumerate(ids):
            result[vertex] = list(struct.unpack_from("<" + char * arity, binary, vv.get("byteOffset", 0) + i * step))
    return result


def main():
    d, binary = read_glb(sys.argv[1])
    assert d["asset"]["version"] == "2.0"
    assert d["materials"][0]["alphaMode"] == "BLEND"
    assert d["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][3] == .6
    skin = d["skins"][0]
    assert [d["nodes"][i]["name"] for i in skin["joints"]] == ["root", "pelvis", "head"]
    assert d["nodes"][skin["joints"][1]]["children"] == [skin["joints"][2]]
    ibm = accessor(d, binary, skin["inverseBindMatrices"])
    assert len(ibm) == 3 and ibm[2][13] == -2
    mesh = d["meshes"][0]
    primitive = mesh["primitives"][0]
    attributes = primitive["attributes"]
    positions = accessor(d, binary, attributes["POSITION"])
    assert positions == [[-1, -1, 0], [1, -1, 0], [1, 1, 0], [-1, 1, 0]]
    assert accessor(d, binary, primitive["indices"]) == [[0], [1], [2], [0], [2], [3]]
    assert mesh["extras"]["targetNames"] == ["smile", "translate"]
    delta = accessor(d, binary, primitive["targets"][0]["POSITION"])
    assert delta == [[0, 0, 0], [0, 0, 0], [0, 0, .5], [0, 0, 0]]
    translation = primitive["targets"][1]["POSITION"]
    assert accessor(d, binary, translation) == [[.25, .5, .75]] * 4
    assert d["accessors"][translation]["min"] == [.25, .5, .75]
    assert d["accessors"][translation]["max"] == [.25, .5, .75]
    joints = accessor(d, binary, attributes["JOINTS_0"])
    weights = accessor(d, binary, attributes["WEIGHTS_0"])
    assert joints[0] == [1, 0, 0, 0] and joints[2] == [2, 0, 0, 0]
    assert all(abs(sum(v) - 1) < 1e-6 for v in weights)
    # Independent reference deformation: head translation +1 X and smile=.5.
    actual = [positions[2][i] + .5 * delta[2][i] + (1 if i == 0 else 0) for i in range(3)]
    assert actual == [2, 1, .25]
    print("GLB container, sparse morph, hierarchy, inverse bind, skin and reference deformation passed")


if __name__ == "__main__":
    main()
