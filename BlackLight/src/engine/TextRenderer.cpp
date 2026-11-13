#include "engine/TextRenderer.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace BlackLight::Engine
{
	namespace
	{
		constexpr std::uint32_t kGlyphColumns = 16;
		constexpr std::uint32_t kGlyphRows = 6;
		constexpr std::uint32_t kGlyphPadding = 2;

		/// Rasterizes printable ASCII glyphs into a transparent top-down RGBA atlas.
		std::vector<std::uint8_t> BuildFontAtlas(
			std::uint32_t pixel_height, std::array<std::uint32_t, 95> &advances,
			float &line_height, std::uint32_t &cell_width, std::uint32_t &cell_height)
		{
			cell_width = pixel_height * 2u + kGlyphPadding * 2u;
			cell_height = pixel_height + kGlyphPadding * 2u + 8u;
			const std::uint32_t width = kGlyphColumns * cell_width;
			const std::uint32_t height = kGlyphRows * cell_height;
			std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4u);
			BITMAPINFO info{};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = static_cast<LONG>(width);
			info.bmiHeader.biHeight = -static_cast<LONG>(height);
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;
			HDC dc = CreateCompatibleDC(nullptr);
			if (!dc)
				throw std::runtime_error("Unable to create a bitmap-font device context");
			void *dib_pixels = nullptr;
			HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &dib_pixels, nullptr, 0);
			if (!dib || !dib_pixels)
			{
				if (dib)
					DeleteObject(dib);
				DeleteDC(dc);
				throw std::runtime_error("Unable to allocate the bitmap-font atlas");
			}
			HGDIOBJ previous_bitmap = SelectObject(dc, dib);
			HFONT font = CreateFontW(-static_cast<int>(pixel_height), 0, 0, 0, FW_NORMAL,
									 FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
									 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
									 DEFAULT_PITCH | FF_DONTCARE, L"Arial");
			if (!font)
			{
				SelectObject(dc, previous_bitmap);
				DeleteObject(dib);
				DeleteDC(dc);
				throw std::runtime_error("Unable to create the bitmap-font face");
			}
			HGDIOBJ previous_font = SelectObject(dc, font);
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, RGB(255, 255, 255));
			std::fill_n(static_cast<std::uint8_t *>(dib_pixels),
						static_cast<std::size_t>(width) * height * 4u, std::uint8_t{0});
			TEXTMETRICW metrics{};
			GetTextMetricsW(dc, &metrics);
			line_height = static_cast<float>((std::max)(metrics.tmHeight, static_cast<LONG>(pixel_height)));
			for (std::uint32_t index = 0; index < advances.size(); ++index)
			{
				const wchar_t character = static_cast<wchar_t>(index + 32u);
				const std::uint32_t x = (index % kGlyphColumns) * cell_width;
				const std::uint32_t y = (index / kGlyphColumns) * cell_height;
				SIZE extent{};
				if (!GetTextExtentPoint32W(dc, &character, 1, &extent))
					extent.cx = static_cast<LONG>(pixel_height / 2u);
				advances[index] = static_cast<std::uint32_t>((std::max)(1L, extent.cx));
				TextOutW(dc, static_cast<int>(x + kGlyphPadding),
						 static_cast<int>(y + kGlyphPadding), &character, 1);
			}
			GdiFlush();
			const auto *bgra = static_cast<const std::uint8_t *>(dib_pixels);
			for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width) * height; ++pixel)
			{
				const std::uint8_t intensity = (std::max)({bgra[pixel * 4], bgra[pixel * 4 + 1],
														   bgra[pixel * 4 + 2]});
				rgba[pixel * 4] = 255;
				rgba[pixel * 4 + 1] = 255;
				rgba[pixel * 4 + 2] = 255;
				rgba[pixel * 4 + 3] = intensity;
			}
			SelectObject(dc, previous_font);
			SelectObject(dc, previous_bitmap);
			DeleteObject(font);
			DeleteObject(dib);
			DeleteDC(dc);
			return rgba;
		}
	}

	/// Owns the rasterized glyph table and its borrowed sprite texture registration.
	class TextRenderer::Impl
	{
	public:
		/// Describes one glyph's pixel rectangle and horizontal advance.
		struct Glyph
		{
			std::uint32_t x = 0;
			std::uint32_t y = 0;
			std::uint32_t width = 0;
			std::uint32_t height = 0;
			float advance = 0.0f;
		};

		/// Stores the renderer that must outlive this font registration.
		explicit Impl(SpriteBatchRenderer &renderer) : renderer_(renderer) {}
		/// Removes the borrowed texture registration before releasing its GPU storage.
		~Impl()
		{
			if (texture_handle_)
				renderer_.UnregisterTexture(texture_handle_);
		}

		SpriteBatchRenderer &renderer_;
		Rendering::Texture texture_;
		TextureHandle texture_handle_ = 0;
		std::array<Glyph, 95> glyphs_{};
		std::uint32_t atlas_width_ = 0;
		std::uint32_t atlas_height_ = 0;
		float line_height_ = 0.0f;
	};

	/// Builds an ASCII glyph atlas and registers its texture for sprite rendering.
	TextRenderer::TextRenderer(Rendering::RenderingDevice &device, SpriteBatchRenderer &renderer,
							   std::uint32_t pixel_height)
		: impl_(std::make_unique<Impl>(renderer))
	{
		if (!pixel_height || pixel_height > 128)
			throw std::invalid_argument("Bitmap font height must be between 1 and 128 pixels");
		std::array<std::uint32_t, 95> advances{};
		auto &font = *impl_;
		std::uint32_t cell_width = 0;
		std::uint32_t cell_height = 0;
		auto pixels = BuildFontAtlas(pixel_height, advances, font.line_height_, cell_width, cell_height);
		font.atlas_width_ = kGlyphColumns * cell_width;
		font.atlas_height_ = kGlyphRows * cell_height;
		for (std::uint32_t index = 0; index < font.glyphs_.size(); ++index)
		{
			const auto width = (std::min)(cell_width, advances[index] + kGlyphPadding * 2u);
			const auto height = (std::min)(cell_height, static_cast<std::uint32_t>(std::ceil(font.line_height_)) + kGlyphPadding * 2u);
			font.glyphs_[index] = {(index % kGlyphColumns) * cell_width,
								   (index / kGlyphColumns) * cell_height,
								   width, height, static_cast<float>(advances[index])};
		}
		Rendering::TextureDesc description;
		description.width = font.atlas_width_;
		description.height = font.atlas_height_;
		description.format = Rendering::ResourceFormat::RGBA8Unorm;
		description.initial_data = pixels.data();
		description.initial_data_size = pixels.size();
		description.minification_filter = Rendering::TextureFilter::Linear;
		description.magnification_filter = Rendering::TextureFilter::Linear;
		font.texture_ = device.CreateTexture(description);
		font.texture_handle_ = renderer.RegisterTexture(font.texture_);
	}

	/// Unregisters the font before destroying its borrowed texture resource.
	TextRenderer::~TextRenderer() = default;

	/// Emits one sprite per visible ASCII glyph and advances the pen for every byte.
	void TextRenderer::Draw(SpriteBatch &batch, std::string_view text, Math::Vector2 position,
							float scale, Math::Color color) const
	{
		if (scale <= 0.0f)
			return;
		const auto &font = *impl_;
		const float initial_x = position.x;
		for (const unsigned char character : text)
		{
			if (character == '\n')
			{
				position.x = initial_x;
				position.y += font.line_height_ * scale;
				continue;
			}
			const std::size_t index = character >= 32 && character <= 126 ? character - 32u : '?' - 32u;
			const Impl::Glyph &glyph = font.glyphs_[index];
			if (character != ' ')
			{
				SpriteCommand command;
				command.texture = font.texture_handle_;
				command.position = {position.x - kGlyphPadding * scale,
									position.y - kGlyphPadding * scale};
				command.size = {static_cast<float>(glyph.width), static_cast<float>(glyph.height)};
				command.scale = {scale, scale};
				command.texture_origin = {static_cast<float>(glyph.x) / font.atlas_width_,
										  static_cast<float>(glyph.y) / font.atlas_height_};
				command.texture_size = {static_cast<float>(glyph.width) / font.atlas_width_,
										static_cast<float>(glyph.height) / font.atlas_height_};
				command.color = color;
				batch.Draw(command);
			}
			position.x += glyph.advance * scale;
		}
	}

	/// Returns the unscaled bitmap-font line height.
	float TextRenderer::GetLineHeight() const noexcept { return impl_->line_height_; }
}
