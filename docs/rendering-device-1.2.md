# Rendering Device 1.2

## Scope

This update aligns the default Win32 surface, input, depth, UI, and loss-recovery
paths across OpenGL and D3D11. The public GPU resource handles stay move-only and
backend-neutral.

## D3D11 Surface

The device subclasses both owned and attached HWNDs. `WM_SIZE` and
`WM_DPICHANGED` mark a deferred resize; after event dispatch, the device reads the
client pixel dimensions, unbinds the current output target, releases its RTV and
D24S8 depth view/texture, calls `ResizeBuffers`, and creates matching replacements.
Minimized windows expose a zero-sized surface and skip frame rendering. The
parameterless `BeginFrame()` binds the default RTV/DSV, sets the full-surface
viewport, and clears color, depth, and stencil.

The shared Win32 input path updates keys, five mouse buttons, client-space mouse
position, and normalized wheel delta. Focus loss and cancellation synthesize
releases for held engine and UI inputs. Window subclassing leaves an attached
host's window procedure and `GWLP_USERDATA` untouched.

## RmlUi DX11

The engine builds the official pinned RmlUi 6.3 DX11 renderer alongside GL2 and
Win32 adapters. The private DX11 UI bridge keeps stable filter and shader handles
while allowing the native renderer to be recreated. It releases UI geometry,
textures, and compiled effects before disposing of the old device, recompiles
effects on recovery, then regenerates font resources.

`RenderUi()` runs after application rendering and composites RmlUi onto the
swap-chain RTV. The integration saves/restores the application output targets,
relevant shader constant buffers, and the second pixel texture slot in addition
to the state restored by RmlUi's renderer. The upstream DX11 renderer reads
uncompressed TGA files for file-backed textures; generated textures and font
atlases are fully supported.

## Device Loss

DXGI device removal, reset, hung, and driver-internal errors map to
`RenderingError::DeviceLost`. `Window::Run()` catches only that category, calls
`OnDeviceLost()`, recreates the device on the same HWND, and calls
`OnDeviceRestored()`. Applications should release device-owned resources in the
first hook and recreate them in the second.

For OpenGL, the device requests `WGL_ARB_create_context_robustness` and
`GL_LOSE_CONTEXT_ON_RESET_ARB` when the driver advertises support. It checks the
reset status and context availability during the event, frame, submit, and
presentation paths. Recovery invalidates old resource destruction callbacks
without calling GL in a lost context, creates a new 3.3 compatibility context,
and regenerates RmlUi font resources. Drivers without the robustness extension
still receive context-availability checks; they cannot provide reset-status
diagnostics.

All pre-loss engine `Buffer`, `Texture`, and `GraphicsPipeline` handles are
invalidated on recovery. Their boolean conversion returns false; the application
must create new resources for the new device identity.

## Build Verification

Verified with Visual Studio 2026 x64 Debug:

```powershell
cmake --build build\x64\debug --target blacklight_engine_static --config Debug -j 4
```
