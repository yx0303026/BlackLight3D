# Demo DPI Investigation

Date: 2026-11-11. Scope: native Windows OpenGL UI rendering at display scaling.

## Cause

The inspection found a system DPI of 144, corresponding to 150% scaling, while
the original Demo window had DPI awareness category 0 (unaware) and reported
96 DPI. The executable did not contain a DPI-awareness declaration.

Windows could therefore virtualize the application's coordinates and scale its
rendered output. The OpenGL device also did not set RmlUi's density-independent
pixel ratio or process `WM_DPICHANGED`. The sample stylesheet used fixed `px`
values throughout.

This DPI integration was missing from the earlier startup repair.

## Changes

- Embed `Demo/app.manifest` with `PerMonitorV2,PerMonitor` awareness. The SDK
  does not override the host process's DPI policy.
- Use the created window's DPI when calculating nonclient borders, so configured
  client dimensions match the drawable's pixel dimensions.
- Synchronize the native client size with the RmlUi context and the OpenGL
  viewport. Pass `window DPI / 96` to `SetDensityIndependentPixelRatio`.
- Handle `WM_DPICHANGED` by applying the suggested window rectangle for owned
  windows, then updating the context. Attached windows retain their host's resize
  handling and synchronize the resulting client size in the event loop.
- Change the sample's scalable lengths, including font sizes, to `dp`. Set a
  default `12dp` font size explicitly.
- Preserve the initial 1280 by 720 logical window size by converting it to
  native pixels in Demo configuration. At 144 DPI, this is a 1920 by 1080
  client area. This is the window's render resolution; maximization and resize
  continue to follow the actual client area.

At 150%, a `24dp` title resolves to a 36-pixel font size and is rasterized at that
size. A `60dp` grid item resolves to 90 pixels.

## Verification

- Built the Demo, static engine library, and engine DLL in x64 Debug with
  Visual Studio 2026 MSVC and CMake/Ninja.
- Native window metadata confirmed the embedded `PerMonitorV2` context.
- `GetDpiForWindow` returned 144.
- The native client rectangle measured 1920 by 1080 pixels.
- Runtime diagnostics recorded:

  ```text
  UI surface: 1920x1080 pixels, DPI 144, dp ratio 1.500000
  ```

- Captured only the Demo window with `PrintWindow`, without resampling:
  `build/x64/debug/demo-dpi-native.png` (1942 by 1136, including the frame).
- Resized the outer window to 1200 by 800 pixels and inspected the reflow:
  `build/x64/debug/demo-dpi-resized.png`.
- Confirmed native pointer coordinates reach the Start button at 150% scaling.
  Its background changed from `#326b8c` to `#3f829f` on hover.
- Normal close returned exit code 0. Runtime logs contained no warnings or errors.
- No automated tests were added or run, following workspace instructions.

The machine has one monitor. The implementation includes DPI-change handling,
but movement between displays with different DPI has not been exercised.
Release builds, other architectures, and Direct3D 11 UI remain outside this check.

## References

- [Microsoft: Setting the default DPI awareness for a process](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process)
- [Microsoft: WM_DPICHANGED](https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged)
- [RmlUi 6.3 Win32 GL2 backend](https://github.com/mikke89/RmlUi/blob/6.3/Backends/RmlUi_Backend_Win32_GL2.cpp)
- [RmlUi 6.3 Context implementation](https://github.com/mikke89/RmlUi/blob/6.3/Source/Core/Context.cpp)
