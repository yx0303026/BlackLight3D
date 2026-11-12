#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Win32Support.h"
#include "engine/Logger.h"

#include <windowsx.h>

namespace BlackLight::Engine::Rendering::Detail
{
	/// Queries per-monitor DPI without requiring a newer Windows loader API.
	UINT GetWindowDpi(HWND window) noexcept
	{
		using GetDpiProc = UINT(WINAPI *)(HWND);
		static const auto get_dpi = reinterpret_cast<GetDpiProc>(
			GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
		const UINT dpi = get_dpi && window ? get_dpi(window) : 0;
		return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
	}

	/// Computes nonclient borders in the same pixel space as the requested surface.
	void AdjustClientBounds(RECT &bounds, DWORD style, UINT dpi) noexcept
	{
		using AdjustBoundsProc = BOOL(WINAPI *)(LPRECT, DWORD, BOOL, DWORD, UINT);
		static const auto adjust_bounds = reinterpret_cast<AdjustBoundsProc>(
			GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi"));
		if (adjust_bounds)
			adjust_bounds(&bounds, style, FALSE, 0, dpi);
		else
			AdjustWindowRectEx(&bounds, style, FALSE, 0);
	}

	/// Clears held keys, all five mouse buttons, and the current wheel accumulation.
	void ReleaseWindowInput(InputState *input) noexcept
	{
		if (!input)
			return;
		for (std::uint32_t key = 0; key < 512; ++key)
			input->SetKeyDown(key, false);
		for (int button = 0; button < static_cast<int>(MouseButton::Count); ++button)
			input->SetMouseButtonDown(static_cast<MouseButton>(button), false);
		input->AddMouseWheelDelta(-input->GetMouseWheelDelta());
	}

	/// Updates the engine snapshot before allowing the UI to consume a native message.
	bool ForwardWindowInput(InputState *input, Rml::Context *context,
							TextInputMethodEditor_Win32 *ime, HWND window,
							std::array<bool, 256> *ui_keys,
							UINT message, WPARAM wparam, LPARAM lparam)
	{
		if (input)
		{
			switch (message)
			{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				input->SetKeyDown(static_cast<std::uint32_t>(wparam), true);
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				input->SetKeyDown(static_cast<std::uint32_t>(wparam), false);
				break;
			case WM_MOUSEMOVE:
				input->SetMousePosition({static_cast<float>(GET_X_LPARAM(lparam)),
										 static_cast<float>(GET_Y_LPARAM(lparam))});
				break;
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
				input->SetMouseButtonDown(MouseButton::Left, message == WM_LBUTTONDOWN);
				break;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
				input->SetMouseButtonDown(MouseButton::Right, message == WM_RBUTTONDOWN);
				break;
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
				input->SetMouseButtonDown(MouseButton::Middle, message == WM_MBUTTONDOWN);
				break;
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
				input->SetMouseButtonDown(
					GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2,
					message == WM_XBUTTONDOWN);
				break;
			case WM_MOUSEWHEEL:
				input->AddMouseWheelDelta(
					static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA);
				break;
			case WM_KILLFOCUS:
			case WM_CANCELMODE:
				ReleaseWindowInput(input);
				break;
			default:
				break;
			}
		}
		if (message == WM_KILLFOCUS || message == WM_CANCELMODE)
		{
			if (GetCapture() == window)
				ReleaseCapture();
			if (context)
			{
				context->ProcessMouseLeave();
				for (int button = 0; button < 3; ++button)
					context->ProcessMouseButtonUp(button, 0);
				if (ui_keys)
				{
					for (std::size_t key = 0; key < ui_keys->size(); ++key)
					{
						if ((*ui_keys)[key])
						{
							context->ProcessKeyUp(RmlWin32::ConvertKey(static_cast<int>(key)), 0);
							(*ui_keys)[key] = false;
						}
					}
				}
			}
			if (ime && ime->IsComposing())
				ime->CancelComposition();
		}
		if (context && ui_keys && wparam < ui_keys->size())
		{
			const auto key = static_cast<std::size_t>(wparam);
			if (message == WM_KEYDOWN)
				(*ui_keys)[key] = RmlWin32::ConvertKey(static_cast<int>(key)) != Rml::Input::KI_UNKNOWN;
			else if (message == WM_KEYUP)
				(*ui_keys)[key] = false;
		}
		if (context && ime)
			return RmlWin32::WindowProcedure(context, *ime, window, message, wparam, lparam);
		return true;
	}

	/// Converts UI log levels to the engine's configured sinks.
	bool UiSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String &message)
	{
		LogLevel level = LogLevel::Info;
		if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
			level = LogLevel::Error;
		else if (type == Rml::Log::LT_WARNING)
			level = LogLevel::Warning;
		else if (type == Rml::Log::LT_DEBUG)
			level = LogLevel::Debug;
		BL_LOG(level, "RmlUi", message);
		return true;
	}

	/// Installs platform services before initializing RmlUi.
	UiRuntime::UiRuntime(const char *backend)
	{
		Rml::SetSystemInterface(&system);
		if (!Rml::Initialise())
		{
			Rml::SetSystemInterface(nullptr);
			throw RenderingException(RenderingError::DeviceCreationFailed, backend,
									 "RmlUi initialization failed");
		}
	}

	/// Releases the core while its platform services still exist.
	UiRuntime::~UiRuntime() { Rml::Shutdown(); }

	/// Keeps one RmlUi core alive across both rendering backends.
	std::shared_ptr<UiRuntime> UiRuntime::Acquire(const char *backend)
	{
		static std::weak_ptr<UiRuntime> active;
		auto runtime = active.lock();
		if (!runtime)
		{
			runtime = std::make_shared<UiRuntime>(backend);
			active = runtime;
		}
		return runtime;
	}
}
