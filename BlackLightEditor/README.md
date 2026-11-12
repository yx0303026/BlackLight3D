# BlackLightEditor

BlackLightEditor is the stage-three shell of the BlackLight3D visual editor. It builds
on BlackLightUI (`.bui` + RmlUi) and implements the editor core framework, the project
management system, and the first UI editing loop: application shell, event bus,
configuration, editor state, command system, undo/redo, menus, toolbar, shortcuts,
dockable panels, themes, project generation, `.bui` document editing, and live preview.

The editor is a separate executable; the engine SDK and the Demo are unchanged.
The output is `build/{arch}/{config}/blacklight_editor.exe`.

## Build and run

Run from a Visual Studio Developer PowerShell with CMake available:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target blacklight_editor
ctest --preset x64-debug
& build/x64/debug/blacklight_editor.exe
```

The editor embeds `ui/Editor.bui`, so it needs no runtime UI directory and no
`rmlui.dll` when linked against `BlackLight::EngineStatic`.

## Architecture

The project is split into a UI-free core library and the RmlUi host:

| Layer | Files | Responsibility |
| --- | --- | --- |
| Core | `include/editor/core`, `src/core` | Events, config, state, commands, history, registry, themes |
| Project | `include/editor/project`, `src/project` | Types, metadata, templates, generator, validator, recent projects |
| UI | `include/editor/ui`, `src/ui` | BlackLightUI layer, dock layout, menus, wizard, host, Win32 dialogs |
| Resources | [`ui/Editor.bui`](ui/Editor.bui) | Layout, structural CSS, wizard markup, and theme tokens |

- [`EditorEvent.h`](include/editor/core/EditorEvent.h) provides the synchronous
  `EventBus` used for decoupled notifications.
- [`EditorConfig.h`](include/editor/core/EditorConfig.h) persists INI settings,
  window geometry, panel layout, theme, and recent workspaces.
- [`EditorState.h`](include/editor/core/EditorState.h) owns the open document,
  objects, selection, mode, and status text, and emits change events.
- [`Commands.h`](include/editor/core/Commands.h) implements new/open/save/close,
  add/delete/rename object, with undo support.
- [`CommandHistory.h`](include/editor/core/CommandHistory.h) maintains undo/redo
  stacks, depth limits, and save-point tracking for the modified marker.
- [`CommandRegistry.h`](include/editor/core/CommandRegistry.h) is the single
  source for menu labels, enablement, check state, and shortcuts.
- [`EditorUi.h`](include/editor/ui/EditorUi.h) owns the `.bui` document and
  scoped listeners; [`PanelManager.h`](include/editor/ui/PanelManager.h) renders
  tabbed docks and defers DOM updates until after input dispatch.
- [`EditorMenu.h`](include/editor/ui/EditorMenu.h) builds dropdowns from the
  command registry; [`EditorApplication.h`](include/editor/ui/EditorApplication.h)
  wires the systems and runs the `Engine::Window` loop.
- [`ProjectTypes.h`](include/editor/project/ProjectTypes.h) defines the project
  kinds, graphics API selection, name and namespace rules, and identifiers.
- [`ProjectFile.h`](include/editor/project/ProjectFile.h) reads and writes the
  line-based `.blacklight` metadata format.
- [`ProjectTemplates.h`](include/editor/project/ProjectTemplates.h) owns the
  embedded, versioned Windows App, BlackLightGame, and BlockLightUIInject
  scaffolds; [`TemplateEngine.h`](include/editor/project/TemplateEngine.h)
  performs `${VARIABLE}` substitution.
- [`ProjectGenerator.h`](include/editor/project/ProjectGenerator.h) writes the
  directory layout and files, while [`ProjectValidator.h`](include/editor/project/ProjectValidator.h)
  checks metadata, generated files, and the engine dependency.
- [`ProjectManager.h`](include/editor/project/ProjectManager.h) owns the open
  project; [`RecentProjects.h`](include/editor/project/RecentProjects.h) persists
  the MRU list; [`ProjectWizard.h`](include/editor/ui/ProjectWizard.h) drives the
  modal dialog defined in `Editor.bui`.

The core and project layers have no RmlUi or engine dependency and are covered by
[`tests/EditorCoreTests.cpp`](tests/EditorCoreTests.cpp).

The UI editor adds a lossless document layer under
`include/editor/uidoc` and `src/uidoc`:

| Layer | Files | Responsibility |
| --- | --- | --- |
| UI document | `include/editor/uidoc`, `src/uidoc` | `.bui` parsing, source ranges, local splices, structural paths |
| Sessions | `UiDocumentSessions.h/.cpp` | Multi-tab documents, selection, per-tab undo, save and save-as |
| Preview | `ui/BuiPreview.h/.cpp` | Synthetic `data-bl-node` layout, Viewport anchoring, click selection |

`BuiDocumentModel` keeps the complete original file text and edits only the
target byte ranges. Attribute and text changes leave every other byte untouched;
insert, remove, and move operations generate or remove complete indented lines
using the document's existing newline spelling (LF or CRLF). The model reparses
after each splice and exposes structural paths such as `0/2/1`, so node identifiers
never become the persisted selection.

UI editing is covered by
[`tests/BuiDocumentTests.cpp`](tests/BuiDocumentTests.cpp), which checks round-trip
fidelity, tolerant parsing, local attribute/text rewrites, structural edits,
path mapping, per-tab undo, save, save-as, and close behavior without RmlUi.

## UI behavior

- Menu bar: File, Edit, View, Build, Tools, Help. Items are generated from the
  command registry and show shortcuts and checked state.
- Toolbar: new/open workspace, new project, save, undo/redo, add/delete object,
  and edit/play/pause mode.
- Docks: Scene, Assets, Folder, and Hierarchy on the left, Viewport in the center,
  Inspector on the right, Output and History at the bottom. Folder is a lazy
  project browser; Hierarchy edits the active `.bui` layout. Tabs can be switched,
  closed from the tab bar, toggled from View, and restored with **Reset Layout**.
- Themes: **Dark**, **Light**, and **Midnight**. The `.bui` stylesheet uses
  `@token` colors that are replaced at load time and when the theme changes.
- Icons: UI glyphs come from the Windows icon font library — **Segoe Fluent
  Icons** on Windows 11 with **Segoe MDL2 Assets** as the fallback. Both are
  registered as the `BlackLightIcons` family, and glyph codepoints live in
  [`include/editor/ui/EditorIcons.h`](include/editor/ui/EditorIcons.h). Tab close
  buttons fall back to a text glyph when no icon font is available.

## Shortcuts

| Command | Shortcut |
| --- | --- |
| New Project / Open Project | Ctrl+Shift+N / Ctrl+Shift+O |
| New / Open / Save / Save As / Close workspace | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S / Ctrl+W |
| Undo / Redo | Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z |
| Delete selected object | Delete |
| Toggle Scene / Assets / Inspector / Output / History | Ctrl+1 .. Ctrl+5 |
| New / Open / Save / Save As / Close UI document | Ctrl+Alt+N / Ctrl+Alt+O / Ctrl+Alt+S / Ctrl+Alt+Shift+S / Ctrl+Alt+W |
| Move selected UI node | Alt+Up / Alt+Down |
| Exit | Ctrl+Q |

When a `.bui` document is active, Ctrl+Z/Ctrl+Y and the toolbar undo/redo buttons
operate on that tab's own history. Without a UI document they continue to operate
on the workspace command history.

## Project system

**File > New Project...** opens the modal wizard defined in `Editor.bui`. The
wizard selects a project type, name, namespace, location, and (for executable
projects) the graphics API. The engine SDK is not configured by hand: when the
project is created, the editor downloads the latest
`blacklight_engine_x64_release.zip` asset from the
[BlackLight GitHub releases](https://github.com/yx0303026/BlackLight3D/releases),
extracts it under `%LOCALAPPDATA%\BlackLightEditor\Engine\<tag>`, and records that
directory in the project. Cached releases are reused, and the editor falls back
to the newest cached SDK when GitHub cannot be reached.

| Project type | Generated target | Notes |
| --- | --- | --- |
| `WindowsApp` | Executable | `Engine::Window` host; OpenGL, DirectX 11, or DirectX 12 selection |
| `BlackLightGame` | Executable | `Engine::Game` loop with variable and fixed updates, `assets/` content root |
| `BlockLightUIInject` | DLL | Exported plugin API, `.def` export table, and `DllMain` shell |

Template version `1.1.0` creates `ui/Main.bui` with a single centered `Hello Light`
button. WindowsApp and BlackLightGame enable UI, load the Windows Arial font, and
embed this document into the executable through the SDK's `blacklight_embed_bui`
helper. Editing the document and rebuilding updates the interface without a runtime
UI directory. The document is created only once, so scaffolding updates preserve
user changes. Existing projects keep their source and UI files during an update.

The default interface currently renders with OpenGL. DirectX backends have no
engine UI adapter yet, so the generated host logs a warning and continues without
UI. The injection template includes the same document for a host overlay to load;
the DLL shell does not create its own window or install rendering hooks.

Every project records the prebuilt engine SDK release it was generated against and
is created with this layout:

```text
MyProject/
  project.blacklight      # metadata, engine dependency, template version
  CMakeLists.txt          # regenerated by the editor
  CMakePresets.json       # debug/release Ninja presets
  src/  include/  assets/  ui/  cmake/  build/
```

`project.blacklight` is a line-based format:

```text
blacklight-project 1
name = My Game
id = {00000000-0000-4000-8000-000000000000}
kind = BlackLightGame
namespace = MyGame
graphics = OpenGL
engine_root = C:/Users/you/AppData/Local/BlackLightEditor/Engine/v0.0.1
engine_version = v0.0.1
template_version = 1.1.0
created = 2026-11-11
dependency = BlackLight | v0.0.1 | C:/Users/you/AppData/Local/BlackLightEditor/Engine/v0.0.1
```

The generator supports:

- `${VARIABLE}` substitution for project name, namespace, identifier, target,
  engine SDK root, engine version, template version, and date. `$${` escapes a
  literal `${` so generated CMake files can keep `${CMAKE_BINARY_DIR}`.
- Template version tracking. **Tools > Update Project Scaffolding** rewrites only
  editor-managed files (`CMakeLists.txt`, `CMakePresets.json`, and project
  metadata) and bumps `template_version`; user source files are never touched.
- Validation through **Build > Validate Project**: metadata, required
  directories and files, the engine SDK, and template freshness are checked
  and reported in the Output panel.
- A persisted recent-projects list shown in the welcome viewport and saved to
  `[recent_projects]` in the editor configuration.

DirectX 12 is selectable in metadata and generated definitions, but the engine
currently exposes only OpenGL and Direct3D 11; DX12 projects are generated with
the Direct3D 11 backend until a DX12 device lands.

## Workspace files

The stage-one `.blw` workspace remains available for the scene/command system:

```text
blacklight-workspace 1
name Untitled Workspace
object obj-1 Group Group 1
object obj-2 Camera Camera 1
```

Unknown lines are ignored for forward compatibility. Projects are the primary
container in stage two; workspaces remain the editable document format that the
later scene editor builds on.

## UI documents

UI layouts live under the project's `ui/` directory, with `ui/Main.bui` as the
default entry document. Each `.bui` file keeps the native container form:

```xml
<bui>
  <styles>/* RCSS */</styles>
  <layout><!-- RML --></layout>
</bui>
```

Open a document from the **Folder** panel. It becomes an independent tab with its
own selection, modified marker, and undo position. The **Hierarchy** panel renders
element and text nodes with expandable paths; **Inspector** exposes `id`, `class`,
and text; **Viewport** shows a live preview in the same RmlUi context. Clicking an
element in the preview selects the matching hierarchy node.

The preview injects `data-bl-node` attributes into a temporary serialization only.
Those markers are never written to the model or the saved file. Saving writes the
model's complete source text as UTF-8, preserving the original `<styles>` block,
comments, whitespace, and line endings outside the edited ranges. New child lines
use the document's existing LF or CRLF convention.

## Configuration

Settings are stored in `%APPDATA%\BlackLight3D\editor.ini`, falling back to the
executable directory when `APPDATA` is unavailable. The file persists:

- `[editor]` theme, history depth, version
- `[layout]` active tab per dock region
- `[panels]` visibility and region per panel
- `[recent]` recent workspace paths
- `[project]` default project location, recent-project limit, last project
- `[recent_projects]` recent project name, path, kind, and timestamp
- `[window]` last client size in density-independent pixels

## Scope

This stage intentionally leaves the asset pipeline, scene editing, general
property editors, build integration, and runtime preview to later stages. The UI
editor is the first real document editor: it covers the folder browser, lossless
`.bui` model, multi-tab sessions, hierarchy edits, `id`/`class`/text properties,
Viewport preview, selection linking, and save/save-as. CSS editing, event binding,
and transform gizmos are deferred to 3.0.x.
