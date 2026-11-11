#pragma once

#include "../BlackLightApi.h"

#include <memory>
#include <cstdint>

namespace Rml
{
	class Context;
	class RenderManager;
	class SystemInterface;
}

namespace BlackLight::Engine
{
	class InputState;

	/// Result of a graphics operation.
	enum class GraphicsResult
	{
		Success,
		WindowCreationFailed,
		ContextCreationFailed,
		RenderFailed,
		NotInitialized
	};

	/// Represents a window for rendering.
	struct GraphicsWindow
	{
		uint32_t width = 1280;
		uint32_t height = 720;
		const char *title = "BlackLight3D";
		bool resizable = true;
		bool vsync = true;
	};

	/// Abstracts the graphics backend (OpenGL, DirectX, Vulkan, etc.).
	class BLACKLIGHT_API IGraphicsBackend
	{
	public:
		virtual ~IGraphicsBackend() = default;

		/// Initializes the graphics backend and creates a window.
		virtual GraphicsResult Initialize(const GraphicsWindow &window) = 0;

		/// Returns the RmlUi context for UI building, or nullptr if not initialized.
		virtual Rml::Context *GetContext() const noexcept = 0;

		/// Returns the RmlUi system interface, or nullptr if not available.
		virtual Rml::SystemInterface *GetSystemInterface() const noexcept = 0;

		/// Returns the RmlUi render manager, or nullptr if not available.
		virtual Rml::RenderManager *GetRenderManager() const noexcept = 0;

		/// Connects a platform input sink. Backends that do not expose input may ignore it.
		virtual void SetInputSink(InputState *input) noexcept;

		/// Returns the current drawable width in pixels, or zero when unavailable.
		virtual uint32_t GetDrawableWidth() const noexcept;

		/// Returns the current drawable height in pixels, or zero when unavailable.
		virtual uint32_t GetDrawableHeight() const noexcept;

		/// Updates input and processes one frame. Returns false if the window should close.
		virtual bool Update() = 0;

		/// Renders the current frame.
		virtual GraphicsResult Render() = 0;

		/// Presents the rendered frame to the screen.
		virtual GraphicsResult Present() = 0;

		/// Shuts down the graphics backend and releases resources.
		virtual void Shutdown() = 0;

		/// Returns true if the window should close.
		virtual bool ShouldClose() const noexcept = 0;

		IGraphicsBackend(const IGraphicsBackend &) = delete;
		IGraphicsBackend &operator=(const IGraphicsBackend &) = delete;
		IGraphicsBackend(IGraphicsBackend &&) = delete;
		IGraphicsBackend &operator=(IGraphicsBackend &&) = delete;

	protected:
		IGraphicsBackend() = default;
	};

	/// Factory function to create the appropriate graphics backend for the platform.
	/// Backends are registered at link time (e.g., OpenGL, DirectX, Vulkan).
	BLACKLIGHT_API std::unique_ptr<IGraphicsBackend> CreateGraphicsBackend(const char *backend_name);

} // namespace BlackLight::Engine
