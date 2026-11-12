#pragma once

#include "engine/rendering_device/RenderingDevice.h"
#include "engine/rendering_device/RenderingResources.h"
#include "engine/rendering_device/CommandList.h"
#include "engine/Input.h"

#include <memory>

namespace Rml
{
	class Context;
	class RenderInterface;
	class RenderManager;
	class SystemInterface;
}

namespace BlackLight::Engine::Rendering::Detail
{
	/// Implements the normalized device contract for the Win32 OpenGL compatibility path.
	class OpenGLDevice final : public RenderingDevice
	{
	public:
		/// Creates an uninitialized device.
		OpenGLDevice();
		/// Initializes the window and OpenGL context.
		void Initialize(const DeviceConfig &config, InputState *input) override;
		/// Releases native and UI resources.
		~OpenGLDevice() override;
		/// Pumps Win32 events and updates input/UI state.
		bool ProcessEvents() override;
		/// Clears the frame target before application rendering.
		void BeginFrame() override;
		/// Clears and binds an optional texture render target.
		void BeginFrame(const Texture *color_target, const Texture *depth_target) override;
		/// Renders the optional RmlUi overlay.
		void RenderUi() override;
		/// Presents the frame.
		void Present() override;
		/// Releases all resources without throwing.
		void Shutdown() noexcept override;
		/// Recreates the WGL context and UI resources on the existing window.
		void Recover() override;
		/// Returns drawable dimensions and close state.
		SurfaceState GetSurfaceState() const noexcept override;
		/// Connects the input snapshot.
		void SetInputSink(InputState *input) noexcept override;
		/// Returns the selected API.
		RenderingBackend GetBackend() const noexcept override;
		/// Returns device limits and optional features.
		DeviceCapabilities GetCapabilities() const noexcept override;
		/// Returns the Win32 window handle.
		NativeWindowHandle GetNativeWindow() const noexcept override;
		/// Returns the optional RmlUi context.
		Rml::Context *GetUiContext() const noexcept override;
		/// Returns the optional RmlUi system interface.
		Rml::SystemInterface *GetUiSystemInterface() const noexcept override;
		/// Creates an empty command list.
		CommandList CreateCommandList() override;
		/// Executes recorded commands.
		void Submit(CommandList &commands) override;
		/// Creates a device buffer.
		Buffer CreateBuffer(const BufferDesc &description) override;
		/// Creates a device texture.
		Texture CreateTexture(const TextureDesc &description) override;
		/// Resolves a multisampled color target into a single-sample texture.
		void ResolveTexture(const Texture &source, const Texture &destination) override;
		/// Creates a graphics pipeline.
		GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc &description) override;

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	/// Implements normalized presentation for the Win32 Direct3D 11 path.
	class Direct3D11Device final : public RenderingDevice
	{
	public:
		/// Creates an uninitialized device.
		Direct3D11Device();
		/// Initializes the window and D3D11 swap chain.
		void Initialize(const DeviceConfig &config, InputState *input) override;
		/// Releases native resources.
		~Direct3D11Device() override;
		/// Pumps Win32 events.
		bool ProcessEvents() override;
		/// Clears the swap-chain target.
		void BeginFrame() override;
		/// Clears and binds an optional texture render target.
		void BeginFrame(const Texture *color_target, const Texture *depth_target) override;
		/// Renders the optional UI overlay.
		void RenderUi() override;
		/// Presents the frame.
		void Present() override;
		/// Releases all resources without throwing.
		void Shutdown() noexcept override;
		/// Recreates the D3D11 device, swap chain, and UI renderer on the existing window.
		void Recover() override;
		/// Returns drawable dimensions and close state.
		SurfaceState GetSurfaceState() const noexcept override;
		/// Connects the input snapshot.
		void SetInputSink(InputState *input) noexcept override;
		/// Returns the selected API.
		RenderingBackend GetBackend() const noexcept override;
		/// Returns device limits and optional features.
		DeviceCapabilities GetCapabilities() const noexcept override;
		/// Returns the Win32 window handle.
		NativeWindowHandle GetNativeWindow() const noexcept override;
		/// Returns the optional RmlUi context.
		Rml::Context *GetUiContext() const noexcept override;
		/// Returns the optional RmlUi system interface.
		Rml::SystemInterface *GetUiSystemInterface() const noexcept override;
		/// Creates an empty command list.
		CommandList CreateCommandList() override;
		/// Executes recorded commands.
		void Submit(CommandList &commands) override;
		/// Creates a device buffer.
		Buffer CreateBuffer(const BufferDesc &description) override;
		/// Creates a device texture.
		Texture CreateTexture(const TextureDesc &description) override;
		/// Resolves a multisampled color target into a single-sample texture.
		void ResolveTexture(const Texture &source, const Texture &destination) override;
		/// Creates a graphics pipeline.
		GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc &description) override;

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}
