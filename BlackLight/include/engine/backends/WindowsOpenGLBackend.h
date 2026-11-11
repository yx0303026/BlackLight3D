#pragma once

#include "../IGraphicsBackend.h"
#include "../Input.h"

#include <windows.h>
#include <memory>

namespace Rml
{
	class Context;
	class RenderInterface;
	class RenderManager;
	class SystemInterface;
}

namespace BlackLight::Engine::Windows
{

	/// Windows Native window + OpenGL rendering backend.
	class WindowsOpenGLBackend final : public IGraphicsBackend
	{
	public:
		WindowsOpenGLBackend();
		~WindowsOpenGLBackend() override;

		GraphicsResult Initialize(const GraphicsWindow &window) override;
		Rml::Context *GetContext() const noexcept override;
		Rml::SystemInterface *GetSystemInterface() const noexcept override;
		Rml::RenderManager *GetRenderManager() const noexcept override;
		void SetInputSink(InputState *input) noexcept override;
		uint32_t GetDrawableWidth() const noexcept override;
		uint32_t GetDrawableHeight() const noexcept override;
		bool Update() override;
		/// Clears the drawable before application rendering.
		GraphicsResult BeginFrame() override;
		GraphicsResult Render() override;
		GraphicsResult Present() override;
		void Shutdown() override;
		bool ShouldClose() const noexcept override;

	private:
		friend LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

		HWND window_handle_ = nullptr;
		HDC device_context_ = nullptr;
		HGLRC opengl_context_ = nullptr;
		bool should_close_ = false;
		bool rmlui_initialized_ = false;
		uint32_t drawable_width_ = 0;
		uint32_t drawable_height_ = 0;
		InputState *input_sink_ = nullptr;

		std::unique_ptr<Rml::Context> rmlui_context_;
		std::unique_ptr<Rml::SystemInterface> rmlui_system_;
		std::unique_ptr<Rml::RenderInterface> rmlui_render_interface_;
		std::unique_ptr<Rml::RenderManager> rmlui_renderer_;

		GraphicsResult InitializeOpenGL(uint32_t width, uint32_t height);
		GraphicsResult ShutdownOpenGL();
	};

} // namespace BlackLight::Engine::Windows
