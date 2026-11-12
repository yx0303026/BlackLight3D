#pragma once

#include "SpriteBatch.h"
#include "Image.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace BlackLight::Engine
{
	/// Identifies one rectangular region in a texture atlas.
	struct SpriteRegion
	{
		TextureHandle texture = 0;
		Math::Vector2 uv_origin{};
		Math::Vector2 uv_size{1.0f, 1.0f};
		Math::Vector2 pixel_size{1.0f, 1.0f};

		/// Applies the region's texture and UV range to a sprite command.
		void Apply(SpriteCommand &command) const noexcept;
	};

	/// Maps named pixel rectangles to normalized regions of one texture.
	class SpriteAtlas final
	{
	public:
		/// Creates atlas metadata for a registered texture.
		SpriteAtlas(TextureHandle texture, std::uint32_t width, std::uint32_t height);
		/// Adds or replaces a named pixel rectangle.
		void AddRegion(std::string name, std::uint32_t x, std::uint32_t y,
					   std::uint32_t width, std::uint32_t height);
		/// Returns a named region or throws when it is missing.
		const SpriteRegion &GetRegion(const std::string &name) const;

	private:
		TextureHandle texture_;
		std::uint32_t width_;
		std::uint32_t height_;
		std::unordered_map<std::string, SpriteRegion> regions_;
	};

	/// Plays a sequence of atlas regions at a fixed frame duration.
	class SpriteAnimation final
	{
	public:
		/// Creates an animation from frame regions and a positive duration.
		SpriteAnimation(std::vector<SpriteRegion> frames, double seconds_per_frame, bool looping = true);
		/// Advances the active frame by elapsed time.
		void Update(double elapsed_seconds) noexcept;
		/// Returns the currently selected frame.
		const SpriteRegion &GetCurrentFrame() const;
		/// Restarts playback at the first frame.
		void Reset() noexcept;
		/// Reports whether playback reached its final frame.
		bool IsFinished() const noexcept;

	private:
		std::vector<SpriteRegion> frames_;
		double seconds_per_frame_;
		double elapsed_ = 0.0;
		std::size_t frame_index_ = 0;
		bool looping_;
		bool finished_ = false;
	};

	/// Contains a packed RGBA8 texture image and its named pixel rectangles.
	struct PackedSpriteAtlas
	{
		DecodedImage image;
		std::unordered_map<std::string, std::array<std::uint32_t, 4>> regions;

		/// Creates sprite UV metadata after the packed image has been uploaded.
		SpriteAtlas CreateAtlas(TextureHandle texture) const;
	};

	/// Packs named images into rows using a height-first shelf heuristic.
	class SpriteAtlasBuilder final
	{
	public:
		/// Adds a decoded image under a unique atlas name.
		void AddImage(std::string name, DecodedImage image);
		/// Packs every image into a bounded RGBA atlas with transparent padding.
		PackedSpriteAtlas Build(std::uint32_t maximum_width = 1024,
								std::uint32_t padding = 1) const;

	private:
		struct Entry
		{
			std::string name;
			DecodedImage image;
		};
		std::vector<Entry> entries_;
	};
}
