# Static UI Runtime and Embedded BUI

BlackLight builds the fetched RmlUi 6.3 and FreeType 2.13.3 as static libraries.
The Demo links `BlackLight::EngineStatic`, so RmlUi, FreeType, and the engine
are linked into its executable. Neither `rmlui.dll` nor `freetyped.dll` needs
to be distributed with the Demo.

The dependency build uses a local `BUILD_SHARED_LIBS=OFF` scope. Existing build
directories that cached `BUILD_SHARED_LIBS=ON` can be reconfigured directly.
An installed RmlUi package must provide a static `RmlUi::Core` target; a shared
target produces a configuration error. Installed packages and their transitive
dependencies must also use static libraries.

## Embed Application UI

Use the build helper after creating the application target:

```cmake
target_link_libraries(my_app PRIVATE BlackLight::EngineStatic)
blacklight_embed_bui(my_app
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    NAMESPACE AppUi
    FILES Main.bui Dialogs/Settings.bui
)
```

Paths in `FILES` are relative to `ROOT` and are the names used for lookup.
The helper generates `my_app_bui.h` and a private `.cpp` under the build
directory. The header contains only the declaration of
`AppUi::Find(std::string_view) noexcept`. The implementation and resource
bytes are compiled into the application. No resources are extracted at runtime.
Each application target should call the helper once.

Include the generated header and pass its resource bytes to the existing parser:

```cpp
#include "my_app_bui.h"
#include "rmlui/BuiLoader.h"

/// Parses the application's embedded main document.
bool LoadMainUi(std::unique_ptr<BlackLight::UI::BuiDocument>& document,
                BlackLight::UI::BuiLoadError& error)
{
    return BlackLight::UI::BuiLoader::Parse(
        AppUi::Find("Main.bui"), document, error);
}
```

Lookup is case-sensitive. A missing name returns an empty view, which `Parse`
reports as empty BUI content. The view remains valid for the lifetime of its
module. Original file bytes, including UTF-8 text and line endings, are retained.

Editing a listed `.bui` file regenerates the embedded source on the next build.
Add new resources to `FILES`. The Demo embeds `Demo/ui/Main.bui`; deployment
does not require a `UI` directory. External images, fonts, linked stylesheets,
or other files referenced from BUI are separate resources and are not embedded
by this helper. The current Demo loads Arial from the Windows Fonts directory.

## SDK Consumers

The ZIP includes the engine libraries, RmlUi and FreeType static libraries,
RmlUi headers, and `cmake/BlackLightEmbedBui.cmake` with its generator script.
For a separate CMake application, include that helper from the extracted SDK.

When linking the static SDK manually with MSVC, define `BLACKLIGHT_STATIC` and
`RMLUI_STATIC_LIB`, add the SDK's `include` directory, and link the static engine,
`rmlui.lib`, the matching FreeType static library (`freetyped.lib` in Debug),
and the Windows libraries `opengl32`, `d3d11`, `dxgi`, and `comctl32`. Use the
same architecture, configuration, and MSVC runtime settings as the SDK build.
These separate static archives are build inputs, not deployment files.

Build without the optional test targets:

```powershell
cmake --preset x64-debug -DBLACKLIGHT_BUILD_TESTS=OFF
cmake --build --preset x64-debug --target blacklight_demo
```

## Build Verification

Verified on 2026-11-11 with MSVC 19.51:

- x64 Debug and Release Demo, engine DLL, static engine, and SDK ZIP built.
- The Demo and engine DLL import tables have no RmlUi or FreeType DLL imports.
- A copy of the Release executable launched from a new directory containing
  only that executable. It parsed the embedded BUI, created its RmlUi tree,
  and resolved `start_button` and `title_panel`.
- The Release process loaded no RmlUi, FreeType, or engine DLL module, created
  only its log beside the executable, and exited with code 0 after a native
  window close request.
- A new build directory configured successfully using the pinned source trees.
  The dependency scope selects static libraries even with a shared-build cache.

The optional test targets were disabled; no test suite was added or run.
Windows graphics libraries and the MSVC runtime remain normal platform
dependencies. x86 builds were not verified.
