#pragma once

#include "../BlackLightApi.h"
#include "rendering_device/RenderingDevice.h"

#include <memory>

namespace Rml
{
	class Context;
}

namespace BlackLight::Engine
{
	class InputState;

	/// Configures a desktop host and its normalized rendering device.
	struct WindowConfig
	{
		Rendering::DeviceConfig rendering{};
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Owns desktop input, rendering, optional UI, and the application lifecycle.
	class BLACKLIGHT_API Window
	{
	public:
		/// Creates an inactive desktop host with the supplied device configuration.
		explicit Window(WindowConfig config = {});
		/// Releases the rendering device and native resources.
		virtual ~Window();
		/// Prevents copying a native window owner.
		Window(const Window &) = delete;
		/// Prevents assigning a native window owner.
		Window &operator=(const Window &) = delete;
		/// Keeps native callback addresses stable.
		Window(Window &&) = delete;
		/// Keeps native callback addresses stable.
		Window &operator=(Window &&) = delete;

		/// Initializes the rendering device and application.
		void Initialize();
		/// Runs the desktop loop and shuts down before returning.
		int Run();
		/// Releases application and rendering resources.
		void Shutdown();
		/// Requests loop termination.
		void Exit() noexcept;
		/// Returns the current input snapshot.
		InputState &GetInput() noexcept;
		/// Returns the normalized rendering device.
		Rendering::RenderingDevice *GetRenderingDevice() noexcept;
		/// Returns the optional RmlUi context.
		Rml::Context *GetContext() const noexcept;

	protected:
		/// Builds application UI after the rendering device is initialized.
		virtual bool OnInitialize() = 0;
		/// Resets application state before initialization.
		virtual void OnStarting();
		/// Updates application state once per frame.
		virtual void OnUpdate();
		/// Records application rendering commands.
		virtual void OnRender();
		/// Releases application resources.
		virtual void OnShutdown();
		/// Releases application GPU resources after the device reports a loss.
		virtual void OnDeviceLost();
		/// Recreates application GPU resources after the device and UI are restored.
		virtual void OnDeviceRestored();

	private:
		/// Stores runtime state outside the public ABI.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP
}
