# Dynamic linking

The optional VisualCodec adapter follows `BUILD_SHARED_LIBS` and exports its
own Windows API symbols. Shared iOS configurations use frameworks and
WebAssembly configurations use side modules. Explicit static builds remain
available by disabling `BUILD_SHARED_LIBS`.

The Emscripten package configuration enables CMake shared-library imports for side modules. The builder or application must supply the main-module compile and link options.
