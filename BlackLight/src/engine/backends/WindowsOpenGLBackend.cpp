#include "engine/backends/WindowsOpenGLBackend.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include <gl/GL.h>
#include <memory>
#include <iostream>
#include <chrono>
#include <windowsx.h>

#pragma comment(lib, "opengl32.lib")

namespace BlackLight::Engine::Windows
{
	namespace
	{

		// Window class name for Windows API
		const char *WINDOW_CLASS_NAME = "BlackLight3D";

		// Simple OpenGL system interface
		class SimpleSystemInterface : public Rml::SystemInterface
		{
		public:
			double GetElapsedTime() override
			{
				static auto start = std::chrono::high_resolution_clock::now();
				auto now = std::chrono::high_resolution_clock::now();
				return std::chrono::duration<double>(now - start).count();
			}

			bool LogMessage(Rml::Log::Type type, const Rml::String &message) override
			{
				const char *level = "INFO";
				switch (type)
				{
				case Rml::Log::LT_ERROR:
					level = "ERROR";
					break;
				case Rml::Log::LT_WARNING:
					level = "WARNING";
					break;
				case Rml::Log::LT_ASSERT:
					level = "ASSERT";
					break;
				case Rml::Log::LT_INFO:
					level = "INFO";
					break;
				case Rml::Log::LT_DEBUG:
					level = "DEBUG";
					break;
				}
				std::cout << "[RmlUi " << level << "] " << message << std::endl;
				return true;
			}
		};

		// Minimal render interface used by the backend until a hardware renderer is installed.
		class SimpleRenderInterface : public Rml::RenderInterface
		{
		public:
			Rml::CompiledGeometryHandle CompileGeometry(
				Rml::Span<const Rml::Vertex> /*vertices*/,
				Rml::Span<const int> /*indices*/) override
			{
				return next_geometry_handle_++;
			}

			void RenderGeometry(Rml::CompiledGeometryHandle /*geometry*/,
				Rml::Vector2f /*translation*/, Rml::TextureHandle /*texture*/) override {}

			void ReleaseGeometry(Rml::CompiledGeometryHandle /*geometry*/) override {}

			Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
				const Rml::String& /*source*/) override
			{
				texture_dimensions = Rml::Vector2i(0, 0);
				return 0;
			}

			Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> /*source*/,
				Rml::Vector2i /*source_dimensions*/) override
			{
				return 0;
			}

			void ReleaseTexture(Rml::TextureHandle /*texture*/) override {}
			void EnableScissorRegion(bool /*enable*/) override {}
			void SetScissorRegion(Rml::Rectanglei /*region*/) override {}

		private:
			Rml::CompiledGeometryHandle next_geometry_handle_ = 1;
		};

	} // namespace

		// Window procedure for Windows API
		LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
		{
			if (msg == WM_CREATE)
			{
				CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lparam);
				SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
				return 0;
			}

			WindowsOpenGLBackend *pThis = nullptr;
			if (msg != WM_CREATE)
			{
				pThis = reinterpret_cast<WindowsOpenGLBackend *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			}

			switch (msg)
			{
			case WM_CLOSE:
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetKeyDown(static_cast<std::uint32_t>(wparam), true);
				return 0;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetKeyDown(static_cast<std::uint32_t>(wparam), false);
				return 0;
			case WM_LBUTTONDOWN:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMouseButtonDown(MouseButton::Left, true);
				return 0;
			case WM_LBUTTONUP:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMouseButtonDown(MouseButton::Left, false);
				return 0;
			case WM_RBUTTONDOWN:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMouseButtonDown(MouseButton::Right, true);
				return 0;
			case WM_RBUTTONUP:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMouseButtonDown(MouseButton::Right, false);
				return 0;
			case WM_MBUTTONDOWN:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMouseButtonDown(MouseButton::Middle, true);
				return 0;
			case WM_MBUTTONUP:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMouseButtonDown(MouseButton::Middle, false);
				return 0;
			case WM_MOUSEMOVE:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->SetMousePosition({
						static_cast<float>(GET_X_LPARAM(lparam)),
						static_cast<float>(GET_Y_LPARAM(lparam))});
				return 0;
			case WM_MOUSEWHEEL:
				if (pThis && pThis->input_sink_)
					pThis->input_sink_->AddMouseWheelDelta(
						static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA);
				return 0;
			case WM_SIZE:
				if (pThis) {
					pThis->drawable_width_ = LOWORD(lparam);
					pThis->drawable_height_ = HIWORD(lparam);
					if (pThis->drawable_width_ > 0 && pThis->drawable_height_ > 0 &&
						pThis->opengl_context_) {
						glViewport(0, 0,
							static_cast<GLint>(pThis->drawable_width_),
							static_cast<GLint>(pThis->drawable_height_));
						if (pThis->rmlui_context_)
							pThis->rmlui_context_->SetDimensions(Rml::Vector2i(
								static_cast<int>(pThis->drawable_width_),
								static_cast<int>(pThis->drawable_height_)));
					}
				}
				return 0;
			}

			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

	WindowsOpenGLBackend::WindowsOpenGLBackend() = default;

	WindowsOpenGLBackend::~WindowsOpenGLBackend()
	{
		Shutdown();
	}

	GraphicsResult WindowsOpenGLBackend::Initialize(const GraphicsWindow &window)
	{
		should_close_ = false;
		drawable_width_ = window.width;
		drawable_height_ = window.height;
		// Register window class
		WNDCLASSA wnd_class{};
		wnd_class.lpfnWndProc = WindowProcedure;
		wnd_class.hInstance = GetModuleHandle(nullptr);
		wnd_class.lpszClassName = WINDOW_CLASS_NAME;
		wnd_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wnd_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

		if (!RegisterClassA(&wnd_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			std::cerr << "Failed to register window class" << std::endl;
			return GraphicsResult::WindowCreationFailed;
		}

		// Create window
		window_handle_ = CreateWindowA(
			WINDOW_CLASS_NAME,
			window.title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			static_cast<int>(window.width), static_cast<int>(window.height),
			nullptr, nullptr, GetModuleHandle(nullptr), this);

		if (!window_handle_)
		{
			std::cerr << "Failed to create window" << std::endl;
			return GraphicsResult::WindowCreationFailed;
		}

		// Initialize OpenGL
		GraphicsResult gl_result = InitializeOpenGL(window.width, window.height);
		if (gl_result != GraphicsResult::Success)
		{
			return gl_result;
		}

		// Initialize RmlUi
		rmlui_system_ = std::make_unique<SimpleSystemInterface>();
		Rml::SetSystemInterface(rmlui_system_.get());

		rmlui_render_interface_ = std::make_unique<SimpleRenderInterface>();
		Rml::SetRenderInterface(rmlui_render_interface_.get());
		if (!Rml::Initialise()) {
			std::cerr << "Failed to initialize RmlUi" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}
		rmlui_initialized_ = true;
		rmlui_renderer_ = std::make_unique<Rml::RenderManager>(rmlui_render_interface_.get());

		// Create RmlUi context
		Rml::Context *context = Rml::CreateContext("default", Rml::Vector2i(
																  static_cast<int>(window.width),
																  static_cast<int>(window.height)));
		if (!context)
		{
			std::cerr << "Failed to create RmlUi context" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}
		rmlui_context_.reset(context);

		// Show window
		ShowWindow(window_handle_, SW_SHOW);
		UpdateWindow(window_handle_);

		return GraphicsResult::Success;
	}

	Rml::Context *WindowsOpenGLBackend::GetContext() const noexcept
	{
		return rmlui_context_.get();
	}

	Rml::SystemInterface *WindowsOpenGLBackend::GetSystemInterface() const noexcept
	{
		return rmlui_system_.get();
	}

	Rml::RenderManager *WindowsOpenGLBackend::GetRenderManager() const noexcept
	{
		return rmlui_renderer_.get();
	}

	void WindowsOpenGLBackend::SetInputSink(InputState *input) noexcept
	{
		input_sink_ = input;
	}

	uint32_t WindowsOpenGLBackend::GetDrawableWidth() const noexcept
	{
		return drawable_width_;
	}

	uint32_t WindowsOpenGLBackend::GetDrawableHeight() const noexcept
	{
		return drawable_height_;
	}

	bool WindowsOpenGLBackend::Update()
	{
		MSG msg{};
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				should_close_ = true;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}

		if (rmlui_context_)
		{
			rmlui_context_->Update();
		}

		return !should_close_;
	}

	GraphicsResult WindowsOpenGLBackend::Render()
	{
		if (!opengl_context_)
		{
			return GraphicsResult::NotInitialized;
		}

		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (rmlui_context_)
		{
			rmlui_context_->Render();
		}

		return GraphicsResult::Success;
	}

	GraphicsResult WindowsOpenGLBackend::Present()
	{
		if (!device_context_)
		{
			return GraphicsResult::NotInitialized;
		}

		if (!SwapBuffers(device_context_))
		{
			return GraphicsResult::RenderFailed;
		}

		return GraphicsResult::Success;
	}

	void WindowsOpenGLBackend::Shutdown()
	{
		rmlui_context_.reset();
		rmlui_renderer_.reset();
		if (rmlui_initialized_) {
			Rml::ReleaseRenderManagers();
			Rml::Shutdown();
			rmlui_initialized_ = false;
		}
		rmlui_render_interface_.reset();
		rmlui_system_.reset();

		ShutdownOpenGL();

		if (window_handle_)
		{
			DestroyWindow(window_handle_);
			window_handle_ = nullptr;
		}
	}

	bool WindowsOpenGLBackend::ShouldClose() const noexcept
	{
		return should_close_;
	}

	GraphicsResult WindowsOpenGLBackend::InitializeOpenGL(uint32_t width, uint32_t height)
	{
		device_context_ = GetDC(window_handle_);
		if (!device_context_)
		{
			std::cerr << "Failed to get device context" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}

		// Set pixel format
		PIXELFORMATDESCRIPTOR pfd{};
		pfd.nSize = sizeof(pfd);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = 24;
		pfd.iLayerType = PFD_MAIN_PLANE;

		int pixel_format = ChoosePixelFormat(device_context_, &pfd);
		if (!pixel_format)
		{
			std::cerr << "Failed to choose pixel format" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}

		if (!SetPixelFormat(device_context_, pixel_format, &pfd))
		{
			std::cerr << "Failed to set pixel format" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}

		// Create OpenGL context
		opengl_context_ = wglCreateContext(device_context_);
		if (!opengl_context_)
		{
			std::cerr << "Failed to create OpenGL context" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}

		if (!wglMakeCurrent(device_context_, opengl_context_))
		{
			std::cerr << "Failed to make OpenGL context current" << std::endl;
			return GraphicsResult::ContextCreationFailed;
		}

		// Set viewport
		glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, width, height, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		return GraphicsResult::Success;
	}

	GraphicsResult WindowsOpenGLBackend::ShutdownOpenGL()
	{
		if (opengl_context_)
		{
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(opengl_context_);
			opengl_context_ = nullptr;
		}

		if (device_context_ && window_handle_)
		{
			ReleaseDC(window_handle_, device_context_);
			device_context_ = nullptr;
		}

		return GraphicsResult::Success;
	}

} // namespace BlackLight::Engine::Windows
