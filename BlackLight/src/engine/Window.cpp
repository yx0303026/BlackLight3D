#include "engine/Window.h"
#include "engine/Input.h"

#include <stdexcept>
#include <utility>

namespace BlackLight::Engine
{
	/// Owns the host configuration and lifecycle flags.
	class Window::Impl
	{
	public:
		/// Stores the normalized device configuration.
		explicit Impl(WindowConfig value) : config(std::move(value)) {}

		WindowConfig config;
		InputState input;
		std::unique_ptr<Rendering::RenderingDevice> device;
		bool initialized = false;
		bool application_started = false;
		bool exit_requested = false;
		bool running = false;
		bool initializing = false;
		bool shutting_down = false;
	};

	/// Creates a host with no native resources.
	Window::Window(WindowConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}

	/// Releases the device without invoking virtual callbacks.
	Window::~Window()
	{
		if (impl_->device)
			impl_->device->Shutdown();
	}

	/// Initializes the device and application hooks.
	void Window::Initialize()
	{
		if (impl_->initializing || impl_->shutting_down)
			throw std::logic_error("Window initialization is already in progress");
		if (impl_->initialized)
			return;

		try
		{
			impl_->device = Rendering::RenderingDevice::Create(impl_->config.rendering);
			impl_->input = InputState{};
			impl_->device->Initialize(impl_->config.rendering, &impl_->input);
			impl_->exit_requested = false;
			impl_->application_started = true;
			impl_->initializing = true;
			OnStarting();
			if (!OnInitialize())
				throw std::runtime_error("Application initialization failed");
			impl_->initializing = false;
			impl_->initialized = true;
		}
		catch (...)
		{
			impl_->initializing = false;
			Shutdown();
			throw;
		}
	}

	/// Pumps events, records application work, renders UI, and presents.
	int Window::Run()
	{
		if (impl_->running)
			return 1;
		Initialize();
		impl_->running = true;
		int status = 0;
		try
		{
			while (!impl_->exit_requested && !impl_->device->GetSurfaceState().should_close)
			{
				impl_->input.BeginFrame();
				if (!impl_->device->ProcessEvents())
					break;
				OnUpdate();
				if (impl_->exit_requested)
					break;
				impl_->device->BeginFrame();
				OnRender();
				impl_->device->RenderUi();
				impl_->device->Present();
			}
		}
		catch (...)
		{
			impl_->running = false;
			Shutdown();
			throw;
		}
		impl_->running = false;
		Shutdown();
		return status;
	}

	/// Releases application and device resources exactly once.
	void Window::Shutdown()
	{
		if (impl_->running || impl_->initializing)
		{
			Exit();
			return;
		}
		if (impl_->shutting_down)
			return;
		impl_->shutting_down = true;
		impl_->initialized = false;
		if (impl_->application_started)
		{
			impl_->application_started = false;
			try
			{
				OnShutdown();
			}
			catch (...)
			{
				if (impl_->device)
					impl_->device->Shutdown();
				impl_->device.reset();
				impl_->shutting_down = false;
				throw;
			}
		}
		if (impl_->device)
			impl_->device->Shutdown();
		impl_->device.reset();
		impl_->shutting_down = false;
	}

	/// Requests loop termination.
	void Window::Exit() noexcept { impl_->exit_requested = true; }
	/// Returns the input state.
	InputState &Window::GetInput() noexcept { return impl_->input; }
	/// Returns the active rendering device.
	Rendering::RenderingDevice *Window::GetRenderingDevice() noexcept { return impl_->device.get(); }
	/// Returns the optional UI context.
	Rml::Context *Window::GetContext() const noexcept
	{
		return impl_->device ? impl_->device->GetUiContext() : nullptr;
	}
	/// Provides an optional initialization hook.
	void Window::OnStarting() {}
	/// Provides an optional update hook.
	void Window::OnUpdate() {}
	/// Provides an optional rendering hook.
	void Window::OnRender() {}
	/// Provides an optional shutdown hook.
	void Window::OnShutdown() {}
}
