#include "engine/Window.h"
#include "engine/Input.h"
#include <utility>

namespace BlackLight::Engine
{
	/// Owns desktop state shared by UI applications and games.
	class Window::Impl
	{
	public:
		/// Copies the title so the backend never observes a temporary config string.
		explicit Impl(WindowConfig value)
			: config(std::move(value)), title(config.window.title ? config.window.title : "BlackLight3D")
		{
			config.window.title = title.c_str();
		}
		WindowConfig config;
		std::string title;
		InputState input;
		std::unique_ptr<IGraphicsBackend> backend;
		bool initialized = false;
		bool application_started = false;
		bool exit_requested = false;
		bool running = false;
		bool initializing = false;
		bool shutting_down = false;
	};

	/// Creates only the state required by the desktop host.
	Window::Window(WindowConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
	/// Releases the backend without dispatching callbacks to a destroyed subclass.
	Window::~Window()
	{
		if (impl_->backend)
			impl_->backend->Shutdown();
	}
	/// Initializes native resources and unwinds partial application initialization on failure.
	GraphicsResult Window::Initialize()
	{
		if (impl_->initializing || impl_->shutting_down)
			return GraphicsResult::NotInitialized;
		if (impl_->initialized)
			return GraphicsResult::Success;
		try
		{
			impl_->backend = CreateGraphicsBackend(impl_->config.backend_name.c_str());
			if (!impl_->backend)
				return GraphicsResult::ContextCreationFailed;
			impl_->input = InputState{};
			impl_->backend->SetInputSink(&impl_->input);
			const auto result = impl_->backend->Initialize(impl_->config.window);
			if (result != GraphicsResult::Success)
			{
				Shutdown();
				return result;
			}
			impl_->exit_requested = false;
			impl_->application_started = true;
			impl_->initializing = true;
			OnStarting();
			const bool ready = OnInitialize();
			impl_->initializing = false;
			if (!ready)
			{
				Shutdown();
				return GraphicsResult::NotInitialized;
			}
			impl_->initialized = true;
			return GraphicsResult::Success;
		}
		catch (...)
		{
			impl_->initializing = false;
			Shutdown();
			throw;
		}
	}
	/// Pumps messages, draws application content and UI, and releases the runtime.
	int Window::Run()
	{
		if (impl_->running)
			return 1;
		if (Initialize() != GraphicsResult::Success)
			return 1;
		impl_->running = true;
		int status = 0;
		try
		{
			while (!impl_->exit_requested && !impl_->backend->ShouldClose())
			{
				impl_->input.BeginFrame();
				if (!impl_->backend->Update() || impl_->backend->ShouldClose())
					break;
				OnUpdate();
				if (impl_->exit_requested)
					break;
				if (impl_->backend->BeginFrame() != GraphicsResult::Success)
				{
					status = 1;
					break;
				}
				OnRender();
				if (impl_->backend->Render() != GraphicsResult::Success ||
					impl_->backend->Present() != GraphicsResult::Success)
				{
					status = 1;
					break;
				}
			}
			impl_->running = false;
			Shutdown();
		}
		catch (...)
		{
			impl_->running = false;
			Shutdown();
			throw;
		}
		return status;
	}
	/// Defers shutdown during callbacks and otherwise releases resources exactly once.
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
				if (impl_->backend)
					impl_->backend->Shutdown();
				impl_->backend.reset();
				impl_->shutting_down = false;
				throw;
			}
		}
		if (impl_->backend)
			impl_->backend->Shutdown();
		impl_->backend.reset();
		impl_->shutting_down = false;
	}
	/// Marks the desktop loop for termination.
	void Window::Exit() noexcept { impl_->exit_requested = true; }
	/// Exposes desktop input without any game service dependency.
	InputState &Window::GetInput() noexcept { return impl_->input; }
	/// Exposes the initialized graphics implementation.
	IGraphicsBackend *Window::GetGraphicsBackend() noexcept { return impl_->backend.get(); }
	/// Exposes the UI context for document construction.
	Rml::Context *Window::GetContext() const noexcept
	{
		return impl_->backend ? impl_->backend->GetContext() : nullptr;
	}
	/// Provides an optional runtime reset hook.
	void Window::OnStarting() {}
	/// Provides an optional desktop update hook.
	void Window::OnUpdate() {}
	/// Provides an optional desktop graphics hook.
	void Window::OnRender() {}
	/// Provides an optional desktop cleanup hook.
	void Window::OnShutdown() {}
}
