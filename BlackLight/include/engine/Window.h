#pragma once

#include "../BlackLightApi.h"
#include "IGraphicsBackend.h"
#include <memory>
#include <string>

namespace BlackLight::Engine
{
	class InputState;

	/// Configures a desktop UI host without allocating game services.
	struct WindowConfig
	{
		GraphicsWindow window{};
		std::string backend_name = "windows-opengl";
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Abstract desktop UI host owning window, input, UI rendering, and presentation.
	class BLACKLIGHT_API Window
	{
	public:
		/// Creates an inactive desktop host with the supplied configuration.
		explicit Window(WindowConfig config = {});
		/// Releases native resources; call Shutdown before derived members are destroyed.
		virtual ~Window();
		/// Prevents copying a native window owner.
		Window(const Window &) = delete;
		/// Prevents assigning a native window owner.
		Window &operator=(const Window &) = delete;
		/// Keeps callback and native window addresses stable.
		Window(Window &&) = delete;
		/// Keeps callback and native window addresses stable.
		Window &operator=(Window &&) = delete;
		/// Creates the backend and invokes application initialization.
		GraphicsResult Initialize();
		/// Runs the desktop loop and shuts down; returns nonzero on rendering failure.
		int Run();
		/// Releases application resources before destroying the UI and native context.
		void Shutdown();
		/// Requests termination before the next frame.
		void Exit() noexcept;
		/// Returns the desktop keyboard and mouse state.
		InputState &GetInput() noexcept;
		/// Returns the active backend, or nullptr while inactive.
		IGraphicsBackend *GetGraphicsBackend() noexcept;
		/// Returns the UI context, or nullptr while inactive.
		Rml::Context *GetContext() const noexcept;

	protected:
		/// Builds application UI after the graphics context is available.
		virtual bool OnInitialize() = 0;
		/// Resets runtime services before invoking application initialization.
		virtual void OnStarting();
		/// Updates desktop application state once per frame.
		virtual void OnUpdate();
		/// Draws application graphics before the UI overlay.
		virtual void OnRender();
		/// Releases context-dependent resources, including partial initialization.
		virtual void OnShutdown();

	private:
		/// Stores private runtime state outside the SDK header.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP
}
