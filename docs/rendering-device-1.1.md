# Rendering Device 1.1

## Scope

This stage replaces the placeholder resource and submission path with real OpenGL
3.3 and Direct3D 11 objects. Public headers keep native graphics API types private.
The ABI now includes `DeviceCapabilities`, render-target `BeginFrame`, texture
sampling fields, multisampled render targets and resolve, vertex semantics, pipeline
state, and named uniform/texture commands.

`CommandList` stores ordered commands in a tagged variant queue. It holds strong
references to every resource it records, so moving or releasing the caller's handle
does not invalidate a queued command. Submission validates device ownership and draw
ranges. A successful submission clears the queue; a failed submission leaves it
intact.

## Resource Lifetime

Each resource payload records its backend, owning device ID, metadata, and a shared
deletion token. OpenGL deletion callbacks are flushed while the WGL context is current.
D3D11 deletion callbacks retain COM references and reset them before the owning device
and context are released. Shutdown drains all registered callbacks even if user handles
or command lists still exist; those later handle destructions are idempotent.

Resources are not thread-safe for simultaneous command recording and destruction.
Create, submit, frame, and device shutdown operations are expected on the device thread.
Resource handles may outlive their device, but cannot be submitted to another device or
a later reinitialization of the same device object; each initialization gets a new
device identity.

## Commands and Uniforms

Commands record pipeline, vertex/index buffers, viewport, uniforms, textures, and draw
calls in exact call order. Uniform names are copied into the queue. Scalar/vector and
matrix values must be finite. Index offsets are aligned and draw ranges are checked
against the captured buffer sizes before reaching the graphics API.

`SetUniform` accepts float, `Vector2`, `Vector3`, four floats, and `Matrix4`.
OpenGL resolves active GLSL uniform locations at submission. D3D11 reflects HLSL
constant buffers and updates only matching floating-point scalar/vector/4x4 matrix
variables. `SetTexture` names a shader sampler and a slot; D3D11 requires that slot to
match the reflected HLSL texture register. D3D11 binds the sampler at that same slot,
so HLSL texture/sampler declarations must use matching `tN`/`sN` registers.

The input-layout contract is backend-specific but explicit: OpenGL uses the semantic
name as the GLSL attribute name (or `a{location}` when omitted); D3D11 uses
`semantic_name` and `semantic_index` to match the vertex shader signature.

## Textures and Render Targets

Texture initial data is tightly packed by mip level, from level zero through the
requested final level. When data is provided, `initial_data_size` must cover the full
mip chain. Dimensions, mip count, format byte-size arithmetic, and backend limits are
validated before allocation. `RGBA8Srgb` maps to sRGB resource/view formats.

Color render targets are created with `TextureDesc::render_target`. `D24S8` textures
can be supplied as optional depth attachments. Color and depth dimensions and sample
counts must match, and both must belong to the same device. The currently bound render
target cannot also be sampled by a command in that pass. Calling parameterless
`BeginFrame()` returns to the default surface.

## Multisampling

`TextureDesc::sample_count` selects the render-target sample count. It must be a
non-zero power of two no greater than `DeviceCapabilities::max_samples`; the default of
1 creates a normal texture. Multisampled textures must be render targets with one mip
level and no initial data, because samples are written by rendering rather than
uploaded. `Texture::GetSampleCount()` reports the resolved value.

`RenderingDevice::ResolveTexture(source, destination)` resolves a multisampled color
target into a single-sample render target. Source and destination must share the
format and dimensions, and the destination must have been created with
`render_target = true`. Resolve is color-only: multisampled depth targets are valid
during a pass but cannot be resolved through this API, because D3D11
`ResolveSubresource` does not accept depth-stencil resources. OpenGL performs the
resolve with `glBlitFramebuffer` and disables `GL_FRAMEBUFFER_SRGB` for the blit so
both backends copy raw sample values. The resolve preserves the render targets that
were bound before the call.

A multisampled texture cannot be sampled directly. `GraphicsPipelineDesc::sample_count`
must match the active color/depth target sample count, and submission rejects a
pipeline/target mismatch or a `SetTexture` binding of an unresolved texture.

## Backend Support

| Capability | OpenGL | Direct3D 11 |
| --- | --- | --- |
| Shader source | GLSL | HLSL via `D3DCompile` |
| Texture/sampler | 2D texture and sampler object | 2D texture, SRV, sampler state |
| Storage buffer | Opaque buffer allocation; GL 3.3 has no shader-storage binding | Raw SRV/UAV views on feature level 11_0+ |
| Compute shaders | Not available in the required GL 3.3 context | Reported for feature level 11_0+ |
| Render targets | One color texture FBO plus optional D24S8 attachment | One color RTV plus optional D24S8 DSV |
| Multisampling | Multisampled texture render targets and color resolve | Multisampled texture render targets and color resolve |
| UI adapter | RmlUi GL2 adapter | Not implemented |

OpenGL resolves with a multisample texture attached to a framebuffer and
`glBlitFramebuffer`; D3D11 creates `TEXTURE2DMS` views and resolves with
`ResolveSubresource`. Both backends report the device sample-count ceiling through
`DeviceCapabilities::max_samples`, and both reject sampling an unresolved target.
OpenGL `BufferUsage::Storage` creates an opaque buffer object, but shader-visible
storage-buffer binding is not advertised or exposed by the GL 3.3 backend.

## Build Verification

Verified with Visual Studio 2026 x64 Debug:

```powershell
cmake --preset=x64-debug
cmake --build build\x64\debug -j 4
ctest --test-dir build\x64\debug --output-on-failure
```

An ad-hoc MSAA smoke executable, removed after verification, created a 4x OpenGL
multisampled color/depth target, rendered a solid quad, resolved it, sampled the
resolved texture into the default framebuffer, and read back the expected green pixel.
The same flow on Direct3D 11 created `TEXTURE2DMS` color/depth targets and completed
`ResolveSubresource` without errors. Pipeline/target sample-count mismatch rejection
was exercised on OpenGL. `D3D11` links `d3dcompiler`; no GLEW/GLAD dependency or IDA
database files are introduced.
