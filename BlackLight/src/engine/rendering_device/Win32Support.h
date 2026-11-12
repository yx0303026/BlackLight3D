#pragma once

#include "engine/Input.h"
#include "engine/rendering_device/RenderingDevice.h"

#include <RmlUi_Platform_Win32.h>
#include <RmlUi/Core.h>

#include <array>
#include <memory>

namespace BlackLight::Engine::Rendering::Detail
{
	/// Returns a window's native DPI, with a fallback for older Windows versions.
	UINT GetWindowDpi(HWND window) noexcept;
	/// Adjusts a requested client rectangle at the window's native DPI.
	void AdjustClientBounds(RECT &bounds, DWORD style, UINT dpi) noexcept;
	/// Releases held inputs while preserving their release transitions.
	void ReleaseWindowInput(InputState *input) noexcept;
	/// Forwards the same keyboard, mouse, focus, text, and IME messages on both APIs.
	bool ForwardWindowInput(InputState *input, Rml::Context *context,
							TextInputMethodEditor_Win32 *ime, HWND window,
							std::array<bool, 256> *ui_keys,
							UINT message, WPARAM wparam, LPARAM lparam);

	/// Routes official Win32 platform diagnostics through the engine logger.
	class UiSystemInterface final : public SystemInterface_Win32
	{
	public:
		/// Logs RmlUi diagnostics without opening assertion dialogs.
		bool LogMessage(Rml::Log::Type type, const Rml::String &message) override;
	};

	/// Shares the process-wide RmlUi core between OpenGL and D3D11 UI devices.
	class UiRuntime final
	{
	public:
		/// Initializes the core with the shared platform services.
		explicit UiRuntime(const char *backend);
		/// Shuts down the core after the last UI device releases it.
		~UiRuntime();
		/// Acquires the core on the common window and rendering thread.
		static std::shared_ptr<UiRuntime> Acquire(const char *backend);

		UiSystemInterface system;
	};
}
