# Build Layout

All CMake-generated target artifacts are placed under an architecture and configuration directory:

```text
build/
  x64/
    debug/
    release/
  x86/
    debug/
    release/
```

Use the matching CMake preset to configure and build:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
```

The `BLACKLIGHT_ARCH` cache variable controls the architecture segment and is restricted to `x64` or `x86`. The output directory is configured globally for archive, library, runtime, and per-configuration outputs, so the BlackLight targets and fetched dependency targets use the same layout.

The engine targets are:

- `blacklight_engine_<arch>.dll`: shared engine runtime.
- `blacklight_engine_<arch>.lib`: MSVC import library for the DLL.
- `blacklight_engine_<arch>_static.lib`: static engine variant.

Run CPack from the configured build directory to create a ZIP containing these libraries and the public headers:

```powershell
cmake --build --preset x64-debug --target blacklight_engine_package
```
