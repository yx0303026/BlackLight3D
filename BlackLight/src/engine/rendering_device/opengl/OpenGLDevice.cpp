#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../DeviceImplementation.h"
#include "../CommandListInternal.h"
#include "../RenderingResourcesInternal.h"
#include "../Win32Support.h"
#include "engine/Logger.h"
#include "engine/rendering_device/Shader.h"
#include "OpenGLFunctions.h"

#include <RmlUi_Platform_Win32.h>
#include <RmlUi_Renderer_GL2.h>
#include <RmlUi/Core.h>
#include <commctrl.h>
#include <windowsx.h>
#include <gl/GL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <type_traits>
#include <stdexcept>
#include <string>
#include <variant>

#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif

namespace BlackLight::Engine::Rendering::Detail
{
	namespace
	{
		constexpr wchar_t kWindowClass[] = L"BlackLightRenderingDeviceOpenGL";
		constexpr int kWglContextMajorVersion = 0x2091;
		constexpr int kWglContextMinorVersion = 0x2092;
		constexpr int kWglContextProfileMask = 0x9126;
		constexpr int kWglContextCompatibilityProfile = 0x00000002;
		constexpr GLenum GL_RGBA8_VALUE = 0x8058;
		constexpr GLenum GL_R32F_VALUE = 0x822E;
		constexpr GLenum GL_RED_VALUE_LOCAL = 0x1903;
		constexpr GLenum GL_DEPTH_COMPONENT_VALUE = 0x1902;
		constexpr GLenum GL_UNSIGNED_BYTE_VALUE = 0x1401;
		constexpr GLenum GL_LINEAR_VALUE = 0x2601;
		constexpr GLenum GL_NEAREST_VALUE = 0x2600;
		constexpr GLenum GL_TEXTURE_MIN_FILTER_LOCAL = 0x2801;
		constexpr GLenum GL_TEXTURE_MAG_FILTER_LOCAL = 0x2800;
		constexpr GLenum GL_TEXTURE_WRAP_S_LOCAL = 0x2802;
		constexpr GLenum GL_TEXTURE_WRAP_T_LOCAL = 0x2803;
		constexpr GLenum GL_LINEAR_MIPMAP_LINEAR_VALUE = 0x2703;
		constexpr GLenum GL_NEAREST_MIPMAP_NEAREST_VALUE = 0x2700;
		constexpr GLenum GL_MAX_TEXTURE_SIZE_VALUE_LOCAL = 0x0D33;
		constexpr GLenum GL_MAX_COLOR_ATTACHMENTS_VALUE_LOCAL = 0x8CDF;
		constexpr GLenum GL_MAX_SAMPLES_VALUE_LOCAL = 0x8D57;
		constexpr GLenum GL_FRAMEBUFFER_SRGB_VALUE_LOCAL = 0x8DB9;
		constexpr GLenum GL_ACTIVE_UNIFORMS_VALUE = 0x8B86;
		constexpr GLenum GL_ACTIVE_UNIFORM_MAX_LENGTH_VALUE = 0x8B87;
		constexpr GLenum GL_FLOAT_VALUE = 0x1406;
		constexpr GLenum GL_FLOAT_VEC2_VALUE = 0x8B50;
		constexpr GLenum GL_FLOAT_VEC3_VALUE = 0x8B51;
		constexpr GLenum GL_FLOAT_VEC4_VALUE = 0x8B52;
		constexpr GLenum GL_FLOAT_MAT4_VALUE = 0x8B5C;
		constexpr GLenum GL_SAMPLER_2D_VALUE = 0x8B5E;
		constexpr GLenum GL_CURRENT_PROGRAM_VALUE_LOCAL = 0x8B8D;
		constexpr GLenum GL_TIME_ELAPSED_QUERY_VALUE = 0x88BF;
		constexpr GLenum GL_QUERY_RESULT_VALUE_LOCAL = 0x8866;
		constexpr GLenum GL_QUERY_RESULT_AVAILABLE_VALUE_LOCAL = 0x8867;
		constexpr GLenum GL_VERTEX_ARRAY_BINDING_VALUE_LOCAL = 0x85B5;
		constexpr GLenum GL_ARRAY_BUFFER_BINDING_VALUE_LOCAL = 0x8894;
		constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_BINDING_VALUE_LOCAL = 0x8895;
		constexpr GLenum GL_FRAMEBUFFER_BINDING_VALUE_LOCAL = 0x8CA6;
		constexpr GLenum GL_NO_ERROR_VALUE_LOCAL = 0;
		std::atomic<std::uint64_t> g_next_device_id{1};

		/// Resolves an optional WGL or OpenGL extension without accepting sentinel values.
		template <typename Function>
		Function FindOpenGLFunction(const char *name) noexcept
		{
			PROC address = wglGetProcAddress(name);
			const auto value = reinterpret_cast<std::uintptr_t>(address);
			if (value <= 3 || value == static_cast<std::uintptr_t>(-1))
				address = GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name);
			return reinterpret_cast<Function>(address);
		}

		/// Maps the requested texture wrap mode to OpenGL.
		GLint MapAddressMode(TextureAddressMode mode)
		{
			switch (mode)
			{
			case TextureAddressMode::Clamp:
				return static_cast<GLint>(GL_CLAMP_TO_EDGE_VALUE);
			case TextureAddressMode::Repeat:
				return static_cast<GLint>(GL_REPEAT_VALUE);
			case TextureAddressMode::Mirror:
				return static_cast<GLint>(GL_MIRRORED_REPEAT_VALUE);
			}
			return static_cast<GLint>(GL_CLAMP_TO_EDGE_VALUE);
		}

		/// Maps the requested texture filter to OpenGL.
		GLint MapFilter(TextureFilter filter)
		{
			return static_cast<GLint>(filter == TextureFilter::Nearest ? GL_NEAREST_VALUE : GL_LINEAR_VALUE);
		}

		/// Returns the byte width of a normalized resource format.
		std::size_t FormatSize(ResourceFormat format)
		{
			switch (format)
			{
			case ResourceFormat::R8Unorm:
				return 1;
			case ResourceFormat::R16Uint:
				return 2;
			case ResourceFormat::RGBA8Unorm:
			case ResourceFormat::RGBA8Srgb:
			case ResourceFormat::R32Float:
			case ResourceFormat::R32Uint:
			case ResourceFormat::D24S8:
				return 4;
			case ResourceFormat::RG32Float:
				return 8;
			case ResourceFormat::RGB32Float:
				return 12;
			case ResourceFormat::RGBA32Float:
				return 16;
			}
			return 0;
		}

		/// Maps normalized texture formats to OpenGL internal and transfer formats.
		bool MapTextureFormat(ResourceFormat format, GLint &internal, GLenum &external, GLenum &type)
		{
			type = GL_UNSIGNED_BYTE;
			switch (format)
			{
			case ResourceFormat::R8Unorm:
				internal = static_cast<GLint>(GL_R8_VALUE);
				external = GL_RED_VALUE;
				return true;
			case ResourceFormat::RGBA8Unorm:
				internal = static_cast<GLint>(GL_RGBA8_VALUE);
				external = GL_RGBA;
				return true;
			case ResourceFormat::RGBA8Srgb:
				internal = static_cast<GLint>(GL_SRGB8_ALPHA8_VALUE);
				external = GL_RGBA;
				return true;
			case ResourceFormat::R32Float:
				internal = static_cast<GLint>(GL_R32F_VALUE);
				external = GL_RED_VALUE_LOCAL;
				type = GL_FLOAT;
				return true;
			case ResourceFormat::RG32Float:
				internal = static_cast<GLint>(GL_RG32F_VALUE);
				external = GL_RG_VALUE;
				type = GL_FLOAT;
				return true;
			case ResourceFormat::RGB32Float:
				internal = static_cast<GLint>(GL_RGB32F_VALUE);
				external = GL_RGB;
				type = GL_FLOAT;
				return true;
			case ResourceFormat::RGBA32Float:
				internal = static_cast<GLint>(GL_RGBA32F_VALUE);
				external = GL_RGBA;
				type = GL_FLOAT;
				return true;
			case ResourceFormat::D24S8:
				internal = static_cast<GLint>(GL_DEPTH24_STENCIL8_VALUE);
				external = GL_DEPTH_STENCIL_VALUE;
				type = GL_UNSIGNED_INT_24_8_VALUE;
				return true;
			case ResourceFormat::R16Uint:
				internal = static_cast<GLint>(GL_R16UI_VALUE);
				external = GL_RED_INTEGER_VALUE;
				type = GL_UNSIGNED_SHORT_VALUE;
				return true;
			case ResourceFormat::R32Uint:
				internal = static_cast<GLint>(GL_R32UI_VALUE);
				external = GL_RED_INTEGER_VALUE;
				type = GL_UNSIGNED_INT_VALUE;
				return true;
			}
			return false;
		}

		/// Maps normalized vertex formats to OpenGL scalar types and component sizes.
		bool MapVertexFormat(ResourceFormat format, GLenum &type, std::uint32_t &component_bytes)
		{
			switch (format)
			{
			case ResourceFormat::R8Unorm:
				type = GL_UNSIGNED_BYTE;
				component_bytes = 1;
				return true;
			case ResourceFormat::R16Uint:
				type = GL_UNSIGNED_SHORT;
				component_bytes = 2;
				return true;
			case ResourceFormat::R32Uint:
				type = GL_UNSIGNED_INT;
				component_bytes = 4;
				return true;
			case ResourceFormat::R32Float:
			case ResourceFormat::RG32Float:
			case ResourceFormat::RGB32Float:
			case ResourceFormat::RGBA32Float:
				type = GL_FLOAT;
				component_bytes = 4;
				return true;
			default:
				return false;
			}
		}

		/// Maps the normalized primitive topology to its OpenGL equivalent.
		GLenum MapTopology(GraphicsPipelineDesc::PrimitiveTopology topology)
		{
			switch (topology)
			{
			case GraphicsPipelineDesc::PrimitiveTopology::TriangleList:
				return GL_TRIANGLES_VALUE;
			case GraphicsPipelineDesc::PrimitiveTopology::LineList:
				return GL_LINES_VALUE;
			case GraphicsPipelineDesc::PrimitiveTopology::PointList:
				return GL_POINTS_VALUE;
			}
			return GL_TRIANGLES_VALUE;
		}

		/// Returns the GL component count implied by a vertex attribute format.
		std::uint32_t FormatComponents(ResourceFormat format)
		{
			switch (format)
			{
			case ResourceFormat::RG32Float:
				return 2;
			case ResourceFormat::RGB32Float:
				return 3;
			case ResourceFormat::RGBA32Float:
				return 4;
			default:
				return 1;
			}
		}

		/// Compiles one GLSL stage and reports the complete driver log on failure.
		GLuint CompileShader(OpenGLFunctions &functions, GLenum type, const char *source, const char *stage)
		{
			const GLuint shader = functions.CreateShader(type);
			if (!shader)
				throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
										 std::string("Failed to create ") + stage + " shader");
			functions.ShaderSource(shader, 1, &source, nullptr);
			functions.CompileShader(shader);
			GLint compiled = GL_FALSE;
			functions.GetShaderiv(shader, GL_COMPILE_STATUS_VALUE, &compiled);
			if (compiled == GL_TRUE)
				return shader;

			GLint log_length = 0;
			functions.GetShaderiv(shader, GL_INFO_LOG_LENGTH_VALUE, &log_length);
			std::string log(static_cast<std::size_t>((log_length > 1) ? log_length : 1), '\0');
			GLsizei written = 0;
			functions.GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &written, log.data());
			functions.DeleteShader(shader);
			log.resize(static_cast<std::size_t>(written > 0 ? written : 0));
			BL_LOG(LogLevel::Error, "OpenGL", std::string(stage) + " shader compilation failed: " + log);
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
									 std::string(stage) + " shader compilation failed: " + log);
		}

		/// Avoids issuing texture deletion commands into a lost or unrelated GL context.
		class OpenGLUiRenderer final : public RenderInterface_GL2
		{
		public:
			/// Releases a texture only while its owning context is available.
			void ReleaseTexture(Rml::TextureHandle texture) override
			{
				if (gpu_available)
					RenderInterface_GL2::ReleaseTexture(texture);
			}

			bool gpu_available = true;
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
			{
				self.close = true;
				ReleaseWindowInput(self.input);
			}
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

			if (self.ui_runtime)
				self.ui_runtime->system.SetWindow(window);
			if (!ForwardWindowInput(self.input, self.ui_context, self.ui_input.get(),
									window, &self.ui_keys, message, wparam, lparam))
				return 0;
			return DefSubclassProc(window, message, wparam, lparam);
		}

		/// Keeps layout, font rasterization, and rendering in the window's pixel space.
		void SynchronizeSurface()
		{
			RECT rectangle{};
			if (window && GetClientRect(window, &rectangle))
			{
				width = IsIconic(window) ? 0u :
					static_cast<std::uint32_t>((std::max)(0L, rectangle.right - rectangle.left));
				height = IsIconic(window) ? 0u :
					static_cast<std::uint32_t>((std::max)(0L, rectangle.bottom - rectangle.top));
			}
			const UINT current_dpi = GetWindowDpi(window);
			if (ui_context)
			{
				ui_context->SetDimensions({static_cast<int>(width), static_cast<int>(height)});
				ui_context->SetDensityIndependentPixelRatio(static_cast<float>(current_dpi) / USER_DEFAULT_SCREEN_DPI);
				if (dpi != current_dpi)
					BL_LOG(LogLevel::Info, "OpenGL", "UI surface: " + std::to_string(width) + "x" + std::to_string(height) + " pixels, DPI " + std::to_string(current_dpi) + ", dp ratio " + std::to_string(ui_context->GetDensityIndependentPixelRatio()));
			}
			dpi = current_dpi;
		}

		/// Makes the owning context current and reports resets as a recoverable failure.
		void EnsureCurrent(RenderingError unavailable = RenderingError::SubmissionFailed)
		{
			if (context_lost)
				throw RenderingException(RenderingError::DeviceLost, "OpenGL", "OpenGL context was lost");
			if (!context || !functions_loaded)
				throw RenderingException(unavailable, "OpenGL", "device is not initialized");
			if (!wglMakeCurrent(dc, context) || !glGetString(GL_VERSION) ||
				(get_reset_status && get_reset_status() != GL_NO_ERROR_VALUE_LOCAL))
			{
				context_lost = true;
				throw RenderingException(RenderingError::DeviceLost, "OpenGL",
										 "WGL context is unavailable or the graphics device was reset");
			}
		}

		/// Creates a 3.3 compatibility context, requesting reset notification when supported.
		void CreateContext()
		{
			context = wglCreateContext(dc);
			if (!context || !wglMakeCurrent(dc, context))
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL",
										 "WGL bootstrap context creation failed");
			using CreateContextProc = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
			using ExtensionsProc = const char *(WINAPI *)(HDC);
			const auto create = FindOpenGLFunction<CreateContextProc>("wglCreateContextAttribsARB");
			const auto extensions_proc = FindOpenGLFunction<ExtensionsProc>("wglGetExtensionsStringARB");
			const char *extensions = extensions_proc ? extensions_proc(dc) : nullptr;
			const bool robust_supported = extensions &&
				(" " + std::string(extensions) + " ").find(
					" WGL_ARB_create_context_robustness ") != std::string::npos;
			if (!create)
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL",
										 "WGL_ARB_create_context is required for OpenGL 3.3");
			const int attributes[] = {
				kWglContextMajorVersion, 3, kWglContextMinorVersion, 3,
				kWglContextProfileMask, kWglContextCompatibilityProfile, 0};
			const int robust_attributes[] = {
				kWglContextMajorVersion, 3, kWglContextMinorVersion, 3,
				kWglContextProfileMask, kWglContextCompatibilityProfile,
				0x2094, 0x0004, // WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB.
				0x8256, 0x8252, // RESET_NOTIFICATION_STRATEGY_ARB, LOSE_CONTEXT_ON_RESET_ARB.
				0};
			wglMakeCurrent(nullptr, nullptr);
			HGLRC modern = robust_supported ? create(dc, nullptr, robust_attributes) : nullptr;
			const bool robust = modern != nullptr;
			if (!modern)
				modern = create(dc, nullptr, attributes);
			if (!modern)
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL",
										 "OpenGL 3.3 compatibility context creation failed");
			if (!wglMakeCurrent(dc, modern))
			{
				wglDeleteContext(modern);
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL",
										 "OpenGL 3.3 context activation failed");
			}
			wglDeleteContext(context);
			context = modern;
			functions = {};
			if (!functions.Load())
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL",
										 "OpenGL 3.3 entry-point loading failed");
			get_reset_status = nullptr;
			if (robust)
			{
				get_reset_status = FindOpenGLFunction<ResetStatusProc>("glGetGraphicsResetStatus");
				if (!get_reset_status)
					get_reset_status = FindOpenGLFunction<ResetStatusProc>("glGetGraphicsResetStatusARB");
				if (!get_reset_status)
					get_reset_status = FindOpenGLFunction<ResetStatusProc>("glGetGraphicsResetStatusKHR");
			}
			functions_loaded = true;
			context_lost = false;
			capabilities = {};
			GLint maximum = 0;
			glGetIntegerv(GL_MAX_TEXTURE_SIZE_VALUE_LOCAL, &maximum);
			capabilities.max_texture_width = capabilities.max_texture_height =
				static_cast<std::uint32_t>(maximum > 0 ? maximum : 0);
			glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_VALUE_LOCAL, &maximum);
			capabilities.max_render_targets = static_cast<std::uint32_t>(maximum > 0 ? maximum : 1);
			glGetIntegerv(GL_MAX_SAMPLES_VALUE_LOCAL, &maximum);
			capabilities.max_samples = static_cast<std::uint32_t>(maximum > 0 ? maximum : 1);
			using SwapIntervalProc = BOOL(WINAPI *)(int);
			if (const auto interval = FindOpenGLFunction<SwapIntervalProc>("wglSwapIntervalEXT"))
				interval(config.surface.vertical_sync ? 1 : 0);
			device_id = g_next_device_id.fetch_add(1, std::memory_order_relaxed);
			lifetime->Activate(true);
			if (ui_renderer)
				ui_renderer->gpu_available = true;
			EnsureCurrent();
		}

		/// Invalidates old resources without executing GL deletion in an unrelated context.
		void ReleaseContext() noexcept
		{
			const bool current = context && wglMakeCurrent(dc, context);
			lifetime->Shutdown(current && !context_lost);
			active_color_target.reset();
			active_depth_target.reset();
			current_framebuffer = 0;
			if (context)
			{
				if (current && !context_lost && (timer_queries[0] || timer_queries[1]))
					functions.DeleteQueries(2, timer_queries.data());
				timer_queries = {};
				timer_query_pending = {};
				timer_query_active = false;
				if (current)
					wglMakeCurrent(nullptr, nullptr);
				wglDeleteContext(context);
				context = nullptr;
			}
			functions_loaded = false;
			get_reset_status = nullptr;
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
		std::unique_ptr<OpenGLUiRenderer> ui_renderer;
		std::unique_ptr<TextInputMethodEditor_Win32> ui_input;
		std::array<bool, 256> ui_keys{};
		Rml::Context *ui_context = nullptr;
		OpenGLFunctions functions;
		std::shared_ptr<DeviceLifetime> lifetime = std::make_shared<DeviceLifetime>();
		std::uint64_t device_id = 0;
		::BlackLight::Engine::Rendering::DeviceCapabilities capabilities{};
		GLuint current_framebuffer = 0;
		std::shared_ptr<Texture::Impl> active_color_target;
		std::shared_ptr<Texture::Impl> active_depth_target;
		bool functions_loaded = false;
		bool context_lost = false;
		using ResetStatusProc = GLenum(APIENTRY *)();
		ResetStatusProc get_reset_status = nullptr;
		std::array<GLuint, 2> timer_queries{};
		std::array<bool, 2> timer_query_pending{};
		std::size_t timer_query_write_index = 0;
		bool timer_query_active = false;
		bool timer_query_skipped = false;
	};

	/// Creates an uninitialized OpenGL device.
	OpenGLDevice::OpenGLDevice() : impl_(std::make_unique<Impl>())
	{
	}
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
		try
		{
			if (config.surface.mode == SurfaceMode::AttachWindow)
			{
				impl_->window = reinterpret_cast<HWND>(config.surface.native_window);
				if (!IsWindow(impl_->window))
					throw RenderingException(RenderingError::InvalidConfiguration, "OpenGL",
											 "external HWND is invalid");
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
			const DWORD style = config.surface.resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX);
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
			impl_->CreateContext();
			RECT client{};
			GetClientRect(impl_->window, &client);
			impl_->width = IsIconic(impl_->window) ? 0u :
				static_cast<std::uint32_t>((std::max)(0L, client.right - client.left));
			impl_->height = IsIconic(impl_->window) ? 0u :
				static_cast<std::uint32_t>((std::max)(0L, client.bottom - client.top));
		if (config.enable_ui)
		{
			impl_->ui_renderer = std::make_unique<OpenGLUiRenderer>();
			impl_->ui_input = std::make_unique<TextInputMethodEditor_Win32>();
			impl_->ui_runtime = UiRuntime::Acquire("OpenGL");
			impl_->ui_runtime->system.SetWindow(impl_->window);
			const std::string name = "blacklight-" + std::to_string(reinterpret_cast<std::uintptr_t>(impl_.get()));
			impl_->ui_context = Rml::CreateContext(name,
												   {static_cast<int>(impl_->width), static_cast<int>(impl_->height)}, impl_->ui_renderer.get(), impl_->ui_input.get());
			if (!impl_->ui_context)
				throw RenderingException(RenderingError::DeviceCreationFailed, "OpenGL", "RmlUi context creation failed");
		}
		impl_->SynchronizeSurface();
			if (impl_->owns_window)
				ShowWindow(impl_->window, SW_SHOW);
		}
		catch (...)
		{
			Shutdown();
			throw;
		}
	}

	/// Pumps Win32 events and updates drawable dimensions.
	bool OpenGLDevice::ProcessEvents()
	{
		if (impl_->close)
			return false;
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
		impl_->EnsureCurrent();
		if (!impl_->width || !impl_->height)
			MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		if (impl_->lifetime)
			impl_->lifetime->Flush();
		return !impl_->close;
	}

	/// Clears the default color and depth buffers.
	void OpenGLDevice::BeginFrame()
	{
		impl_->EnsureCurrent();
		impl_->active_color_target.reset();
		impl_->active_depth_target.reset();
		impl_->current_framebuffer = 0;
		impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, 0);
		if (!impl_->width || !impl_->height)
			return;
		glViewport(0, 0, static_cast<GLsizei>(impl_->width), static_cast<GLsizei>(impl_->height));
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClearStencil(0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		impl_->EnsureCurrent();
	}

	/// Binds and clears a texture framebuffer for off-screen rendering.
	void OpenGLDevice::BeginFrame(const Texture *color_target, const Texture *depth_target)
	{
		impl_->EnsureCurrent();
		if (!color_target && !depth_target)
		{
			impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, 0);
			impl_->current_framebuffer = 0;
			impl_->active_color_target.reset();
			impl_->active_depth_target.reset();
			BeginFrame();
			return;
		}
		if (!color_target || !color_target->impl_ || !color_target->impl_->render_target ||
			color_target->impl_->device_id != impl_->device_id ||
			color_target->impl_->backend != RenderingBackend::OpenGL)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "invalid color render target");
		const GLuint framebuffer = static_cast<GLuint>(color_target->impl_->tertiary);
		if (!framebuffer)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "color texture has no framebuffer");
		if (depth_target)
		{
			if (!depth_target->impl_ || depth_target->impl_->format != ResourceFormat::D24S8 ||
				!depth_target->impl_->render_target ||
				depth_target->impl_->device_id != impl_->device_id ||
				depth_target->impl_->backend != RenderingBackend::OpenGL ||
				depth_target->impl_->width != color_target->impl_->width ||
				depth_target->impl_->height != color_target->impl_->height ||
				depth_target->impl_->sample_count != color_target->impl_->sample_count)
				throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "invalid depth render target");
		}
		const GLenum depth_texture_target =
			depth_target && depth_target->impl_->sample_count > 1
				? GL_TEXTURE_2D_MULTISAMPLE_VALUE
				: GL_TEXTURE_2D_VALUE;
		impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, framebuffer);
		impl_->functions.FramebufferTexture2D(GL_FRAMEBUFFER_VALUE, GL_DEPTH_STENCIL_ATTACHMENT_VALUE,
											 depth_texture_target,
											 depth_target ? static_cast<GLuint>(depth_target->impl_->native) : 0, 0);
		if (impl_->functions.CheckFramebufferStatus(GL_FRAMEBUFFER_VALUE) != GL_FRAMEBUFFER_COMPLETE_VALUE)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "render target framebuffer is incomplete");
		impl_->current_framebuffer = framebuffer;
		impl_->active_color_target = color_target->impl_;
		impl_->active_depth_target = depth_target ? depth_target->impl_ : nullptr;
		glViewport(0, 0, static_cast<GLsizei>(color_target->impl_->width),
				   static_cast<GLsizei>(color_target->impl_->height));
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	/// Updates and renders the UI while preserving application OpenGL state.
	void OpenGLDevice::RenderUi()
	{
		if (!impl_->ui_context || impl_->close || !impl_->width || !impl_->height)
			return;
		impl_->EnsureCurrent();
		impl_->ui_context->Update();
		GLint matrix_mode = GL_MODELVIEW;
		GLint current_program = 0;
		GLint framebuffer = 0;
		GLint vertex_array = 0;
		GLint array_buffer = 0;
		GLint element_buffer = 0;
		glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
		glGetIntegerv(GL_CURRENT_PROGRAM_VALUE_LOCAL, &current_program);
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_VALUE_LOCAL, &framebuffer);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING_VALUE_LOCAL, &vertex_array);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING_VALUE_LOCAL, &array_buffer);
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_VALUE_LOCAL, &element_buffer);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
		impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, 0);
		impl_->functions.UseProgram(0);
		impl_->functions.BindVertexArray(0);
		impl_->functions.BindBuffer(GL_ARRAY_BUFFER_VALUE, 0);
		impl_->functions.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_VALUE, 0);
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
		try
		{
			impl_->ui_context->Render();
		}
		catch (...)
		{
			impl_->ui_renderer->EndFrame();
			for (const GLenum mode : {GL_MODELVIEW, GL_TEXTURE, GL_PROJECTION})
			{
				glMatrixMode(mode);
				glPopMatrix();
			}
			glPopClientAttrib();
			glPopAttrib();
			impl_->functions.BindVertexArray(static_cast<GLuint>(vertex_array));
			impl_->functions.BindBuffer(GL_ARRAY_BUFFER_VALUE, static_cast<GLuint>(array_buffer));
			impl_->functions.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_VALUE, static_cast<GLuint>(element_buffer));
			impl_->functions.UseProgram(static_cast<GLuint>(current_program));
			impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, static_cast<GLuint>(framebuffer));
			glMatrixMode(matrix_mode);
			throw;
		}
		impl_->ui_renderer->EndFrame();
		for (const GLenum mode : {GL_MODELVIEW, GL_TEXTURE, GL_PROJECTION})
		{
			glMatrixMode(mode);
			glPopMatrix();
		}
		glPopClientAttrib();
		glPopAttrib();
		impl_->functions.BindVertexArray(static_cast<GLuint>(vertex_array));
		impl_->functions.BindBuffer(GL_ARRAY_BUFFER_VALUE, static_cast<GLuint>(array_buffer));
		impl_->functions.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_VALUE, static_cast<GLuint>(element_buffer));
		impl_->functions.UseProgram(static_cast<GLuint>(current_program));
		impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, static_cast<GLuint>(framebuffer));
		glMatrixMode(matrix_mode);
	}
	/// Presents the WGL double-buffered drawable.
	void OpenGLDevice::Present()
	{
		impl_->EnsureCurrent(RenderingError::PresentationFailed);
		if (!impl_->width || !impl_->height)
			return;
		if (!impl_->dc || !SwapBuffers(impl_->dc))
		{
			impl_->EnsureCurrent(RenderingError::PresentationFailed);
			throw RenderingException(RenderingError::PresentationFailed, "OpenGL", "SwapBuffers failed");
		}
		impl_->EnsureCurrent(RenderingError::PresentationFailed);
	}

	/// Recreates a lost WGL context while retaining UI documents and the native window.
	void OpenGLDevice::Recover()
	{
		if (!impl_->window || !IsWindow(impl_->window) || impl_->close)
			throw RenderingException(RenderingError::DeviceRecoveryFailed, "OpenGL",
									 "recovery requires a live surface");
		BL_LOG(LogLevel::Warning, "OpenGL", "Recreating the rendering context");
		ReleaseWindowInput(impl_->input);
		if (impl_->ui_renderer)
		{
			impl_->ui_renderer->gpu_available = false;
			Rml::ReleaseCompiledGeometry(impl_->ui_renderer.get());
			Rml::ReleaseTextures(impl_->ui_renderer.get());
		}
		impl_->ReleaseContext();
		try
		{
			if (!impl_->dc)
			{
				impl_->dc = GetDC(impl_->window);
				if (!impl_->dc)
					throw RenderingException(RenderingError::DeviceRecoveryFailed, "OpenGL",
											 "GetDC failed during recovery");
			}
			impl_->CreateContext();
			if (impl_->ui_context)
			{
				impl_->ui_runtime->system.SetWindow(impl_->window);
				impl_->ui_renderer->SetViewport(static_cast<int>(impl_->width),
												 static_cast<int>(impl_->height));
				Rml::ReleaseFontResources();
			}
			impl_->SynchronizeSurface();
			BL_LOG(LogLevel::Info, "OpenGL", "Rendering context restored");
		}
		catch (const std::exception &error)
		{
			impl_->ReleaseContext();
			throw RenderingException(RenderingError::DeviceRecoveryFailed, "OpenGL", error.what());
		}
	}

	/// Releases OpenGL, DC, and owned window resources.
	void OpenGLDevice::Shutdown() noexcept
	{
		if (!impl_)
			return;
		if (impl_->context && !impl_->context_lost)
			wglMakeCurrent(impl_->dc, impl_->context);
		if (impl_->ui_context)
		{
			Rml::RemoveContext(impl_->ui_context->GetName());
			impl_->ui_context = nullptr;
		}
		if (impl_->ui_renderer)
		{
			if (impl_->functions_loaded && !impl_->context_lost)
			{
				Rml::ReleaseCompiledGeometry(impl_->ui_renderer.get());
				Rml::ReleaseTextures(impl_->ui_renderer.get());
			}
			impl_->ui_renderer->gpu_available = false;
		}
		if (impl_->ui_runtime)
		{
			Rml::ReleaseRenderManagers();
			impl_->ui_runtime->system.SetWindow(nullptr);
			impl_->ui_runtime.reset();
		}
		impl_->ui_input.reset();
		impl_->ui_renderer.reset();
		impl_->ReleaseContext();
		if (impl_->subclassed && IsWindow(impl_->window))
			RemoveWindowSubclass(impl_->window, Impl::WindowProcedure, reinterpret_cast<UINT_PTR>(impl_.get()));
		impl_->subclassed = false;
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
		impl_->context_lost = false;
		impl_->width = impl_->height = 0;
		impl_->input = nullptr;
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
	/// Returns the limits and optional features detected at initialization.
	DeviceCapabilities OpenGLDevice::GetCapabilities() const noexcept { return impl_->capabilities; }
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

	/// Executes recorded normalized commands in their original order.
	void OpenGLDevice::Submit(CommandList &commands)
	{
		impl_->EnsureCurrent();
		if (!commands.impl_)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "command list was moved from");

		auto &functions = impl_->functions;
		std::shared_ptr<GraphicsPipeline::Impl> pipeline;
		std::shared_ptr<Buffer::Impl> vertex_buffer;
		std::shared_ptr<Buffer::Impl> index_buffer;
		std::uint32_t vertex_stride = 0;
		std::uint32_t vertex_offset = 0;
		std::uint32_t index_offset = 0;
		ResourceFormat index_format = ResourceFormat::R16Uint;
		bool has_vertex_buffer = false;
		bool has_index_buffer = false;

		auto validate_owner = [&](std::uint64_t device_id, RenderingBackend backend)
		{
			if (device_id != impl_->device_id || backend != RenderingBackend::OpenGL)
				throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
										 "command list contains a resource from another device");
		};
		auto apply_vertex_buffer = [&]()
		{
			if (!pipeline || !has_vertex_buffer)
				return;
			if (vertex_stride > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()))
				throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
										 "vertex stride exceeds OpenGL limits");
			functions.BindVertexArray(static_cast<GLuint>(pipeline->auxiliary));
			functions.BindBuffer(GL_ARRAY_BUFFER_VALUE, static_cast<GLuint>(vertex_buffer->native));
			for (const auto &attribute : pipeline->attributes)
			{
				GLenum scalar_type = GL_FLOAT;
				std::uint32_t component_bytes = 0;
				if (!MapVertexFormat(attribute.format, scalar_type, component_bytes))
					throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "unsupported vertex attribute format");
				const std::uintptr_t address = static_cast<std::uintptr_t>(vertex_offset) + attribute.offset;
				if (address > vertex_buffer->size)
					throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "vertex attribute offset exceeds its buffer");
				functions.EnableVertexAttribArray(attribute.location);
				if (attribute.format == ResourceFormat::R16Uint || attribute.format == ResourceFormat::R32Uint)
					functions.VertexAttribIPointer(attribute.location, static_cast<GLint>(attribute.component_count),
													scalar_type, static_cast<GLsizei>(vertex_stride),
													reinterpret_cast<const void *>(address));
				else
					functions.VertexAttribPointer(attribute.location, static_cast<GLint>(attribute.component_count),
												  scalar_type, attribute.format == ResourceFormat::R8Unorm ? GL_TRUE : GL_FALSE,
												  static_cast<GLsizei>(vertex_stride),
												  reinterpret_cast<const void *>(address));
			}
		};
		auto uniform_location = [&](const std::string &name) -> GLint
		{
			auto found = pipeline->uniform_locations.find(name);
			if (found != pipeline->uniform_locations.end())
				return found->second;
			const GLint location = functions.GetUniformLocation(static_cast<GLuint>(pipeline->native), name.c_str());
			pipeline->uniform_locations.emplace(name, location);
			return location;
		};
		auto require_uniform_type = [&](const std::string &name, GLenum expected)
		{
			const auto found = pipeline->uniform_types.find(name);
			if (found == pipeline->uniform_types.end() || found->second != expected)
				throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
										 "uniform type does not match the supplied value: " + name);
		};

		try
		{
			for (const auto &entry : commands.impl_->commands)
			{
				std::visit([&](const auto &command)
				{
					using T = std::decay_t<decltype(command)>;
					if constexpr (std::is_same_v<T, CommandList::Impl::PipelineCommand>)
					{
						pipeline = command.pipeline;
						validate_owner(pipeline->device_id, pipeline->backend);
						const std::uint32_t target_samples =
							impl_->active_color_target
								? impl_->active_color_target->sample_count
								: (impl_->active_depth_target ? impl_->active_depth_target->sample_count : 1);
						if (pipeline->sample_count != target_samples)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "pipeline sample count does not match the active render target");
						functions.UseProgram(static_cast<GLuint>(pipeline->native));
						functions.BindVertexArray(static_cast<GLuint>(pipeline->auxiliary));
						if (pipeline->depth_test)
							glEnable(GL_DEPTH_TEST);
						else
							glDisable(GL_DEPTH_TEST);
						glDepthMask(pipeline->depth_write ? GL_TRUE : GL_FALSE);
						if (pipeline->alpha_blending)
						{
							glEnable(GL_BLEND);
							functions.BlendFuncSeparate(
								GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
						}
						else
						{
							glDisable(GL_BLEND);
						}
						if (pipeline->cull_mode == GraphicsPipelineDesc::CullMode::None)
						{
							glDisable(GL_CULL_FACE);
						}
						else
						{
							glEnable(GL_CULL_FACE);
							glCullFace(pipeline->cull_mode == GraphicsPipelineDesc::CullMode::Front ? GL_FRONT_VALUE : GL_BACK_VALUE);
						}
						if (pipeline->srgb)
							glEnable(GL_FRAMEBUFFER_SRGB_VALUE_LOCAL);
						else
							glDisable(GL_FRAMEBUFFER_SRGB_VALUE_LOCAL);
						apply_vertex_buffer();
						if (has_index_buffer)
						{
							validate_owner(index_buffer->device_id, index_buffer->backend);
							functions.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_VALUE,
												 static_cast<GLuint>(index_buffer->native));
						}
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::VertexBufferCommand>)
					{
						vertex_buffer = command.buffer;
						validate_owner(vertex_buffer->device_id, vertex_buffer->backend);
						vertex_stride = command.stride;
						vertex_offset = command.offset;
						has_vertex_buffer = true;
						apply_vertex_buffer();
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::IndexBufferCommand>)
					{
						index_buffer = command.buffer;
						validate_owner(index_buffer->device_id, index_buffer->backend);
						index_format = command.format;
						index_offset = command.offset;
						has_index_buffer = true;
						if (pipeline)
						{
							functions.BindVertexArray(static_cast<GLuint>(pipeline->auxiliary));
							functions.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_VALUE,
												 static_cast<GLuint>(index_buffer->native));
						}
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::ViewportCommand>)
					{
						if (command.viewport.x < static_cast<float>(std::numeric_limits<GLint>::min()) ||
							command.viewport.x > static_cast<float>(std::numeric_limits<GLint>::max()) ||
							command.viewport.y < static_cast<float>(std::numeric_limits<GLint>::min()) ||
							command.viewport.y > static_cast<float>(std::numeric_limits<GLint>::max()) ||
							command.viewport.width > static_cast<float>(std::numeric_limits<GLsizei>::max()) ||
							command.viewport.height > static_cast<float>(std::numeric_limits<GLsizei>::max()))
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "viewport exceeds OpenGL integer limits");
						glViewport(static_cast<GLint>(command.viewport.x), static_cast<GLint>(command.viewport.y),
								   static_cast<GLsizei>(command.viewport.width), static_cast<GLsizei>(command.viewport.height));
						glDepthRange(command.viewport.minimum_depth, command.viewport.maximum_depth);
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::UniformCommand>)
					{
						if (!pipeline)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "uniform command requires a pipeline");
						const GLenum expected_type = command.matrix
														 ? GL_FLOAT_MAT4_VALUE
														 : command.component_count == 1
															   ? GL_FLOAT_VALUE
															   : command.component_count == 2
																	 ? GL_FLOAT_VEC2_VALUE
																	 : command.component_count == 3
																		   ? GL_FLOAT_VEC3_VALUE
																		   : GL_FLOAT_VEC4_VALUE;
						require_uniform_type(command.name, expected_type);
						const GLint location = uniform_location(command.name);
						if (location < 0)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "uniform not found in current pipeline: " + command.name);
						if (command.matrix)
							functions.UniformMatrix4fv(location, 1, GL_TRUE, command.values.data());
						else if (command.component_count == 1)
							functions.Uniform1f(location, command.values[0]);
						else if (command.component_count == 2)
							functions.Uniform2fv(location, 1, command.values.data());
						else if (command.component_count == 3)
							functions.Uniform3fv(location, 1, command.values.data());
						else
							functions.Uniform4fv(location, 1, command.values.data());
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::TextureCommand>)
					{
						if (!pipeline)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "texture command requires a pipeline");
						validate_owner(command.texture->device_id, command.texture->backend);
						if (command.texture->sample_count != 1)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "multisampled textures must be resolved before sampling");
						if (command.texture == impl_->active_color_target ||
							command.texture == impl_->active_depth_target)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "an active render target cannot be sampled in the same pass");
						if (command.slot >= 32)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "texture slot exceeds the OpenGL 3.3 minimum");
						require_uniform_type(command.uniform_name, GL_SAMPLER_2D_VALUE);
						const GLint location = uniform_location(command.uniform_name);
						if (location < 0)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "sampler uniform not found in current pipeline: " + command.uniform_name);
						functions.ActiveTexture(GL_TEXTURE0_VALUE + command.slot);
						functions.BindTexture(GL_TEXTURE_2D_VALUE, static_cast<GLuint>(command.texture->native));
						functions.BindSampler(command.slot, static_cast<GLuint>(command.texture->auxiliary));
						functions.Uniform1i(location, static_cast<GLint>(command.slot));
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::DrawCommand>)
					{
						if (!pipeline || !has_vertex_buffer)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "draw requires a pipeline and vertex buffer");
						const std::size_t available_vertices = (vertex_buffer->size - vertex_offset) / vertex_stride;
						if (command.first > static_cast<std::uint32_t>(std::numeric_limits<GLint>::max()) ||
							command.count > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()) ||
							command.first > available_vertices ||
							command.count > available_vertices - command.first)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "draw range exceeds the vertex buffer");
						functions.DrawArrays(MapTopology(pipeline->topology), static_cast<GLint>(command.first),
											 static_cast<GLsizei>(command.count));
					}
					else if constexpr (std::is_same_v<T, CommandList::Impl::DrawIndexedCommand>)
					{
						if (!pipeline || !has_vertex_buffer || !has_index_buffer)
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
													 "indexed draw requires a pipeline and both buffers");
						const std::size_t index_size = index_format == ResourceFormat::R16Uint ? 2 : 4;
						const std::size_t available = (index_buffer->size - index_offset) / index_size;
						if (command.first > available || command.count > available - command.first ||
							command.count > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()))
							throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "indexed draw range exceeds the index buffer");
						const std::uintptr_t byte_offset =
							static_cast<std::uintptr_t>(index_offset) + static_cast<std::uintptr_t>(command.first) * index_size;
						functions.DrawElementsBaseVertex(MapTopology(pipeline->topology),
														 static_cast<GLsizei>(command.count),
														 index_format == ResourceFormat::R16Uint ? GL_UNSIGNED_SHORT_VALUE : GL_UNSIGNED_INT_VALUE,
														 reinterpret_cast<const void *>(byte_offset), command.vertex_offset);
					}
				}, entry);
			}
		}
		catch (const RenderingException &)
		{
			throw;
		}
		catch (const std::exception &error)
		{
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", error.what());
		}
		pipeline.reset();
		vertex_buffer.reset();
		index_buffer.reset();
		commands.Clear();
		impl_->lifetime->Flush();
	}

	/// Creates an OpenGL buffer with checked size, usage, and ownership metadata.
	Buffer OpenGLDevice::CreateBuffer(const BufferDesc &description)
	{
		impl_->EnsureCurrent(RenderingError::ResourceCreationFailed);
		if (description.size == 0 || description.size > static_cast<std::size_t>(std::numeric_limits<GLSizePtr>::max()))
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "buffer size is invalid or too large");
		if (description.usage == BufferUsage::Constant && description.size % 16 != 0)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
									 "constant buffer sizes must be multiples of 16 bytes");

		// ARRAY_BUFFER keeps allocation independent from the currently bound VAO.
		const GLenum target = description.usage == BufferUsage::Constant
								  ? GL_UNIFORM_BUFFER_VALUE
								  : GL_ARRAY_BUFFER_VALUE;
		GLuint buffer = 0;
		impl_->functions.GenBuffers(1, &buffer);
		if (!buffer)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "glGenBuffers returned zero");
		impl_->functions.BindBuffer(target, buffer);
		impl_->functions.BufferData(target, static_cast<GLSizePtr>(description.size),
									description.initial_data,
									description.usage == BufferUsage::DynamicVertex
										? GL_DYNAMIC_DRAW_VALUE
										: GL_STATIC_DRAW_VALUE);
		Buffer result;
		try
		{
			auto resource = std::make_shared<Buffer::Impl>();
			resource->size = description.size;
			resource->usage = description.usage;
			resource->device_id = impl_->device_id;
			resource->backend = RenderingBackend::OpenGL;
			resource->lifetime = impl_->lifetime;
			resource->native = buffer;
			const auto delete_buffers = impl_->functions.DeleteBuffers;
			resource->deletion = impl_->lifetime->Register([delete_buffers, buffer]()
			{
				delete_buffers(1, &buffer);
			});
			result.impl_ = std::move(resource);
		}
		catch (...)
		{
			impl_->functions.DeleteBuffers(1, &buffer);
			throw;
		}
		return result;
	}

	/// Updates a checked buffer byte range using the streaming OpenGL path.
	void OpenGLDevice::UpdateBuffer(Buffer &buffer, std::size_t offset,
									const void *data, std::size_t size)
	{
		impl_->EnsureCurrent(RenderingError::ResourceCreationFailed);
		if (!buffer.impl_ || buffer.impl_->usage != BufferUsage::DynamicVertex ||
			buffer.impl_->device_id != impl_->device_id ||
			buffer.impl_->backend != RenderingBackend::OpenGL ||
			offset > buffer.impl_->size || size > buffer.impl_->size - offset ||
			(size && !data) ||
			size > static_cast<std::size_t>(std::numeric_limits<GLSizePtr>::max()) ||
			offset > static_cast<std::size_t>(std::numeric_limits<GLSizePtr>::max()))
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
									 "dynamic vertex-buffer update range or owner is invalid");
		if (!size)
			return;
		GLint previous_binding = 0;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING_VALUE_LOCAL, &previous_binding);
		impl_->functions.BindBuffer(GL_ARRAY_BUFFER_VALUE, static_cast<GLuint>(buffer.impl_->native));
		if (offset == 0)
			impl_->functions.BufferData(GL_ARRAY_BUFFER_VALUE,
										static_cast<GLSizePtr>(buffer.impl_->size),
										nullptr, GL_DYNAMIC_DRAW_VALUE);
		impl_->functions.BufferSubData(GL_ARRAY_BUFFER_VALUE, static_cast<std::ptrdiff_t>(offset),
									   static_cast<GLSizePtr>(size), data);
		impl_->functions.BindBuffer(GL_ARRAY_BUFFER_VALUE, static_cast<GLuint>(previous_binding));
		impl_->EnsureCurrent(RenderingError::ResourceCreationFailed);
	}

	/// Starts an available non-blocking OpenGL elapsed-time query.
	void OpenGLDevice::BeginGpuTimerQuery()
	{
		impl_->EnsureCurrent(RenderingError::UnsupportedOperation);
		if (impl_->timer_query_active)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
									 "a GPU timer query is already active");
		if (!impl_->timer_queries[0] && !impl_->timer_queries[1])
			impl_->functions.GenQueries(2, impl_->timer_queries.data());
		impl_->timer_query_skipped = false;
		for (std::size_t attempt = 0; attempt < impl_->timer_queries.size(); ++attempt)
		{
			const std::size_t index = (impl_->timer_query_write_index + attempt) % impl_->timer_queries.size();
			if (impl_->timer_queries[index] && !impl_->timer_query_pending[index])
			{
				impl_->timer_query_write_index = index;
				impl_->functions.BeginQuery(GL_TIME_ELAPSED_QUERY_VALUE, impl_->timer_queries[index]);
				impl_->timer_query_active = true;
				return;
			}
		}
		impl_->timer_query_skipped = true;
	}

	/// Ends the active elapsed-time query and makes its result eligible for polling.
	void OpenGLDevice::EndGpuTimerQuery()
	{
		if (!impl_->timer_query_active)
			return;
		impl_->functions.EndQuery(GL_TIME_ELAPSED_QUERY_VALUE);
		impl_->timer_query_pending[impl_->timer_query_write_index] = true;
		impl_->timer_query_write_index =
			(impl_->timer_query_write_index + 1) % impl_->timer_queries.size();
		impl_->timer_query_active = false;
	}

	/// Returns an available OpenGL duration in milliseconds without stalling.
	bool OpenGLDevice::TryGetGpuTimeMilliseconds(double &milliseconds)
	{
		impl_->EnsureCurrent(RenderingError::UnsupportedOperation);
		for (std::size_t index = 0; index < impl_->timer_queries.size(); ++index)
		{
			if (!impl_->timer_query_pending[index])
				continue;
			GLint available = GL_FALSE;
			impl_->functions.GetQueryObjectiv(
				impl_->timer_queries[index], GL_QUERY_RESULT_AVAILABLE_VALUE_LOCAL, &available);
			if (available != GL_TRUE)
				continue;
			std::uint64_t elapsed_nanoseconds = 0;
			impl_->functions.GetQueryObjectui64v(
				impl_->timer_queries[index], GL_QUERY_RESULT_VALUE_LOCAL, &elapsed_nanoseconds);
			impl_->timer_query_pending[index] = false;
			milliseconds = static_cast<double>(elapsed_nanoseconds) / 1000000.0;
			return true;
		}
		return false;
	}

	/// Creates a sampled two-dimensional texture and sampler state.
	Texture OpenGLDevice::CreateTexture(const TextureDesc &description)
	{
		impl_->EnsureCurrent(RenderingError::ResourceCreationFailed);
		if (!description.width || !description.height || !description.mip_levels)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "texture dimensions and mip count must be non-zero");
		std::uint32_t maximum_mip_levels = 1;
		for (std::uint32_t dimension = (std::max)(description.width, description.height);
			 dimension > 1; dimension >>= 1)
			++maximum_mip_levels;
		if (description.mip_levels > maximum_mip_levels)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "texture mip count exceeds its dimensions");
		if (!IsValidSampleCount(description.sample_count) ||
			description.sample_count > impl_->capabilities.max_samples)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
									 "texture sample count is not supported");
		const bool multisampled = description.sample_count > 1;
		if (multisampled && (description.mip_levels != 1 || description.initial_data ||
							 description.initial_data_size || !description.render_target))
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
									 "multisampled textures require one mip level, no initial data, and render_target");
		GLint maximum_size = 0;
		glGetIntegerv(0x0D33, &maximum_size);
		if (description.width > static_cast<std::uint32_t>(maximum_size) ||
			description.height > static_cast<std::uint32_t>(maximum_size))
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "texture exceeds GL_MAX_TEXTURE_SIZE");
		const std::size_t pixel_size = FormatSize(description.format);
		if (!pixel_size || description.width > std::numeric_limits<std::size_t>::max() / description.height ||
			static_cast<std::size_t>(description.width) * description.height >
				std::numeric_limits<std::size_t>::max() / pixel_size)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "texture byte size overflows size_t");
		std::size_t required_data_size = 0;
		std::uint32_t data_width = description.width;
		std::uint32_t data_height = description.height;
		for (std::uint32_t level = 0; level < description.mip_levels; ++level)
		{
			const std::size_t level_size = static_cast<std::size_t>(data_width) * data_height * pixel_size;
			if (required_data_size > std::numeric_limits<std::size_t>::max() - level_size)
				throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "texture mip data size overflows size_t");
			required_data_size += level_size;
			data_width = data_width > 1 ? data_width / 2 : 1;
			data_height = data_height > 1 ? data_height / 2 : 1;
		}
		if (description.initial_data && description.initial_data_size < required_data_size)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "initial texture data does not contain every mip level");
		if (!description.initial_data && description.initial_data_size != 0)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "initial data size is non-zero for a null pointer");
		GLint internal_format = 0;
		GLenum external_format = 0;
		GLenum data_type = 0;
		if (!MapTextureFormat(description.format, internal_format, external_format, data_type))
			throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL", "texture format is not supported");

		GLuint texture = 0;
		GLuint sampler = 0;
		GLuint framebuffer = 0;
		impl_->functions.GenTextures(1, &texture);
		if (!multisampled)
			impl_->functions.GenSamplers(1, &sampler);
		if (!texture || (!multisampled && !sampler))
		{
			if (texture)
				impl_->functions.DeleteTextures(1, &texture);
			if (sampler)
				impl_->functions.DeleteSamplers(1, &sampler);
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "texture or sampler allocation failed");
		}
		try
		{
			const GLenum texture_target = multisampled ? GL_TEXTURE_2D_MULTISAMPLE_VALUE : GL_TEXTURE_2D_VALUE;
			impl_->functions.BindTexture(texture_target, texture);
			impl_->functions.PixelStorei(GL_UNPACK_ALIGNMENT_VALUE, 1);
			if (multisampled)
			{
				impl_->functions.TexImage2DMultisample(
					texture_target, static_cast<GLsizei>(description.sample_count), internal_format,
					static_cast<GLsizei>(description.width), static_cast<GLsizei>(description.height), GL_TRUE);
			}
			else
			{
				std::uint32_t mip_width = description.width;
				std::uint32_t mip_height = description.height;
				std::size_t data_offset = 0;
				const auto *initial_bytes = static_cast<const std::uint8_t *>(description.initial_data);
				for (std::uint32_t level = 0; level < description.mip_levels; ++level)
				{
					const void *level_data = initial_bytes ? initial_bytes + data_offset : nullptr;
					impl_->functions.TexImage2D(texture_target, static_cast<GLint>(level), internal_format,
												static_cast<GLsizei>(mip_width), static_cast<GLsizei>(mip_height), 0,
												external_format, data_type, level_data);
					data_offset += static_cast<std::size_t>(mip_width) * mip_height * pixel_size;
					mip_width = mip_width > 1 ? mip_width / 2 : 1;
					mip_height = mip_height > 1 ? mip_height / 2 : 1;
				}
				const bool uses_mip_filter = description.mip_levels > 1;
				const GLint minimum_filter = uses_mip_filter
												 ? (description.minification_filter == TextureFilter::Nearest
														? static_cast<GLint>(GL_NEAREST_MIPMAP_NEAREST_VALUE)
														: static_cast<GLint>(GL_LINEAR_MIPMAP_LINEAR_VALUE))
												 : MapFilter(description.minification_filter);
				const GLint magnification_filter = MapFilter(description.magnification_filter);
				impl_->functions.TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER_LOCAL, minimum_filter);
				impl_->functions.TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER_LOCAL, magnification_filter);
				impl_->functions.TexParameteri(texture_target, GL_TEXTURE_WRAP_S_LOCAL,
												MapAddressMode(description.address_u));
				impl_->functions.TexParameteri(texture_target, GL_TEXTURE_WRAP_T_LOCAL,
												MapAddressMode(description.address_v));
				impl_->functions.TexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL_VALUE,
												static_cast<GLint>(description.mip_levels - 1));
				impl_->functions.SamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER_LOCAL, minimum_filter);
				impl_->functions.SamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER_LOCAL, magnification_filter);
				impl_->functions.SamplerParameteri(sampler, GL_TEXTURE_WRAP_S_LOCAL, MapAddressMode(description.address_u));
				impl_->functions.SamplerParameteri(sampler, GL_TEXTURE_WRAP_T_LOCAL, MapAddressMode(description.address_v));
			}
			if (description.render_target && description.format != ResourceFormat::D24S8)
			{
				impl_->functions.GenFramebuffers(1, &framebuffer);
				if (!framebuffer)
					throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "framebuffer allocation failed");
				impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, framebuffer);
				impl_->functions.FramebufferTexture2D(GL_FRAMEBUFFER_VALUE, GL_COLOR_ATTACHMENT0_VALUE,
													 texture_target, texture, 0);
				if (impl_->functions.CheckFramebufferStatus(GL_FRAMEBUFFER_VALUE) != GL_FRAMEBUFFER_COMPLETE_VALUE)
					throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "color texture framebuffer is incomplete");
				impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, 0);
			}

			Texture result;
			auto resource = std::make_shared<Texture::Impl>();
			resource->width = description.width;
			resource->height = description.height;
			resource->mip_levels = description.mip_levels;
			resource->format = description.format;
			resource->render_target = description.render_target;
			resource->sample_count = description.sample_count;
			resource->device_id = impl_->device_id;
			resource->backend = RenderingBackend::OpenGL;
			resource->lifetime = impl_->lifetime;
			resource->native = texture;
			resource->auxiliary = sampler;
			resource->tertiary = framebuffer;
			const auto delete_textures = impl_->functions.DeleteTextures;
			const auto delete_samplers = impl_->functions.DeleteSamplers;
			const auto delete_framebuffers = impl_->functions.DeleteFramebuffers;
			resource->deletion = impl_->lifetime->Register([delete_textures, delete_samplers,
															 delete_framebuffers, texture, sampler, framebuffer]()
			{
				delete_textures(1, &texture);
				delete_samplers(1, &sampler);
				if (framebuffer)
					delete_framebuffers(1, &framebuffer);
			});
			result.impl_ = std::move(resource);
			return result;
		}
		catch (...)
		{
			impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, 0);
			if (framebuffer)
				impl_->functions.DeleteFramebuffers(1, &framebuffer);
			impl_->functions.DeleteSamplers(1, &sampler);
			impl_->functions.DeleteTextures(1, &texture);
			throw;
		}
	}

	/// Resolves a multisampled color target into a single-sample render target.
	void OpenGLDevice::ResolveTexture(const Texture &source, const Texture &destination)
	{
		impl_->EnsureCurrent(RenderingError::SubmissionFailed);
		const auto *source_impl = source.impl_.get();
		const auto *destination_impl = destination.impl_.get();
		if (!source_impl || !destination_impl)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL", "resolve texture is empty");
		if (source_impl->device_id != impl_->device_id || source_impl->backend != RenderingBackend::OpenGL ||
			destination_impl->device_id != impl_->device_id || destination_impl->backend != RenderingBackend::OpenGL)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
									 "resolve textures must belong to this device");
		if (source_impl->format == ResourceFormat::D24S8)
			throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL",
									 "depth targets cannot be resolved");
		if (source_impl->sample_count <= 1 || !source_impl->render_target || !source_impl->tertiary)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
									 "resolve source must be a multisampled render target");
		if (destination_impl->sample_count != 1 || !destination_impl->render_target || !destination_impl->tertiary)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
									 "resolve destination must be a single-sample render target");
		if (source_impl->format != destination_impl->format ||
			source_impl->width != destination_impl->width ||
			source_impl->height != destination_impl->height)
			throw RenderingException(RenderingError::SubmissionFailed, "OpenGL",
									 "resolve textures must share a format and dimensions");

		const GLuint read_framebuffer = static_cast<GLuint>(source_impl->tertiary);
		const GLuint draw_framebuffer = static_cast<GLuint>(destination_impl->tertiary);
		impl_->functions.BindFramebuffer(GL_READ_FRAMEBUFFER_VALUE, read_framebuffer);
		impl_->functions.BindFramebuffer(GL_DRAW_FRAMEBUFFER_VALUE, draw_framebuffer);
		// Resolve raw samples without applying a second sRGB conversion.
		const GLboolean srgb_enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB_VALUE_LOCAL);
		if (srgb_enabled)
			glDisable(GL_FRAMEBUFFER_SRGB_VALUE_LOCAL);
		impl_->functions.BlitFramebuffer(
			0, 0, static_cast<GLint>(source_impl->width), static_cast<GLint>(source_impl->height),
			0, 0, static_cast<GLint>(destination_impl->width), static_cast<GLint>(destination_impl->height),
			GL_COLOR_BUFFER_BIT_VALUE, GL_NEAREST_FILTER_VALUE);
		if (srgb_enabled)
			glEnable(GL_FRAMEBUFFER_SRGB_VALUE_LOCAL);
		impl_->functions.BindFramebuffer(GL_FRAMEBUFFER_VALUE, impl_->current_framebuffer);
		impl_->EnsureCurrent(RenderingError::SubmissionFailed);
	}

	/// Compiles a backend GLSL program and creates its input vertex array.
	GraphicsPipeline OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc &description)
	{
		impl_->EnsureCurrent(RenderingError::ResourceCreationFailed);
		if (description.vertex_attribute_count && !description.vertex_attributes)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "vertex attributes pointer is null");
		if (!description.vertex_stride && description.vertex_attribute_count)
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "vertex stride must be non-zero");
		if (!IsValidSampleCount(description.sample_count) ||
			description.sample_count > impl_->capabilities.max_samples)
			throw RenderingException(RenderingError::UnsupportedOperation, "OpenGL",
									 "pipeline sample count is not supported");

		const char *vertex_source = nullptr;
		const char *fragment_source = nullptr;
		try
		{
			vertex_source = ShaderContract::GetSource(description.vertex_shader, RenderingBackend::OpenGL);
			fragment_source = ShaderContract::GetSource(description.fragment_shader, RenderingBackend::OpenGL);
		}
		catch (const std::exception &error)
		{
			throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", error.what());
		}

		GLuint vertex_shader = 0;
		GLuint fragment_shader = 0;
		GLuint program = 0;
		GLuint vertex_array = 0;
		try
		{
			vertex_shader = CompileShader(impl_->functions, GL_VERTEX_SHADER_VALUE, vertex_source, "vertex");
			fragment_shader = CompileShader(impl_->functions, GL_FRAGMENT_SHADER_VALUE, fragment_source, "fragment");
			program = impl_->functions.CreateProgram();
			if (!program)
				throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "glCreateProgram returned zero");
			impl_->functions.AttachShader(program, vertex_shader);
			impl_->functions.AttachShader(program, fragment_shader);
			for (std::uint32_t index = 0; index < description.vertex_attribute_count; ++index)
			{
				const auto &attribute = description.vertex_attributes[index];
				GLenum scalar_type = GL_FLOAT;
				std::uint32_t component_bytes = 0;
				if (attribute.component_count == 0 || attribute.component_count > 4 ||
					attribute.component_count != FormatComponents(attribute.format) ||
					attribute.offset > description.vertex_stride ||
					!MapVertexFormat(attribute.format, scalar_type, component_bytes) ||
					attribute.component_count * component_bytes > description.vertex_stride - attribute.offset)
					throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "vertex attribute layout is invalid");
				for (std::uint32_t previous = 0; previous < index; ++previous)
					if (description.vertex_attributes[previous].location == attribute.location)
						throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "vertex attribute locations must be unique");
				const std::string fallback = "a" + std::to_string(attribute.location);
				const char *semantic = attribute.semantic_name && *attribute.semantic_name
										  ? attribute.semantic_name
										  : fallback.c_str();
				impl_->functions.BindAttribLocation(program, attribute.location, semantic);
			}
			impl_->functions.LinkProgram(program);
			GLint linked = GL_FALSE;
			impl_->functions.GetProgramiv(program, GL_LINK_STATUS_VALUE, &linked);
			if (linked != GL_TRUE)
			{
				GLint log_length = 0;
				impl_->functions.GetProgramiv(program, GL_INFO_LOG_LENGTH_VALUE, &log_length);
				std::string log(static_cast<std::size_t>(log_length > 1 ? log_length : 1), '\0');
				GLsizei written = 0;
				impl_->functions.GetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &written, log.data());
				log.resize(static_cast<std::size_t>(written > 0 ? written : 0));
				BL_LOG(LogLevel::Error, "OpenGL", "pipeline link failed: " + log);
				throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "pipeline link failed: " + log);
			}
			std::unordered_map<std::string, std::uint32_t> uniform_types;
			GLint active_uniforms = 0;
			GLint maximum_uniform_name = 0;
			impl_->functions.GetProgramiv(program, GL_ACTIVE_UNIFORMS_VALUE, &active_uniforms);
			impl_->functions.GetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH_VALUE, &maximum_uniform_name);
			if (active_uniforms > 0 && maximum_uniform_name <= 1)
				throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL",
										 "active uniform metadata is invalid");
			std::string uniform_name(static_cast<std::size_t>(maximum_uniform_name), '\0');
			for (GLint index = 0; index < active_uniforms; ++index)
			{
				GLsizei written = 0;
				GLint array_size = 0;
				GLenum uniform_type = 0;
				impl_->functions.GetActiveUniform(
					program, static_cast<GLuint>(index), static_cast<GLsizei>(uniform_name.size()),
					&written, &array_size, &uniform_type, uniform_name.data());
				if (written <= 0 || static_cast<std::size_t>(written) >= uniform_name.size())
					continue;
				std::string name(uniform_name.data(), static_cast<std::size_t>(written));
				uniform_types.emplace(name, uniform_type);
				const std::string array_suffix = "[0]";
				if (array_size > 1 && name.size() > array_suffix.size() &&
					name.compare(name.size() - array_suffix.size(), array_suffix.size(), array_suffix) == 0)
				{
					name.resize(name.size() - array_suffix.size());
					uniform_types.emplace(std::move(name), uniform_type);
				}
			}
			impl_->functions.GenVertexArrays(1, &vertex_array);
			if (!vertex_array)
				throw RenderingException(RenderingError::ResourceCreationFailed, "OpenGL", "vertex array allocation failed");

			auto resource = std::make_shared<GraphicsPipeline::Impl>();
			resource->vertex_stride = description.vertex_stride;
			resource->color_format = description.color_format;
			resource->depth_format = description.depth_format;
			resource->depth_test = description.depth_test;
			resource->depth_write = description.depth_write;
			resource->alpha_blending = description.alpha_blending;
			resource->cull_mode = description.cull_mode;
			resource->topology = description.topology;
			resource->sample_count = description.sample_count;
			resource->srgb = description.srgb;
			resource->vertex_source = vertex_source;
			resource->fragment_source = fragment_source;
			resource->entry_point = description.vertex_shader.entry_point ? description.vertex_shader.entry_point : "main";
			resource->uniform_types = std::move(uniform_types);
			if (description.vertex_attribute_count)
			{
				resource->attributes.assign(description.vertex_attributes,
											description.vertex_attributes + description.vertex_attribute_count);
				resource->attribute_semantics.reserve(description.vertex_attribute_count);
				for (std::uint32_t index = 0; index < description.vertex_attribute_count; ++index)
					resource->attribute_semantics.emplace_back(description.vertex_attributes[index].semantic_name
																 ? description.vertex_attributes[index].semantic_name
																 : "");
			}
			resource->device_id = impl_->device_id;
			resource->backend = RenderingBackend::OpenGL;
			resource->lifetime = impl_->lifetime;
			resource->native = program;
			resource->auxiliary = vertex_array;
			const auto delete_program = impl_->functions.DeleteProgram;
			const auto delete_vertex_arrays = impl_->functions.DeleteVertexArrays;
			resource->deletion = impl_->lifetime->Register([delete_program, delete_vertex_arrays, program, vertex_array]()
			{
				delete_program(program);
				delete_vertex_arrays(1, &vertex_array);
			});
			impl_->functions.DeleteShader(vertex_shader);
			impl_->functions.DeleteShader(fragment_shader);
			GraphicsPipeline result;
			result.impl_ = std::move(resource);
			return result;
		}
		catch (...)
		{
			if (vertex_array)
				impl_->functions.DeleteVertexArrays(1, &vertex_array);
			if (program)
				impl_->functions.DeleteProgram(program);
			if (vertex_shader)
				impl_->functions.DeleteShader(vertex_shader);
			if (fragment_shader)
				impl_->functions.DeleteShader(fragment_shader);
			throw;
		}
	}
}
