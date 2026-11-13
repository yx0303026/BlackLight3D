#pragma once

#include "SpriteBatch.h"
#include "Image.h"

#include <array>
#include <cstdint>
#include <memory>
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
		BLACKLIGHT_API void Apply(SpriteCommand &command) const noexcept;
	};

	/// Maps named pixel rectangles to normalized regions of one texture.
	class BLACKLIGHT_API SpriteAtlas final
	{
	public:
		/// Creates atlas metadata for a registered texture.
		SpriteAtlas(TextureHandle texture, std::uint32_t width, std::uint32_t height);
		/// Releases private region metadata.
		~SpriteAtlas();
		/// Copies the named regions independently of the source atlas.
		SpriteAtlas(const SpriteAtlas &other);
		/// Replaces the named regions with a copy of another atlas.
		SpriteAtlas &operator=(const SpriteAtlas &other);
		/// Transfers the region metadata without copying.
		SpriteAtlas(SpriteAtlas &&other) noexcept;
		/// Transfers the region metadata without copying.
		SpriteAtlas &operator=(SpriteAtlas &&other) noexcept;
		/// Adds or replaces a named pixel rectangle.
		void AddRegion(std::string name, std::uint32_t x, std::uint32_t y,
					   std::uint32_t width, std::uint32_t height);
		/// Returns a named region or throws when it is missing.
		const SpriteRegion &GetRegion(const std::string &name) const;

	private:
		/// Keeps region lookup storage outside the public ABI.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	/// Plays a sequence of atlas regions at a fixed frame duration.
	class BLACKLIGHT_API SpriteAnimation final
	{
	public:
		/// Creates an animation from frame regions and a positive duration.
		SpriteAnimation(std::vector<SpriteRegion> frames, double seconds_per_frame, bool looping = true);
		/// Releases private playback state.
		~SpriteAnimation();
		/// Copies the frame sequence and playback cursor.
		SpriteAnimation(const SpriteAnimation &other);
		/// Replaces the frame sequence and playback cursor with a copy.
		SpriteAnimation &operator=(const SpriteAnimation &other);
		/// Transfers the frame sequence and playback cursor.
		SpriteAnimation(SpriteAnimation &&other) noexcept;
		/// Transfers the frame sequence and playback cursor.
		SpriteAnimation &operator=(SpriteAnimation &&other) noexcept;
		/// Advances the active frame by elapsed time.
		void Update(double elapsed_seconds) noexcept;
		/// Returns the currently selected frame.
		const SpriteRegion &GetCurrentFrame() const;
		/// Restarts playback at the first frame.
		void Reset() noexcept;
		/// Resumes playback, restarting an animation that has already finished.
		void Play() noexcept;
		/// Suspends playback without changing the active frame.
		void Pause() noexcept;
		/// Returns whether Update currently advances playback.
		bool IsPlaying() const noexcept;
		/// Changes whether playback wraps after the final frame.
		void SetLooping(bool looping) noexcept;
		/// Reports whether playback reached its final frame.
		bool IsFinished() const noexcept;

	private:
		/// Keeps frame storage and playback controls outside the public ABI.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	/// Contains a packed RGBA8 texture image and its named pixel rectangles.
	struct PackedSpriteAtlas
	{
		DecodedImage image;
		std::unordered_map<std::string, std::array<std::uint32_t, 4>> regions;

		/// Creates sprite UV metadata after the packed image has been uploaded.
		BLACKLIGHT_API SpriteAtlas CreateAtlas(TextureHandle texture) const;
	};

	/// Packs named images into rows using a height-first shelf heuristic.
	class BLACKLIGHT_API SpriteAtlasBuilder final
	{
	public:
		/// Creates an empty atlas packing job.
		SpriteAtlasBuilder();
		/// Releases decoded source images.
		~SpriteAtlasBuilder();
		/// Copies source images into an independent packing job.
		SpriteAtlasBuilder(const SpriteAtlasBuilder &other);
		/// Replaces source images with an independent copy.
		SpriteAtlasBuilder &operator=(const SpriteAtlasBuilder &other);
		/// Transfers source images without copying pixels.
		SpriteAtlasBuilder(SpriteAtlasBuilder &&other) noexcept;
		/// Transfers source images without copying pixels.
		SpriteAtlasBuilder &operator=(SpriteAtlasBuilder &&other) noexcept;
		/// Adds a decoded image under a unique atlas name.
		void AddImage(std::string name, DecodedImage image);
		/// Packs every image into a bounded RGBA atlas with transparent padding.
		PackedSpriteAtlas Build(std::uint32_t maximum_width = 1024,
								std::uint32_t padding = 1) const;

	private:
		/// Keeps packing entries and algorithms outside the public ABI.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};
}
