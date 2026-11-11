#include "../DeviceImplementation.h"
#include "engine/Logger.h"

#include <RmlUi_Platform_Win32.h>
#include <RmlUi_Renderer_GL2.h>
#include <RmlUi/Core.h>
#include <commctrl.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <stdexcept>
#include <string>

namespace BlackLight::Engine::Rendering::Detail
{
	namespace
	{
		constexpr wchar_t kWindowClass[] = L"BlackLightRenderingDeviceOpenGL";

		/// Queries the window's DPI without requiring a newer Windows loader API.
		UINT GetWindowDpi(HWND window)
		{
			using GetDpiProc = UINT(WINAPI *)(HWND);
			static const auto get_dpi = reinterpret_cast<GetDpiProc>(
				GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
			const UINT dpi = get_dpi ? get_dpi(window) : 0;
			return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
		}

		/// Computes nonclient borders at the DPI of the newly created window.
		void AdjustClientBounds(RECT &bounds, DWORD style, UINT dpi)
		{
			using AdjustBoundsProc = BOOL(WINAPI *)(LPRECT, DWORD, BOOL, DWORD, UINT);
			static const auto adjust_bounds = reinterpret_cast<AdjustBoundsProc>(
				GetProcAddress(GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi"));
			if (adjust_bounds)
				adjust_bounds(&bounds, style, FALSE, 0, dpi);
			else
				AdjustWindowRectEx(&bounds, style, FALSE, 0);
		}

		/// Uses the official Win32 services and routes RmlUi diagnostics to the engine.
		class UiSystemInterface final : public SystemInterface_Win32
		{
		public:
			/// Logs assertions without opening the default RmlUi modal dialog.
			bool LogMessage(Rml::Log::Type type, const Rml::String &message) override
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
		};

		/// Keeps the process-wide core alive until the last UI device has shut down.
		class UiRuntime final
		{
		public:
			/// Registers platform services before initializing the core.
			UiRuntime()
			{
				Rml::SetSystemInterface(&system);
				if (!Rml::Initialise())
				{
					Rml::SetSystemInterface(nullptr);
					throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "RmlUi initialization failed");
				}
			}
			/// Shuts down the core before the platform services are destroyed.
			~UiRuntime() { Rml::Shutdown(); }

			/// Shares a single core between UI devices on the window thread.
			static std::shared_ptr<UiRuntime> Acquire()
			{
				static std::weak_ptr<UiRuntime> active;
				auto runtime = active.lock();
				if (!runtime)
				{
					runtime = std::make_shared<UiRuntime>();
					active = runtime;
				}
				return runtime;
			}

			UiSystemInterface system;
		};
	}

	/// Owns Win32 and WGL handles for one OpenGL rendering device.
	class OpenGLDevice::Impl
	{
	public:
		/// Forwards native input to the engine snapshot and the official RmlUi handler.
		static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam,
			LPARAM lparam, UINT_PTR, DWORD_PTR reference)
		{
			auto &self = *reinterpret_cast<Impl *>(reference);
			if (message == WM_CLOSE)
			{
				self.close = true;
				return self.owns_window ? 0 : DefSubclassProc(window, message, wparam, lparam);
			}
			if (message == WM_NCDESTROY)
				self.close = true;
			if (message == WM_DPICHANGED)
			{
				if (self.owns_window)
				{
					const auto &bounds = *reinterpret_cast<const RECT *>(lparam);
					SetWindowPos(window, nullptr, bounds.left, bounds.top,
						bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
					self.SynchronizeSurface();
					return 0;
				}
				return DefSubclassProc(window, message, wparam, lparam);
			}

			if (self.input)
			{
				switch (message)
				{
				case WM_KEYDOWN: case WM_SYSKEYDOWN:
					self.input->SetKeyDown(static_cast<std::uint32_t>(wparam), true);
					break;
				case WM_KEYUP: case WM_SYSKEYUP:
					self.input->SetKeyDown(static_cast<std::uint32_t>(wparam), false);
					break;
				case WM_MOUSEMOVE:
					self.input->SetMousePosition({static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))});
					break;
				case WM_LBUTTONDOWN: case WM_LBUTTONUP:
					self.input->SetMouseButtonDown(MouseButton::Left, message == WM_LBUTTONDOWN);
					break;
				case WM_RBUTTONDOWN: case WM_RBUTTONUP:
					self.input->SetMouseButtonDown(MouseButton::Right, message == WM_RBUTTONDOWN);
					break;
				case WM_MBUTTONDOWN: case WM_MBUTTONUP:
					self.input->SetMouseButtonDown(MouseButton::Middle, message == WM_MBUTTONDOWN);
					break;
				case WM_MOUSEWHEEL:
					self.input->AddMouseWheelDelta(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA);
					break;
				case WM_KILLFOCUS:
					for (std::uint32_t key = 0; key < 512; ++key)
						self.input->SetKeyDown(key, false);
					for (int button = 0; button < static_cast<int>(MouseButton::Count); ++button)
						self.input->SetMouseButtonDown(static_cast<MouseButton>(button), false);
					break;
				default: break;
				}
			}
			if (self.ui_context &&
				!RmlWin32::WindowProcedure(self.ui_context, *self.ui_input, window, message, wparam, lparam))
				return 0;
			return DefSubclassProc(window, message, wparam, lparam);
		}

		/// Keeps layout, font rasterization, and rendering in the window's pixel space.
		void SynchronizeSurface()
		{
			RECT rectangle{};
			if (window && GetClientRect(window, &rectangle))
			{
				width = static_cast<std::uint32_t>(rectangle.right - rectangle.left);
				height = static_cast<std::uint32_t>(rectangle.bottom - rectangle.top);
			}
			const UINT current_dpi = GetWindowDpi(window);
			if (ui_context)
			{
				ui_context->SetDimensions({static_cast<int>(width), static_cast<int>(height)});
				ui_context->SetDensityIndependentPixelRatio(static_cast<float>(current_dpi) / USER_DEFAULT_SCREEN_DPI);
				if (dpi != current_dpi)
					BL_LOG(LogLevel::Info, "OpenGL", "UI surface: " + std::to_string(width) + "x" +
						std::to_string(height) + " pixels, DPI " + std::to_string(current_dpi) +
						", dp ratio " + std::to_string(ui_context->GetDensityIndependentPixelRatio()));
			}
			dpi = current_dpi;
		}

		DeviceConfig config;
		InputState *input = nullptr;
		HWND window = nullptr;
		HDC dc = nullptr;
		HGLRC context = nullptr;
		bool owns_window = false;
		bool subclassed = false;
		bool close = false;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		UINT dpi = 0;
		std::shared_ptr<UiRuntime> ui_runtime;
		std::unique_ptr<RenderInterface_GL2> ui_renderer;
		std::unique_ptr<TextInputMethodEditor_Win32> ui_input;
		Rml::Context *ui_context = nullptr;
	};

	/// Creates an uninitialized OpenGL device.
	OpenGLDevice::OpenGLDevice() : impl_(std::make_unique<Impl>()) {}
	/// Releases WGL and window resources.
	OpenGLDevice::~OpenGLDevice() { Shutdown(); }

	/// Initializes a Win32 window or attaches to an existing HWND.
	void OpenGLDevice::Initialize(const DeviceConfig &config, InputState *input)
	{
		if (impl_->window)
			throw RenderingException(RenderingError::InvalidConfiguration, "OpenGL", "device is already initialized");
		impl_->config = config;
		impl_->close = false;
		impl_->input = input;
		impl_->width = config.surface.width;
		impl_->height = config.surface.height;
		if (config.surface.mode == SurfaceMode::AttachWindow)
		{
			impl_->window = reinterpret_cast<HWND>(config.surface.native_window);
			if (!IsWindow(impl_->window))
				throw RenderingException(RenderingError::InvalidConfiguration, "OpenGL", "external HWND is invalid");
		}
		else
		{
			WNDCLASSW window_class{};
			window_class.style = CS_OWNDC;
			window_class.lpfnWndProc = DefWindowProcW;
			window_class.hInstance = GetModuleHandleW(nullptr);
			window_class.lpszClassName = kWindowClass;
			window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
			RegisterClassW(&window_class);
			const std::wstring title = RmlWin32::ConvertToUTF16(config.surface.title);
			const DWORD style = config.surface.resizable ? WS_OVERLAPPEDWINDOW :
				(WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX);
			impl_->window = CreateWindowExW(
				0, kWindowClass, title.c_str(), style,
				CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
			if (!impl_->window)
				throw RenderingException(RenderingError::WindowCreationFailed, "OpenGL", "CreateWindowExW failed");
			impl_->owns_window = true;
			RECT bounds{0, 0, static_cast<LONG>(config.surface.width), static_cast<LONG>(config.surface.height)};
			AdjustClientBounds(bounds, style, GetWindowDpi(impl_->window));
			SetWindowPos(impl_->window, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		}

		if (!SetWindowSubclass(impl_->window, Impl::WindowProcedure, reinterpret_cast<UINT_PTR>(impl_.get()),
			reinterpret_cast<DWORD_PTR>(impl_.get())))
			throw RenderingException(RenderingError::WindowCreationFailed, "OpenGL", "window input handler setup failed");
		impl_->subclassed = true;

		impl_->dc = GetDC(impl_->window);
		if (!impl_->dc)
			throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "GetDC failed");
		PIXELFORMATDESCRIPTOR descriptor{};
		descriptor.nSize = sizeof(descriptor);
		descriptor.nVersion = 1;
		descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		descriptor.iPixelType = PFD_TYPE_RGBA;
		descriptor.cColorBits = 32;
		descriptor.cAlphaBits = 8;
		descriptor.cDepthBits = 24;
		descriptor.cStencilBits = 8;
		descriptor.iLayerType = PFD_MAIN_PLANE;
		const int pixel_format = ChoosePixelFormat(impl_->dc, &descriptor);
		if (!pixel_format || !SetPixelFormat(impl_->dc, pixel_format, &descriptor))
			throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "pixel format setup failed");
		impl_->context = wglCreateContext(impl_->dc);
		if (!impl_->context || !wglMakeCurrent(impl_->dc, impl_->context))
			throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "WGL context creation failed");
		using SwapIntervalProc = BOOL(WINAPI *)(int);
		const PROC swap_interval = wglGetProcAddress("wglSwapIntervalEXT");
		const auto swap_interval_address = reinterpret_cast<std::uintptr_t>(swap_interval);
		if (swap_interval_address > 3 && swap_interval_address != static_cast<std::uintptr_t>(-1))
			reinterpret_cast<SwapIntervalProc>(swap_interval)(config.surface.vertical_sync ? 1 : 0);
		RECT client{};
		GetClientRect(impl_->window, &client);
		impl_->width = static_cast<std::uint32_t>(client.right);
		impl_->height = static_cast<std::uint32_t>(client.bottom);
		if (config.enable_ui)
		{
			impl_->ui_renderer = std::make_unique<RenderInterface_GL2>();
			impl_->ui_input = std::make_unique<TextInputMethodEditor_Win32>();
			impl_->ui_runtime = UiRuntime::Acquire();
			impl_->ui_runtime->system.SetWindow(impl_->window);
			const std::string name = "blacklight-" + std::to_string(reinterpret_cast<std::uintptr_t>(impl_.get()));
			impl_->ui_context = Rml::CreateContext(name,
				{static_cast<int>(impl_->width), static_cast<int>(impl_->height)}, impl_->ui_renderer.get(), impl_->ui_input.get());
			if (!impl_->ui_context)
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "RmlUi context creation failed");
		}
		impl_->SynchronizeSurface();
		ShowWindow(impl_->window, SW_SHOW);
	}

	/// Pumps Win32 events and updates drawable dimensions.
	bool OpenGLDevice::ProcessEvents()
	{
		if (impl_->ui_runtime)
			impl_->ui_runtime->system.SetWindow(impl_->window);
		MSG message{};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				impl_->close = true;
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		impl_->SynchronizeSurface();
		return !impl_->close;
	}

	/// Clears the default color and depth buffers.
	void OpenGLDevice::BeginFrame()
	{
		if (!impl_->context || !wglMakeCurrent(impl_->dc, impl_->context))
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "context is not current");
		glViewport(0, 0, static_cast<GLsizei>(impl_->width), static_cast<GLsizei>(impl_->height));
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClearStencil(0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	/// Updates and renders the UI while preserving application OpenGL state.
	void OpenGLDevice::RenderUi()
	{
		if (!impl_->ui_context || impl_->close || !impl_->width || !impl_->height)
			return;
		impl_->ui_context->Update();
		GLint matrix_mode = GL_MODELVIEW;
		glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
		for (const GLenum mode : {GL_PROJECTION, GL_TEXTURE, GL_MODELVIEW})
		{
			glMatrixMode(mode);
			glPushMatrix();
		}
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_LIGHTING);
		glDisable(GL_FOG);
		impl_->ui_renderer->SetViewport(static_cast<int>(impl_->width), static_cast<int>(impl_->height));
		impl_->ui_renderer->BeginFrame();
		impl_->ui_context->Render();
		impl_->ui_renderer->EndFrame();
		for (const GLenum mode : {GL_MODELVIEW, GL_TEXTURE, GL_PROJECTION})
		{
			glMatrixMode(mode);
			glPopMatrix();
		}
		glPopClientAttrib();
		glPopAttrib();
		glMatrixMode(matrix_mode);
	}
	/// Presents the WGL double-buffered drawable.
	void OpenGLDevice::Present()
	{
		if (!impl_->dc || !SwapBuffers(impl_->dc))
			throw RenderingException(RenderingError::PresentationFailed, "OpenGL", "SwapBuffers failed");
	}
	/// Releases OpenGL, DC, and owned window resources.
	void OpenGLDevice::Shutdown() noexcept
	{
		if (!impl_)
			return;
		if (impl_->context)
			wglMakeCurrent(impl_->dc, impl_->context);
		if (impl_->ui_context)
		{
			Rml::RemoveContext(impl_->ui_context->GetName());
			impl_->ui_context = nullptr;
		}
		if (impl_->ui_runtime)
		{
			Rml::ReleaseRenderManagers();
			impl_->ui_runtime->system.SetWindow(nullptr);
			impl_->ui_runtime.reset();
		}
		impl_->ui_input.reset();
		impl_->ui_renderer.reset();
		if (impl_->subclassed && IsWindow(impl_->window))
			RemoveWindowSubclass(impl_->window, Impl::WindowProcedure, reinterpret_cast<UINT_PTR>(impl_.get()));
		impl_->subclassed = false;
		if (impl_->context)
		{
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(impl_->context);
			impl_->context = nullptr;
		}
		if (impl_->dc && impl_->window)
		{
			ReleaseDC(impl_->window, impl_->dc);
			impl_->dc = nullptr;
		}
		if (impl_->owns_window && impl_->window)
			DestroyWindow(impl_->window);
		impl_->window = nullptr;
		impl_->owns_window = false;
		impl_->dpi = 0;
	}
	/// Returns the current surface state.
	SurfaceState OpenGLDevice::GetSurfaceState() const noexcept
	{
		return {impl_->width, impl_->height, impl_->close};
	}
	/// Sets the host input sink.
	void OpenGLDevice::SetInputSink(InputState *input) noexcept { impl_->input = input; }
	/// Returns the OpenGL backend identity.
	RenderingBackend OpenGLDevice::GetBackend() const noexcept { return RenderingBackend::OpenGL; }
	/// Returns the window handle.
	NativeWindowHandle OpenGLDevice::GetNativeWindow() const noexcept
	{
		return reinterpret_cast<NativeWindowHandle>(impl_->window);
	}
	/// Returns the UI context when UI was enabled during initialization.
	Rml::Context *OpenGLDevice::GetUiContext() const noexcept { return impl_->ui_context; }
	/// Returns the shared Win32 platform services while UI is initialized.
	Rml::SystemInterface *OpenGLDevice::GetUiSystemInterface() const noexcept
	{
		return impl_->ui_runtime ? &impl_->ui_runtime->system : nullptr;
	}

	/// Creates a normalized command list.
	CommandList OpenGLDevice::CreateCommandList() { return {}; }
	/// Rejects draw commands until the normalized GL pipeline executor is installed.
	void OpenGLDevice::Submit(CommandList &commands)
	{
		if (commands.GetDrawCount() != 0)
			throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL", "normalized draw submission is not implemented");
	}
	/// Rejects resource creation until the normalized GL resource allocator is installed.
	Buffer OpenGLDevice::CreateBuffer(const BufferDesc &)
	{
		throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL", "buffer creation is not implemented");
	}
	/// Rejects texture creation until the normalized GL resource allocator is installed.
	Texture OpenGLDevice::CreateTexture(const TextureDesc &)
	{
		throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL", "texture creation is not implemented");
	}
	/// Rejects pipeline creation until shader compilation is configured.
	GraphicsPipeline OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &)
	{
		throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL", "pipeline creation is not implemented");
	}
}
