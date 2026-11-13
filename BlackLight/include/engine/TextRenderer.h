#pragma once

#include "Math.h"
#include "SpriteBatch.h"
#include "rendering_device/RenderingDevice.h"
#include "rendering_device/RenderingResources.h"

#include <cstdint>
#include <memory>
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
		/// Appends glyph quads in the active sprite camera's coordinate system.
		void Draw(SpriteBatch &batch, std::string_view text, Math::Vector2 position,
				  float scale = 1.0f, Math::Color color = {}) const;
		/// Returns the line height in pixels before scaling.
		float GetLineHeight() const noexcept;

	private:
		/// Keeps glyph layout and platform font resources outside the public ABI.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}
