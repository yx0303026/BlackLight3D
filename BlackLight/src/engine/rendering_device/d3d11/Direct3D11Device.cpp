#include "../DeviceImplementation.h"

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>

#include <string>

namespace BlackLight::Engine::Rendering::Detail
{
	namespace
	{
		constexpr wchar_t kWindowClass[] = L"BlackLightRenderingDeviceD3D11";
		Direct3D11Device *g_active_device = nullptr;

		/// Routes native close, resize, and mouse messages into the active D3D11 host.
		LRESULT CALLBACK DeviceWindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
		{
			if (message == WM_CLOSE)
			{
				PostQuitMessage(0);
				return 0;
			}
			if (message == WM_SIZE && g_active_device)
			{
				return DefWindowProcW(window, message, wparam, lparam);
			}
			return DefWindowProcW(window, message, wparam, lparam);
		}

		/// Releases a COM interface and clears the stored pointer.
		template <typename T>
		void Release(T *&value) noexcept
		{
			if (value)
			{
				value->Release();
				value = nullptr;
			}
		}
	}

	/// Owns D3D11 device, context, swap chain, and render target.
	class Direct3D11Device::Impl
	{
	public:
		DeviceConfig config;
		InputState *input = nullptr;
		HWND window = nullptr;
		bool owns_window = false;
		bool close = false;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		ID3D11Device *device = nullptr;
		ID3D11DeviceContext *context = nullptr;
		IDXGISwapChain *swap_chain = nullptr;
		ID3D11RenderTargetView *render_target = nullptr;
	};

	/// Creates an uninitialized D3D11 device.
	Direct3D11Device::Direct3D11Device() : impl_(std::make_unique<Impl>()) {}
	/// Releases device and native window resources.
	Direct3D11Device::~Direct3D11Device() { Shutdown(); }

	/// Initializes D3D11 and creates or attaches the presentation surface.
	void Direct3D11Device::Initialize(const DeviceConfig &config, InputState *input)
	{
		impl_->config = config;
		impl_->input = input;
		impl_->width = config.surface.width;
		impl_->height = config.surface.height;
		if (config.surface.mode == SurfaceMode::AttachWindow)
		{
			impl_->window = reinterpret_cast<HWND>(config.surface.native_window);
			if (!IsWindow(impl_->window))
				throw RenderingException(RenderingError::InvalidConfiguration, "Direct3D11", "external HWND is invalid");
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
				static_cast<int>(config.surface.height), nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
			if (!impl_->window)
				throw RenderingException(RenderingError::WindowCreationFailed, "Direct3D11", "CreateWindowExW failed");
			impl_->owns_window = true;
		}

		DXGI_SWAP_CHAIN_DESC swap_desc{};
		swap_desc.BufferCount = 2;
		swap_desc.BufferDesc.Width = config.surface.width;
		swap_desc.BufferDesc.Height = config.surface.height;
		swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_desc.OutputWindow = impl_->window;
		swap_desc.SampleDesc.Count = 1;
		swap_desc.Windowed = TRUE;
		swap_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
		D3D_FEATURE_LEVEL obtained{};
		const HRESULT result = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, requested,
			static_cast<UINT>(sizeof(requested) / sizeof(requested[0])), D3D11_SDK_VERSION,
			&swap_desc, &impl_->swap_chain, &impl_->device, &obtained, &impl_->context);
		if (FAILED(result))
			throw RenderingException(RenderingError::DeviceCreationFailed, "Direct3D11", "D3D11CreateDeviceAndSwapChain failed");

		ID3D11Texture2D *back_buffer = nullptr;
		if (FAILED(impl_->swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&back_buffer))))
			throw RenderingException(RenderingError::DeviceCreationFailed, "Direct3D11", "swap-chain back buffer query failed");
		const HRESULT target_result = impl_->device->CreateRenderTargetView(back_buffer, nullptr, &impl_->render_target);
		back_buffer->Release();
		if (FAILED(target_result))
			throw RenderingException(RenderingError::DeviceCreationFailed, "Direct3D11", "render-target view creation failed");
		ShowWindow(impl_->window, SW_SHOW);
		g_active_device = this;
	}

	/// Pumps native events and refreshes drawable size.
	bool Direct3D11Device::ProcessEvents()
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

	/// Clears the swap-chain render target and binds it for drawing.
	void Direct3D11Device::BeginFrame()
	{
		if (!impl_->context || !impl_->render_target)
			throw RenderingException(RenderingError::SubmissionFailed, "Direct3D11", "device is not initialized");
		const float color[] = {0.2f, 0.2f, 0.2f, 1.0f};
		impl_->context->OMSetRenderTargets(1, &impl_->render_target, nullptr);
		impl_->context->ClearRenderTargetView(impl_->render_target, color);
	}

	/// Does nothing until a D3D11 RmlUi adapter is available.
	void Direct3D11Device::RenderUi() {}
	/// Presents the current swap-chain buffer.
	void Direct3D11Device::Present()
	{
		if (!impl_->swap_chain ||
			FAILED(impl_->swap_chain->Present(impl_->config.surface.vertical_sync ? 1 : 0, 0)))
			throw RenderingException(RenderingError::PresentationFailed, "Direct3D11", "swap-chain Present failed");
	}
	/// Releases D3D11 and window resources.
	void Direct3D11Device::Shutdown() noexcept
	{
		if (!impl_)
			return;
		if (g_active_device == this)
			g_active_device = nullptr;
		Release(impl_->render_target);
		Release(impl_->swap_chain);
		Release(impl_->context);
		Release(impl_->device);
		if (impl_->owns_window && impl_->window)
			DestroyWindow(impl_->window);
		impl_->window = nullptr;
		impl_->owns_window = false;
	}
	/// Returns surface dimensions and close state.
	SurfaceState Direct3D11Device::GetSurfaceState() const noexcept
	{
		return {impl_->width, impl_->height, impl_->close};
	}
	/// Sets the host input sink.
	void Direct3D11Device::SetInputSink(InputState *input) noexcept { impl_->input = input; }
	/// Returns the D3D11 backend identity.
	RenderingBackend Direct3D11Device::GetBackend() const noexcept { return RenderingBackend::Direct3D11; }
	/// Returns the native window handle.
	NativeWindowHandle Direct3D11Device::GetNativeWindow() const noexcept
	{
		return reinterpret_cast<NativeWindowHandle>(impl_->window);
	}
	/// Returns no UI context until the optional D3D11 UI adapter is added.
	Rml::Context *Direct3D11Device::GetUiContext() const noexcept { return nullptr; }
	/// Returns no system interface until the optional D3D11 UI adapter is added.
	Rml::SystemInterface *Direct3D11Device::GetUiSystemInterface() const noexcept { return nullptr; }
	/// Creates a normalized command list.
	CommandList Direct3D11Device::CreateCommandList() { return {}; }
	/// Rejects draw commands until the D3D11 pipeline executor is installed.
	void Direct3D11Device::Submit(CommandList &commands)
	{
		if (commands.GetDrawCount() != 0)
			throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11", "normalized draw submission is not implemented");
	}
	/// Rejects D3D11 buffer creation until normalized resource binding is implemented.
	Buffer Direct3D11Device::CreateBuffer(const BufferDesc &)
	{
		throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11", "buffer creation is not implemented");
	}
	/// Rejects D3D11 texture creation until normalized resource binding is implemented.
	Texture Direct3D11Device::CreateTexture(const TextureDesc &)
	{
		throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11", "texture creation is not implemented");
	}
	/// Rejects D3D11 pipeline creation until normalized shader compilation is implemented.
	GraphicsPipeline Direct3D11Device::CreateGraphicsPipeline(const GraphicsPipelineDesc &)
	{
		throw RenderingException(RenderingError::UnsupportedOperation, "Direct3D11", "pipeline creation is not implemented");
	}
}
