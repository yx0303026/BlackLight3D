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
