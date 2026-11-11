#include "../DeviceImplementation.h"

#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <sstream>
#include <stdexcept>
#include <string>

namespace BlackLight::Engine::Rendering::Detail
{
	namespace
	{
		constexpr wchar_t kWindowClass[] = L"BlackLightRenderingDeviceOpenGL";

		/// Routes Win32 close and input messages into the owning OpenGL device.
		LRESULT CALLBACK DeviceWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
		{
			if (message == WM_NCCREATE)
			{
				const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
				SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
			}
			if (message == WM_CLOSE || message == WM_DESTROY)
			{
				PostQuitMessage(0);
				return 0;
			}
			return DefWindowProcW(window, message, wparam, lparam);
		}
	}

	/// Owns Win32 and WGL handles for one OpenGL rendering device.
	class OpenGLDevice::Impl
	{
	public:
		DeviceConfig config;
		InputState *input = nullptr;
		HWND window = nullptr;
		HDC dc = nullptr;
		HGLRC context = nullptr;
		bool owns_window = false;
		bool close = false;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
	};

	/// Creates an uninitialized OpenGL device.
	OpenGLDevice::OpenGLDevice() : impl_(std::make_unique<Impl>()) {}
	/// Releases WGL and window resources.
	OpenGLDevice::~OpenGLDevice() { Shutdown(); }

	/// Initializes a Win32 window or attaches to an existing HWND.
	void OpenGLDevice::Initialize(const DeviceConfig &config, InputState *input)
	{
		impl_->config = config;
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
			window_class.lpfnWndProc = DeviceWindowProcedure;
			window_class.hInstance = GetModuleHandleW(nullptr);
			window_class.lpszClassName = kWindowClass;
			window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
			RegisterClassW(&window_class);
			const std::wstring title(config.surface.title.begin(), config.surface.title.end());
			impl_->window = CreateWindowExW(
				0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(config.surface.width),
				static_cast<int>(config.surface.height), nullptr, nullptr, GetModuleHandleW(nullptr), this);
			if (!impl_->window)
				throw RenderingException(RenderingError::WindowCreationFailed, "OpenGL", "CreateWindowExW failed");
			impl_->owns_window = true;
		}

		impl_->dc = GetDC(impl_->window);
		if (!impl_->dc)
			throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "GetDC failed");
		PIXELFORMATDESCRIPTOR descriptor{};
		descriptor.nSize = sizeof(descriptor);
		descriptor.nVersion = 1;
		descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		descriptor.iPixelType = PFD_TYPE_RGBA;
		descriptor.cColorBits = 32;
		descriptor.cDepthBits = 24;
		descriptor.iLayerType = PFD_MAIN_PLANE;
		const int pixel_format = ChoosePixelFormat(impl_->dc, &descriptor);
		if (!pixel_format || !SetPixelFormat(impl_->dc, pixel_format, &descriptor))
			throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "pixel format setup failed");
		impl_->context = wglCreateContext(impl_->dc);
		if (!impl_->context || !wglMakeCurrent(impl_->dc, impl_->context))
			throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "WGL context creation failed");
		ShowWindow(impl_->window, SW_SHOW);
	}

	/// Pumps Win32 events and updates drawable dimensions.
	bool OpenGLDevice::ProcessEvents()
	{
		MSG message{};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				impl_->close = true;
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		RECT rectangle{};
		if (impl_->window && GetClientRect(impl_->window, &rectangle))
		{
			impl_->width = static_cast<std::uint32_t>(rectangle.right - rectangle.left);
			impl_->height = static_cast<std::uint32_t>(rectangle.bottom - rectangle.top);
		}
		return !impl_->close;
	}

	/// Clears the default color and depth buffers.
	void OpenGLDevice::BeginFrame()
	{
		if (!impl_->context || !wglMakeCurrent(impl_->dc, impl_->context))
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "context is not current");
		glViewport(0, 0, static_cast<GLsizei>(impl_->width), static_cast<GLsizei>(impl_->height));
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	/// Does nothing when the optional RmlUi adapter is unavailable.
	void OpenGLDevice::RenderUi() {}
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
	/// Returns no context until a RmlUi renderer is installed.
	Rml::Context *OpenGLDevice::GetUiContext() const noexcept { return nullptr; }
	/// Returns no system interface until a RmlUi renderer is installed.
	Rml::SystemInterface *OpenGLDevice::GetUiSystemInterface() const noexcept { return nullptr; }

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
