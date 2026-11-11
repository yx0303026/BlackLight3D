# BlackLight3D

BlackLight Engine is a C++17 engine SDK backed by RmlUi. Each build produces a shared engine DLL, a static library variant, and a distributable ZIP package containing the libraries and public headers. The public API lives in `BlackLight/include/rmlui`; HTML parsing and RmlUi integration are implemented under `BlackLight/src/rmlui`.

## Build

The project uses CMake with the Ninja generator. CMake looks for a static RmlUi 6.3 package and otherwise fetches pinned RmlUi 6.3 and FreeType 2.13.3 sources. Both fetched dependencies build as static libraries. Run these commands from a Visual Studio Developer PowerShell with CMake and Ninja available:

```powershell
$env:PATH = "D:\VisualStudio2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
cmake --build --preset x64-debug --target blacklight_engine_package
```

The four presets are `x64-debug`, `x64-release`, `x86-debug`, and `x86-release`. Their build trees and target artifacts are kept under `build/{x64|x86}/{debug|release}`.

The package contains `blacklight_engine_<arch>.dll`, its import library,
`blacklight_engine_<arch>_static.lib`, the UI dependency static libraries, public
headers, and the CMake BUI embedding helper.

The Demo embeds `Demo/ui/Main.bui` into its executable and links the static engine.
It runs without `rmlui.dll`, a FreeType DLL, or a separate `UI` directory.
See [`docs/embedded-ui.md`](docs/embedded-ui.md) for embedding UI in other targets
and linking the static SDK.

The Editor target embeds `BlackLightEditor/ui/Editor.bui` and implements the
stage-one editor core framework: application shell, event bus, configuration,
editor state, commands with undo/redo, menus, toolbar, shortcuts, dockable
panels, and swappable themes. See
[`BlackLightEditor/README.md`](BlackLightEditor/README.md) for the architecture
and controls. Build and run it with:

```powershell
cmake --build --preset x64-debug --target blacklight_editor
& build/x64/debug/blacklight_editor.exe
```

## Engine Runtime

Applications derive from one of two abstract hosts:

- `Window` (`engine/Window.h`) owns the desktop window, input, RmlUi context,
  event loop, UI drawing, and presentation. It does not allocate game services.
- `Game : Window` (`engine/Game.h`) adds variable and fixed updates, game timing,
  generation-safe ECS entities, rooted content caching, and a sprite collector.
  Every concrete game implements `OnRender(const GameTime&)`.

Include `engine/Engine.h` for the full engine convenience API, or `engine/Window.h`
and the UI headers for desktop applications. Application entry points subclass
these hosts instead of managing a graphics backend loop directly. The Demo
implements the lightweight `Window` path.

The runtime keeps graphics API selection behind `RenderingDevice`. The Windows
surface lifecycle supports OpenGL and Direct3D 11; normalized GPU resources,
draw submission, and UI adapters remain incomplete. Current game services are a
runtime foundation: audio, physics, and GPU sprite submission are not supplied.
See [`docs/window-game-architecture.md`](docs/window-game-architecture.md) for
lifecycle details, examples, and migration from `OnDraw`.

See [`docs/rendering-device.md`](docs/rendering-device.md) for the new graphics
interface and its current implementation limits.

To require locally installed packages, configure with `-DBLACKLIGHT_FETCH_DEPENDENCIES=OFF`. Disable tests with `-DBLACKLIGHT_BUILD_TESTS=OFF`.

## HTML

`HtmlCssParser` turns standard HTML and CSS into a `HTMLDocument` tree, which
`RmlUiTree` converts into a native RmlUi document. Element `id` attributes become
unique identifiers, `class` attributes are split on whitespace, and `style`
attributes are normalized to RmlUi-compatible values such as `width: 320px`.
Inline `background` is mapped to `background-color`.

```html
<div id="app" style="display: flex; flex-direction: column; padding: 12px;">
  <button id="confirm" style="width: 120px;">Confirm</button>
</div>
```

## Runtime

The host application initializes RmlUi and its renderer/font interfaces, then parses
and builds the document using its existing `Rml::Context`:

```cpp
std::unique_ptr<BlackLight::UI::HTMLDocument> document;
BlackLight::UI::HTMLParseError parse_error;
if (!BlackLight::UI::HtmlCssParser::Parse(html, css, document, parse_error)) {
    // Handle parse_error.message and parse_error.line.
}

std::unique_ptr<BlackLight::UI::RmlUiTree> ui;
BlackLight::UI::UIBuildError build_error;
if (!BlackLight::UI::RmlUiTree::Build(context, *document, ui, build_error)) {
    // Handle build_error.message.
}
ui->Show();
```

The tree owns its RmlUi document but not the context. Close or destroy every `RmlUiTree`
before shutting down its context. `HTMLContainer::FindById` resolves an element `id` in
the parsed tree, and `RmlUiTree::FindById` resolves it to its native RmlUi element.
