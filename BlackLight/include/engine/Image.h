#pragma once

#include "Content.h"
#include "rendering_device/RenderingDevice.h"
#include "rendering_device/RenderingResources.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace BlackLight::Engine
{
	/// Owns decoded top-to-bottom RGBA8 pixels for one image.
	struct DecodedImage
	{
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::vector<std::uint8_t> rgba;
	};

	/// Decodes a cached PNG or JPEG asset through Windows Imaging Component.
	BLACKLIGHT_API DecodedImage DecodeImage(ContentManager &content,
											const std::filesystem::path &path);
	/// Decodes and uploads an image asset as a normalized device texture.
	BLACKLIGHT_API Rendering::Texture LoadTexture(ContentManager &content,
												 Rendering::RenderingDevice &device,
												 const std::filesystem::path &path,
												 Rendering::TextureFilter filter = Rendering::TextureFilter::Linear);
}
