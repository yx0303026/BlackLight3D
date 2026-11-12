#pragma once

#include "Math.h"
#include "SpriteBatch.h"
#include "rendering_device/RenderingDevice.h"
#include "rendering_device/RenderingResources.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace BlackLight::Engine
{
	/// Renders ASCII text from a generated bitmap font atlas into a SpriteBatch.
	class BLACKLIGHT_API TextRenderer final
	{
	public:
		/// Creates a Windows-rasterized bitmap atlas and registers it with the sprite renderer.
		TextRenderer(Rendering::RenderingDevice &device, SpriteBatchRenderer &renderer,
					 std::uint32_t pixel_height = 20);
		/// Unregisters the atlas before releasing its GPU texture.
		~TextRenderer();
		/// Prevents copying the font texture and registration.
		TextRenderer(const TextRenderer &) = delete;
		/// Prevents assigning the font texture and registration.
		TextRenderer &operator=(const TextRenderer &) = delete;
		/// Appends glyph quads at the requested screen-space scale and color.
		void Draw(SpriteBatch &batch, std::string_view text, Math::Vector2 position,
				  float scale = 1.0f, Math::Color color = {}) const;
		/// Returns the line height in pixels before scaling.
		float GetLineHeight() const noexcept;

	private:
		struct Glyph
		{
			std::uint32_t x = 0;
			std::uint32_t y = 0;
			std::uint32_t width = 0;
			std::uint32_t height = 0;
			float advance = 0.0f;
		};

		Rendering::RenderingDevice &device_;
		SpriteBatchRenderer &renderer_;
		Rendering::Texture texture_;
		TextureHandle texture_handle_ = 0;
		std::array<Glyph, 95> glyphs_{};
		std::uint32_t atlas_width_ = 0;
		std::uint32_t atlas_height_ = 0;
		float line_height_ = 0.0f;
	};
}
