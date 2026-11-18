#!/usr/bin/env python3
"""Assert the first-party Apache-2.0 grant and preserved third-party notices."""

import pathlib
import sys


def read(path):
    return pathlib.Path(path).read_text(encoding="utf-8")


def require(path, needle):
    text = read(path)
    if needle not in text:
        raise SystemExit(f"{path} does not contain {needle!r}")
    return text


def main():
    root = pathlib.Path(sys.argv[1]).resolve()
    generated = pathlib.Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

    license_text = require(root / "LICENSE", "Apache License")
    require(root / "LICENSE", "Version 2.0, January 2004")
    require(root / "LICENSE", "END OF TERMS AND CONDITIONS")
    if license_text.lstrip().startswith("MIT License"):
        raise SystemExit("root LICENSE is still the MIT license")

    require(root / "NOTICE", "Copyright (c) 2026 iisacc")
    readme = require(root / "README.md", "SPDX-License-Identifier: Apache-2.0")
    if "are MIT licensed" in readme:
        raise SystemExit("README still claims original code is MIT licensed")
    require(root / "docs" / "License.md", "SPDX-License-Identifier: Apache-2.0")
    require(root / "AGENT.md", "Apache License 2.0")
    actor = require(root / "docs" / "Actor.md", "SPDX-License-Identifier: Apache-2.0")
    if "SPDX-License-Identifier: MIT" in actor or "MIT 라이선스로 제공" in actor:
        raise SystemExit("docs/Actor.md still claims the MIT license")
    changes = read(root / "docs" / "Changes-20261021.md")
    if "자체 코드·문서의 MIT 조건을 유지" in changes:
        raise SystemExit("Changes-20261021.md still says first-party code remains MIT")
    require(root / "docs" / "Changes-20261021.md", "Apache License 2.0")

    require(root / "CMakeLists.txt", 'set(iiActorObject_SPDX_LICENSE "Apache-2.0")')
    require(root / "CMakeLists.txt", "LICENSE NOTICE README.md")
    require(
        root / "cmake" / "iiActorObjectConfig.cmake.in",
        'set(iiActorObject_SPDX_LICENSE "Apache-2.0")',
    )
    require(
        root / "example" / "VisualCodec" / "CMakeLists.txt",
        'set(iiActorObjectVisualCodec_SPDX_LICENSE "Apache-2.0")',
    )
    require(
        root / "example" / "VisualCodec" / "cmake" / "iiActorObjectVisualCodecConfig.cmake.in",
        'set(iiActorObjectVisualCodec_SPDX_LICENSE "Apache-2.0")',
    )

    ufbx = read(root / "src" / "third_party" / "ufbx" / "LICENSE")
    if "ALTERNATIVE A - MIT License" not in ufbx or "Copyright (c) 2020 Samuli Raivio" not in ufbx:
        raise SystemExit("ufbx license text was altered")
    epic = read(root / "src" / "third_party" / "openriglogic" / "LICENSE")
    if not epic.startswith("MIT License") or "Copyright (c) 2026 Epic Games, Inc." not in epic:
        raise SystemExit("OpenRigLogic license text was altered")
    mikk = read(root / "src" / "third_party" / "mikktspace" / "LICENSE")
    if "Copyright (C) 2011 by Morten S. Mikkelsen" not in mikk:
        raise SystemExit("MikkTSpace license text was altered")
    for name in ("MakeHuman", "MakeHumanFace", "MakeHumanMacro", "MakeHumanExpressions"):
        upstream = read(root / "data" / name / "LICENSE.md")
        if "GNU Affero General Public License" not in upstream:
            raise SystemExit(f"{name} upstream license text was altered")

    if generated is not None:
        config = read(generated)
        if 'set(iiActorObject_SPDX_LICENSE "Apache-2.0")' not in config:
            raise SystemExit("generated package config does not export Apache-2.0")
    print("license text ok")


if __name__ == "__main__":
    main()
