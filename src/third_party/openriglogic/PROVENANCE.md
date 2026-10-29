# OpenRigLogic source provenance

- Upstream: https://github.com/EpicGames/openriglogic
- Revision: `55756aca0c4c55adc56ad68a796bcc1c0bf6fa5a`
- Version: RigLogic 13.2.9-hotfix1 (upstream UE 5.8.4 snapshot).
- License: MIT; see `LICENSE`.
- Copied paths: `include/`, `src/`, `cmake/RLDependencies.cmake`, and `LICENSE`.
- Source contents are unmodified; `UPSTREAM-SHA256.json` records every copied file.
- iiActorObject builds these C++ sources with its own C++23-only CMake wrapper.
- C++23 compatibility: the wrapper supplies the standard `<type_traits>` header through a private build PCH because upstream ScopedPtr.h relies on a removed transitive include. Source files remain unchanged.
- Python bindings, application SDKs, commercial sample DNA/assets, and upstream build downloads are not included.
