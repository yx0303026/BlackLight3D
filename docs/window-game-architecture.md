# Window and Game Hosts

## Structure

Application hosts use one of two paths, with a shared desktop lifecycle:

```text
Window (abstract)
  +-- DesktopApplication : Window
  +-- Game (abstract)
        +-- ApplicationGame : Game
```

`Window` owns the native backend, keyboard/mouse input, RmlUi context, message
pump, rendering order, and presentation. It allocates no World, ContentManager,
FrameClock, or SpriteBatch. `Game` adds those services through private storage.
Both hosts keep state and implementation in `.cpp` files through PImpl. Public
headers declare configuration, lifecycle, and extension APIs.

This is a runtime separation within the existing engine library. It does not
produce a separate UI-only DLL or remove game object code from the shared DLL.
Existing service APIs, STL parameters, and the graphics backend vtable still
require a compatible C++ toolchain and rebuilding SDK clients after this change.

## Desktop Application

```cpp
#include "engine/Window.h"
#include "rmlui/HtmlCssParser.h"
#include "rmlui/RmlUiTree.h"

/// Implements a desktop application using the lightweight engine path.
class DesktopApplication final : public BlackLight::Engine::Window
{
protected:
    /// Builds application UI with GetContext() after the backend is ready.
    bool OnInitialize() override
    {
        return true;
    }

    /// Releases application UI before its context is destroyed.
    void OnShutdown() override {}
};

/// Runs the engine-owned desktop lifecycle.
int main()
{
    DesktopApplication application;
    return application.Run();
}
```

The working HTML example is `Demo/src/main.cpp`. Desktop applications may
override parameterless `OnUpdate()` and `OnRender()` when needed. RmlUi drawing
is automatic; a desktop application does not have to implement a render loop.

## Game Application

```cpp
#include "engine/Engine.h"

/// Implements the full game host rather than a standalone backend loop.
class ApplicationGame final : public BlackLight::Engine::Game
{
protected:
    /// Loads content and creates entities after graphics initialization.
    bool OnInitialize() override
    {
        GetWorld().CreateEntity();
        return true;
    }

    /// Advances gameplay using the configured fixed time step.
    void OnFixedUpdate(const BlackLight::Engine::GameTime &time) override
    {
        (void)time;
    }

    /// Submits game scene graphics before the host draws the UI overlay.
    void OnRender(const BlackLight::Engine::GameTime &time) override
    {
        (void)time;
    }
};

/// Runs the engine-owned game lifecycle.
int main()
{
    ApplicationGame application;
    return application.Run();
}
```

`GameConfig` extends `WindowConfig`, retaining `window` and `backend_name` and
adding `fixed_step_seconds`, `maximum_delta_seconds`, and `content_root`.
`GetInput()`, `GetContext()`, `Initialize()`, `Run()`, `Exit()`, and `Shutdown()`
are inherited. `GetGameTime()`, `GetWorld()`, `GetContent()`, and
`GetSpriteBatch()` expose game services.

## Lifecycle

1. `Initialize()` creates the backend, connects input, resets game timing when
   applicable, and invokes `OnInitialize()`. A failed initialization releases
   native resources; partially initialized application resources are released
   through `OnShutdown()`.
2. Each frame starts an input snapshot, pumps messages, and updates application
   state. `Game` samples its clock and dispatches `OnUpdate(GameTime)` followed
   by zero or more `OnFixedUpdate(GameTime)` calls.
3. Backend `BeginFrame()` prepares and clears the framebuffer. Application
   `OnRender` draws the scene, backend `Render()` draws UI, and `Present()`
   displays the result. The Windows OpenGL backend implements both stages.
4. `Run()` calls `OnShutdown()` while the context and game services still exist,
   then shuts down the backend. It returns `1` on initialization, frame setup,
   UI rendering, or presentation failure, and `0` on ordinary termination.
   Callback exceptions release resources and propagate to the caller.

`Exit()` requests termination. `Shutdown()` invoked from an active frame or
initialization callback requests termination and defers resource destruction
until the callback completes. Recursive `Run()` and initialization during
initialization/shutdown are rejected.

Application callbacks must not recursively start another lifecycle or destroy
their own host. Hooks run on the calling thread; these hosts do not provide
thread-safe lifecycle control. `OnShutdown()` should not throw.

When using `Initialize()` without `Run()`, explicitly call `Shutdown()` while
the concrete application is still alive, including before destroying it. Base
destructors cannot dispatch cleanup to derived hooks. Native backend release
still occurs on destruction. Destroy all UI trees and GPU resources before
their context; `DemoWindow::OnShutdown()` illustrates this ordering.

## Migration and Scope

- Replace a hand-written application backend loop with a subclass of `Window`
  or `Game`. Graphics backends remain service implementations of
  `IGraphicsBackend`; they are not application hosts.
- Rename game `OnDraw(const GameTime&)` overrides to
  `OnRender(const GameTime&)`. `Game` is abstract until this hook is implemented.
- Include `Ecs.h`, `Content.h`, or `SpriteBatch.h` when using those concrete
  services from a `Game.h` client, or include `Engine.h` for the complete API.
- Custom backends should clear in `BeginFrame()` and draw UI in `Render()`.
  The default `BeginFrame()` is a no-op; backends clearing in `Render()` must
  migrate to preserve application scene graphics.

The game path exposes every game service currently implemented in this
repository. It is not yet feature-equivalent to MonoGame: audio, gamepads,
physics, image decoding, GPU SpriteBatch submission, and a complete graphics
device/resource API remain future work. SpriteBatch still requires a renderer
submission callback. This refactor does not invent placeholder APIs for these
unimplemented systems.

## Validation

The x64 Debug DLL, static library, and migrated Demo are compiled with the
existing Visual Studio 2026 CMake build. Tests are neither added nor run for
this change, following the repository instructions. Interactive visual
behavior is not covered by this build validation.
