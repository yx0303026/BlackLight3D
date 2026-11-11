# Demo RmlUi Integration Investigation

Date: 2026-11-11. Scope: the OpenGL Demo and the project's pinned RmlUi 6.3.

## Observed Failure

The original x64 Debug Demo was built and launched from its output directory.
Its log stopped at `Loading .bui file: UI/Main.bui`; the process consumed
approximately 1.88 CPU seconds during a two-second observation.

`BuiLoader::ExtractSection` repeatedly called `ParseTag` on ordinary CSS text.
`ParseTag` returned false without advancing, and the inner loop immediately
retried at the same position. This reproduces a startup hang. No access violation
was established from that run.

Fixing only that loop would expose another initialization failure: the OpenGL
device's `GetUiContext()` returned null and `RenderUi()` was empty.

The existing document builder also passed a complete document containing a head
and stylesheet to `Element::SetInnerRML`, which is intended to replace element
content. Complete documents need the document-loading path.

## Upstream Sources Checked

The following official sources were read at tag `6.3`, rather than assuming that
examples for a different render-interface version would apply:

- [Win32 GL2 backend](https://github.com/mikke89/RmlUi/blob/6.3/Backends/RmlUi_Backend_Win32_GL2.cpp):
  WGL pixel format, stencil buffer, frame setup, input forwarding, and teardown.
- [GL2 renderer](https://github.com/mikke89/RmlUi/blob/6.3/Backends/RmlUi_Renderer_GL2.cpp):
  geometry, texture, clipping, and viewport interfaces.
- [Win32 platform](https://github.com/mikke89/RmlUi/blob/6.3/Backends/RmlUi_Platform_Win32.cpp):
  timing, cursor, clipboard, keyboard, mouse, and IME services.
- [Core implementation](https://github.com/mikke89/RmlUi/blob/6.3/Source/Core/Core.cpp):
  initialization, explicit per-context render interfaces, context removal,
  `ReleaseRenderManagers`, and shutdown.
- [Context implementation](https://github.com/mikke89/RmlUi/blob/6.3/Source/Core/Context.cpp):
  `LoadDocumentFromMemory` and document ownership.

## Changes

1. Ensure the section scanner advances on every iteration and rejects missing
   closing tags. Extract content using the actual closing tag start offset.
2. Build `<rml><head><style>...</style></head><body>...</body></rml>` and pass it
   to `Context::LoadDocumentFromMemory`. Retain the existing BuiTree public API.
3. Compile the upstream GL2 and Win32 adapters privately into both engine
   variants. Require the installed core package to match version 6.3. Support a
   supplied backend-source directory or fetch the pinned sources when enabled.
4. Share the process-wide core among engine UI devices, with a renderer and IME
   handler per context. Pass these directly to `CreateContext`; a global render
   interface is therefore unnecessary.
5. Forward native events, resize the UI context, update/render each frame, and
   preserve application OpenGL state. Request an eight-bit stencil buffer.
6. Close application documents, remove the UI context, release render managers,
   and shut down the core before destroying the renderer or WGL context.
7. Load Demo assets relative to the executable and explicitly load Windows Arial
   regular and bold fonts. Route RmlUi diagnostics to the engine log.
8. Use RmlUi-compatible border declarations, wrap flex-item text in child
   elements, and remove unsupported emoji from this Arial-based sample.

All platform and renderer implementation remains under `BlackLight/src` or in
the private fetched dependency. Public SDK headers were not given backend code.

## Verification

- Built `blacklight_demo`, `blacklight_engine_static`, and `blacklight_engine`
  using Visual Studio 2026 MSVC and CMake/Ninja, x64 Debug.
- Configured with `BLACKLIGHT_BUILD_TESTS=OFF`; no automated tests were added or
  run, following the workspace instructions.
- Launched the executable from the repository root, independently of its asset
  directory. Verified font loading, document creation, and lookup of
  `start_button` and `title_panel` in the runtime log.
- Sent mouse input, resized the window, and requested normal shutdown through
  `WM_CLOSE`. The process exited with code 0.
- Confirmed real pointer hover changes the Start button background from
  `#326b8c` to `#3f829f`, comparing pixels in captures of the demo window.
  Synthetic mouse movement alone was insufficient because native mouse-leave
  tracking cleared the artificial hover.
- Captured the actual visible window using `PrintWindow` and inspected the UI.
  The final capture is `build/x64/debug/demo-window.png`.
- The executable writes diagnostics to `build/x64/debug/blacklight_demo.log`.
  The final normal run contained no warning or error records.

Direct3D 11 UI, other architectures, and release builds were outside this
verification. The existing Direct3D 11 device still returns a null UI context.
