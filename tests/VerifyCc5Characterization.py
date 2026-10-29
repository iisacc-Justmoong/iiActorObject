"""Compare SDK-generated name mappings with user-supplied CC5 profile sections."""
import pathlib
import subprocess
import sys


def bone_map(data):
    result = {}
    in_map = False
    for line in data.decode("utf-8-sig").splitlines():
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            in_map = line == "[BoneMap]"
        elif in_map and line and not line.startswith(("#", ";")):
            name, role = (part.strip() for part in line.split("=", 1))
            assert name and role and name not in result
            result[name] = role
    assert result
    return result


for mode, path, exact in (("profile_g5", sys.argv[2], True),
                          ("profile_blender", sys.argv[3], False)):
    reference = bone_map(pathlib.Path(path).read_bytes())
    actual = bone_map(subprocess.run([sys.argv[1], mode], capture_output=True,
                                     check=True).stdout)
    assert all(actual.get(name) == role for name, role in reference.items()), mode
    if exact:
        assert actual == reference, mode
print("CC5 G5 and Blender body mapping sections match user-supplied profiles")
