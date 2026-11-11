# Rendering Device

## Current boundary

The engine now exposes `BlackLight::Engine::Rendering::RenderingDevice` instead of
`IGraphicsBackend`. `Window` owns one device and drives the common lifecycle:

1. `ProcessEvents`
2. `BeginFrame`
3. application `OnRender`
4. `RenderUi`
5. `Present`

`DeviceConfig` selects `OpenGL` or `Direct3D11`, and `SurfaceDesc` either creates a
Win32 window or attaches to an existing `HWND` represented by `NativeWindowHandle`.
Initialization and runtime failures throw `RenderingException`.

## Public resources

`RenderingResources.h` defines move-only RAII wrappers for `Buffer`, `Texture`, and
`GraphicsPipeline`. `CommandList` records normalized pipeline, buffer, viewport and
draw operations. The wrappers do not expose WGL, D3D11, or DXGI types in the public
ABI.

The first implementation stage provides real Win32 surface creation, event pumping,
frame clearing, and presentation for both APIs. Resource allocation and normalized
draw execution currently report `RenderingError::UnsupportedOperation` until the
backend-specific resource binding layer is completed; callers must not treat these
operations as no-ops.

## UI integration

RmlUi is optional. `RenderingDevice::GetUiContext()` returns a context only when a
device has a UI adapter installed. The core device interface itself remains usable
without RmlUi.

The OpenGL device installs the official RmlUi 6.3 GL2 renderer and Win32 platform
adapter when `DeviceConfig::enable_ui` is true. It forwards native input, updates
context dimensions on resize, and calls `Context::Update()` and `Context::Render()`
from `RenderUi()`. UI documents must be closed before device shutdown. Render
managers and the core are released while the WGL context is still current.
Platform and renderer implementations remain private to the engine sources.
The Direct3D 11 device currently has no UI adapter and returns a null UI context.

See [Demo RmlUi integration investigation](demo-rmlui-fix.md) for the startup
failure, the upstream sources checked, and verification details.

## Display DPI

The host executable selects its process DPI awareness. The Demo embeds a
PerMonitorV2 manifest; SDK consumers should declare the same awareness in their
own executable. The engine does not change the host process's DPI policy.

For a DPI-aware host, OpenGL surface dimensions and the viewport use native client
pixels. The RmlUi context receives `GetDpiForWindow() / 96` as its dp ratio.
Use `dp` in styles for dimensions that should follow display scaling. The Demo
converts its initial 1280 by 720 dp size to native pixels on the primary display.
Owned windows apply the recommended rectangle on `WM_DPICHANGED`; attached
windows leave that resize decision to their host.

See [Demo DPI investigation](demo-dpi-fix.md) for the observed 150% scaling issue
and the runtime measurements after the fix.

## Shader contract

`ShaderSource` documents an explicit OpenGL and Direct3D11 source variant. The current
tree does not ship glslang, shaderc, DXC, or SPIRV-Cross, so a single-source
cross-compiler is not claimed by this release. A future compiler integration can keep
the same `ShaderSource` ABI while generating backend-specific binaries.

## Migration

Replace:

- `IGraphicsBackend` with `RenderingDevice`.
- `GraphicsWindow` with `DeviceConfig::surface`.
- `GraphicsResult` return checks with `RenderingException` handling.
- `Update`/`Render`/`Present` with `ProcessEvents`/`BeginFrame`/`RenderUi`/`Present`.
- `GetGraphicsBackend()` with `GetRenderingDevice()`.

The old `engine/backends` headers and sources, backend registry, and old graphics
result types have been removed from the SDK and build graph.
