# BlackLight3D

BlackLight Engine is a C++17 engine SDK backed by RmlUi. Each build produces a shared engine DLL, a static library variant, and a distributable ZIP package containing the libraries and public headers. The public API lives in `BlackLight/include/rmlui`; XML parsing and RmlUi integration are implemented under `BlackLight/src/rmlui`.

## Build

The project uses CMake with the Ninja generator. CMake first looks for RmlUi 6.3 and TinyXML2 11 packages. If either is missing, the default configuration fetches those pinned versions. Run these commands from a Visual Studio Developer PowerShell with CMake and Ninja available:

```powershell
$env:PATH = "D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
cmake --build --preset x64-debug --target blacklight_engine_package
```

The four presets are `x64-debug`, `x64-release`, `x86-debug`, and `x86-release`. Their build trees and target artifacts are kept under `build/{x64|x86}/{debug|release}`.

The package contains `blacklight_engine_<arch>.dll`, its import library,
`blacklight_engine_<arch>_static.lib`, and the public `include/*.h` headers.

## Engine Runtime

The public engine runtime now includes a host-independent `Game` loop, fixed-step
timing, platform input snapshots, generation-safe ECS entities, rooted content
caching, math primitives, and a deferred `SpriteBatch` command collector. Start
from `BlackLight/include/engine/Game.h` when building a game host. The runtime
keeps rendering behind `IGraphicsBackend`, so a DirectX or Vulkan backend can be
added without changing gameplay code.

The implementation notes and the current RmlUi backend boundary are documented in
[`docs/engine-core.md`](docs/engine-core.md).

To require locally installed packages, configure with `-DBLACKLIGHT_FETCH_DEPENDENCIES=OFF`. Disable tests with `-DBLACKLIGHT_BUILD_TESTS=OFF`.

## XML

`Screen` is the required root. Nested `Panel` elements are containers, and non-whitespace text between elements becomes a text node. `c:id` is preferred; plain `id` is also accepted, but the two cannot be used on the same element. IDs are optional, unique within a screen, and cannot contain whitespace. The parser accepts the literal `c:id` attribute even when no `xmlns:c` declaration is present; the sample includes a declaration to remain well-formed XML.

```xml
<Screen xmlns:c="urn:blacklight:ui" c:id="screen_id">
  <Panel c:id="panel_id" auto_size="true" width="320">
    Panel content
    <Panel c:id="nested_panel" height="50%" />
  </Panel>
</Screen>
```

`auto_size` applies to containers and defaults to `false`. For each axis, an explicit `width` or `height` wins. An unspecified axis becomes content-sized when `auto_size="true"`, or fills the parent when `auto_size="false"`. Bare non-negative numbers are interpreted as pixels; supported dimensions are `px`, `%`, `dp`, `em`, and `auto`. Panels lay out children vertically.

## Runtime

The host application initializes RmlUi and its renderer/font interfaces, then parses and builds the screen using its existing `Rml::Context`:

```cpp
std::unique_ptr<BlackLight::UI::UIScreen> screen;
BlackLight::UI::UIParseError parse_error;
if (!BlackLight::UI::UIXmlParser::Parse(xml, screen, parse_error)) {
    // Handle parse_error.message and parse_error.line.
}

std::unique_ptr<BlackLight::UI::RmlUiTree> ui;
BlackLight::UI::UIBuildError build_error;
if (!BlackLight::UI::RmlUiTree::Build(context, *screen, ui, build_error)) {
    // Handle build_error.message.
}
ui->Show();
```

The tree owns its RmlUi document but not the context. Close or destroy every `RmlUiTree` before shutting down its context. `FindById` resolves the source `c:id` to its native RmlUi element.
