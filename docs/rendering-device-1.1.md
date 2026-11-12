# Rendering Device 1.1

## Scope

This stage replaces the placeholder resource and submission path with real OpenGL
3.3 and Direct3D 11 objects. Public headers keep native graphics API types private.
The ABI now includes `DeviceCapabilities`, render-target `BeginFrame`, texture
sampling fields, vertex semantics, pipeline state, and named uniform/texture commands.

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
can be supplied as optional depth attachments. Color and depth dimensions must match,
and both must belong to the same device. The currently bound render target cannot also
be sampled by a command in that pass. Calling parameterless `BeginFrame()` returns to
the default surface.

## Backend Support

| Capability | OpenGL | Direct3D 11 |
| --- | --- | --- |
| Shader source | GLSL | HLSL via `D3DCompile` |
| Texture/sampler | 2D texture and sampler object | 2D texture, SRV, sampler state |
| Storage buffer | Opaque buffer allocation; GL 3.3 has no shader-storage binding | Raw SRV/UAV views on feature level 11_0+ |
| Compute shaders | Not available in the required GL 3.3 context | Reported for feature level 11_0+ |
| Render targets | One color texture FBO plus optional D24S8 attachment | One color RTV plus optional D24S8 DSV |
| Multisampling | Capability limit reported; pipeline/target creation is single-sample | Capability limit reported; pipeline/target creation is single-sample |
| UI adapter | RmlUi GL2 adapter | Not implemented |

`sample_count` must currently be 1. Full MSAA needs a multisampled texture description
and a resolve operation, neither of which is part of this stage's command API.
OpenGL `BufferUsage::Storage` creates an opaque buffer object, but shader-visible
storage-buffer binding is not advertised or exposed by the GL 3.3 backend.

## Build Verification

Verified with Visual Studio 2026 x64 Debug:

```powershell
cmd /c 'call "D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build\x64\debug --target blacklight_engine_static -j 2'
cmd /c 'call "D:\VisualStudio2026\Common7\Tools\VsDevCmd.bat" -arch=x64 && cmake --build build\x64\debug --target blacklight_engine -j 2'
```

No GPU runtime smoke test was run in this change. `D3D11` links `d3dcompiler`; no
GLEW/GLAD dependency or IDA database files are introduced.
