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
`GraphicsPipeline`. `CommandList` records ordered pipeline, vertex/index buffer,
viewport, uniform, texture, and draw operations. Successful submission clears the
recorded queue; a failed submission preserves it for inspection or retry. The wrappers
do not expose WGL, D3D11, or DXGI types in the public ABI.

Both backends create buffers, textures, samplers, and graphics pipelines and execute
draw command lists. Multisampled render targets are created with
`TextureDesc::sample_count` and resolved into a single-sample render target with
`RenderingDevice::ResolveTexture`. The OpenGL backend uses an in-tree OpenGL 3.3
entry-point loader; the D3D11 backend compiles HLSL with `D3DCompile`. Resources
carry device identity, and submission rejects resources created by another device. Resources held past device
shutdown have their backend objects released during shutdown while the native context
or D3D device is still valid.

See [Rendering Device 1.1 implementation notes](rendering-device-1.1.md) for the
uniform ABI, mip-data layout, render-target rules, capability reporting, and backend
limitations.

## UI integration

RmlUi is optional. `RenderingDevice::GetUiContext()` returns a context only when a
device has a UI adapter installed. The core device interface itself remains usable
without RmlUi.

Both devices install the official RmlUi 6.3 Win32 platform adapter and their
matching GL2 or DX11 renderer when `DeviceConfig::enable_ui` is true. They forward
native input, synchronize context dimensions and DPI after resize, and update and
render the overlay from `RenderUi()`. The DX11 adapter uses the swap-chain target
and restores application D3D11 bindings after rendering. The upstream DX11 adapter
supports uncompressed TGA files for `LoadTexture`; generated textures, including
font atlases, use the normal RmlUi path.

UI documents must be closed before device shutdown. Contexts, render managers,
and native UI resources are released while their renderer still exists; OpenGL
resources are also released while the WGL context is current. Platform and
renderer implementations remain private to engine sources.

`Window` classifies lost devices separately from ordinary rendering failures. Its
default loop invokes `OnDeviceLost()`, asks the device to recover on the existing
window, then invokes `OnDeviceRestored()`. These hooks let applications drop and
recreate their own GPU resources. Recovery invalidates all old `Buffer`, `Texture`,
and `GraphicsPipeline` handles; those handles report false and cannot be submitted
to the replacement device. Direct `RenderingDevice` users can call `Recover()`
themselves after `DeviceLost`.

The D3D11 backend recreates its device, swap chain, surface views, and DX11 UI
renderer after device removal or reset. The OpenGL backend requests robust WGL
context creation when available and checks for reset status during rendering. It
recreates the context and regenerates UI font and texture resources while retaining
the RmlUi context and documents.

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
