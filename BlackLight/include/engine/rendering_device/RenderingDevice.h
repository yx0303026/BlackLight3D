#pragma once

#include "../../BlackLightApi.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace Rml
{
	class Context;
	class SystemInterface;
}

namespace BlackLight::Engine
{
	class InputState;

	namespace Rendering
	{
		class Buffer;
		class CommandList;
		class Texture;
		class GraphicsPipeline;
		struct BufferDesc;
		struct TextureDesc;
		struct GraphicsPipelineDesc;

		/// Reports normalized limits and optional features supported by a device.
		struct DeviceCapabilities
		{
			std::uint32_t max_texture_width = 0;
			std::uint32_t max_texture_height = 0;
			std::uint32_t max_render_targets = 1;
			std::uint32_t max_samples = 1;
			bool compute_shaders = false;
			bool storage_buffers = false;
		};

		/// Selects the graphics API used by a rendering device.
		enum class RenderingBackend
		{
			OpenGL,
			Direct3D11
		};

		/// Selects whether the device creates a window or attaches to an existing HWND.
		enum class SurfaceMode
		{
			CreateWindow,
			AttachWindow
		};

		/// Describes a native Win32 window handle without exposing Windows headers in the SDK.
		using NativeWindowHandle = std::uintptr_t;

		/// Defines the window or external surface used by a rendering device.
		struct SurfaceDesc
		{
			std::uint32_t width = 1280;
			std::uint32_t height = 720;
			std::string title = "BlackLight3D";
			bool resizable = true;
			bool vertical_sync = true;
			SurfaceMode mode = SurfaceMode::CreateWindow;
			NativeWindowHandle native_window = 0;
		};

		/// Configures a rendering device and its presentation surface.
		struct DeviceConfig
		{
			RenderingBackend backend = RenderingBackend::OpenGL;
			SurfaceDesc surface{};
			bool enable_ui = true;
		};

		/// Identifies a normalized rendering-device failure.
		enum class RenderingError
		{
			InvalidConfiguration,
			WindowCreationFailed,
			DeviceCreationFailed,
			UnsupportedOperation,
			ResourceCreationFailed,
			SubmissionFailed,
			PresentationFailed
		};

		/// Reports a rendering failure with a stable error code and backend identity.
		class BLACKLIGHT_API RenderingException : public std::runtime_error
		{
		public:
			/// Creates an exception containing the backend and diagnostic message.
			RenderingException(RenderingError code, std::string backend, std::string message);
			/// Returns the normalized error code.
			RenderingError GetCode() const noexcept;
			/// Returns the backend name associated with the failure.
			const std::string &GetBackend() const noexcept;

		private:
			RenderingError code_;
			std::string backend_;
		};

		/// Describes drawable dimensions and close state for the active surface.
		struct SurfaceState
		{
			std::uint32_t width = 0;
			std::uint32_t height = 0;
			bool should_close = false;
		};

		/// Provides a normalized lifecycle and presentation interface for graphics APIs.
		class BLACKLIGHT_API RenderingDevice
		{
		public:
			/// Releases device, surface, and optional UI resources.
			virtual ~RenderingDevice();
			/// Initializes the selected backend and configured surface.
			virtual void Initialize(const DeviceConfig &config, InputState *input) = 0;
			/// Pumps native events and updates the input snapshot.
			virtual bool ProcessEvents() = 0;
			/// Begins a frame and clears the default color/depth target.
			virtual void BeginFrame() = 0;
			/// Begins a frame on a texture color target and optional depth target.
			virtual void BeginFrame(const Texture *color_target, const Texture *depth_target) = 0;
			/// Renders the optional UI overlay after application drawing.
			virtual void RenderUi() = 0;
			/// Presents the current frame to the configured surface.
			virtual void Present() = 0;
			/// Releases device resources and detaches from the native surface.
			virtual void Shutdown() noexcept = 0;
			/// Returns the current drawable dimensions and close state.
			virtual SurfaceState GetSurfaceState() const noexcept = 0;
			/// Connects a platform input sink to the native event pump.
			virtual void SetInputSink(InputState *input) noexcept = 0;
			/// Returns the selected backend.
			virtual RenderingBackend GetBackend() const noexcept = 0;
			/// Returns device limits and optional features.
			virtual DeviceCapabilities GetCapabilities() const noexcept = 0;
			/// Returns the native window handle, or zero if unavailable.
			virtual NativeWindowHandle GetNativeWindow() const noexcept = 0;
			/// Returns the optional RmlUi context, or null when UI is disabled.
			virtual Rml::Context *GetUiContext() const noexcept = 0;
			/// Returns the optional RmlUi system interface.
			virtual Rml::SystemInterface *GetUiSystemInterface() const noexcept = 0;
			/// Creates a normalized command list for the current frame.
			virtual CommandList CreateCommandList() = 0;
			/// Executes a recorded command list.
			virtual void Submit(CommandList &commands) = 0;
			/// Creates a backend buffer using the normalized description.
			virtual Buffer CreateBuffer(const BufferDesc &description) = 0;
			/// Creates a backend texture using the normalized description.
			virtual Texture CreateTexture(const TextureDesc &description) = 0;
			/// Creates a backend graphics pipeline.
			virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc &description) = 0;

			/// Creates a platform implementation from the supplied configuration.
			static std::unique_ptr<RenderingDevice> Create(const DeviceConfig &config);
		};
	}
}
