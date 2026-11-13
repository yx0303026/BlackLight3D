#include "engine/SpriteAtlas.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace BlackLight::Engine
{
	/// Owns named regions and source texture dimensions.
	class SpriteAtlas::Impl
	{
	public:
		TextureHandle texture = 0;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::unordered_map<std::string, SpriteRegion> regions;
	};

	/// Owns one independent animation sequence and its playback cursor.
	class SpriteAnimation::Impl
	{
	public:
		std::vector<SpriteRegion> frames;
		double seconds_per_frame = 0.0;
		double elapsed = 0.0;
		std::size_t frame_index = 0;
		bool looping = true;
		bool finished = false;
		bool playing = true;
	};

	/// Owns decoded inputs for a shelf packing job.
	class SpriteAtlasBuilder::Impl
	{
	public:
		/// Associates a unique region name with its decoded pixels.
		struct Entry
		{
			std::string name;
			DecodedImage image;
		};
		std::vector<Entry> entries;
	};

	/// Copies atlas identity and UV coordinates into a draw command.
	void SpriteRegion::Apply(SpriteCommand &command) const noexcept
	{
		command.texture = texture;
		command.texture_origin = uv_origin;
		command.texture_size = uv_size;
		if (command.size.x == 1.0f && command.size.y == 1.0f)
			command.size = pixel_size;
	}

	/// Validates and stores the texture's pixel dimensions.
	SpriteAtlas::SpriteAtlas(TextureHandle texture, std::uint32_t width, std::uint32_t height)
		: impl_(std::make_unique<Impl>())
	{
		if (!width || !height)
			throw std::invalid_argument("Sprite atlas dimensions must be non-zero");
		impl_->texture = texture;
		impl_->width = width;
		impl_->height = height;
	}
	/// Releases atlas metadata.
	SpriteAtlas::~SpriteAtlas() = default;
	/// Deep-copies named regions.
	SpriteAtlas::SpriteAtlas(const SpriteAtlas &other) : impl_(std::make_unique<Impl>(*other.impl_)) {}
	/// Deep-copies named regions while preserving the destination on allocation failure.
	SpriteAtlas &SpriteAtlas::operator=(const SpriteAtlas &other)
	{
		if (this != &other)
			impl_ = std::make_unique<Impl>(*other.impl_);
		return *this;
	}
	/// Transfers private atlas storage.
	SpriteAtlas::SpriteAtlas(SpriteAtlas &&other) noexcept = default;
	/// Transfers private atlas storage.
	SpriteAtlas &SpriteAtlas::operator=(SpriteAtlas &&other) noexcept = default;

	/// Adds a pixel rectangle after checking it remains inside the atlas.
	void SpriteAtlas::AddRegion(std::string name, std::uint32_t x, std::uint32_t y,
								std::uint32_t width, std::uint32_t height)
	{
		if (name.empty() || !width || !height || x > impl_->width || y > impl_->height ||
			width > impl_->width - x || height > impl_->height - y)
			throw std::invalid_argument("Sprite atlas region is empty or outside its texture");
		SpriteRegion region;
		region.texture = impl_->texture;
		region.uv_origin = {static_cast<float>(x) / impl_->width, static_cast<float>(y) / impl_->height};
		region.uv_size = {static_cast<float>(width) / impl_->width, static_cast<float>(height) / impl_->height};
		region.pixel_size = {static_cast<float>(width), static_cast<float>(height)};
		impl_->regions[std::move(name)] = region;
	}

	/// Resolves a region by its stable name.
	const SpriteRegion &SpriteAtlas::GetRegion(const std::string &name) const
	{
		const auto found = impl_->regions.find(name);
		if (found == impl_->regions.end())
			throw std::out_of_range("Sprite atlas region was not found: " + name);
		return found->second;
	}

	/// Validates frame data and the playback interval.
	SpriteAnimation::SpriteAnimation(std::vector<SpriteRegion> frames, double seconds_per_frame, bool looping)
		: impl_(std::make_unique<Impl>())
	{
		if (frames.empty() || !std::isfinite(seconds_per_frame) || seconds_per_frame <= 0.0 ||
			!std::isfinite(seconds_per_frame * static_cast<double>(frames.size())))
			throw std::invalid_argument("Sprite animation requires frames and a positive frame duration");
		impl_->frames = std::move(frames);
		impl_->seconds_per_frame = seconds_per_frame;
		impl_->looping = looping;
	}
	/// Releases playback state.
	SpriteAnimation::~SpriteAnimation() = default;
	/// Deep-copies the frame sequence and active playback state.
	SpriteAnimation::SpriteAnimation(const SpriteAnimation &other)
		: impl_(std::make_unique<Impl>(*other.impl_)) {}
	/// Deep-copies the frame sequence and active playback state.
	SpriteAnimation &SpriteAnimation::operator=(const SpriteAnimation &other)
	{
		if (this != &other)
			impl_ = std::make_unique<Impl>(*other.impl_);
		return *this;
	}
	/// Transfers private playback state.
	SpriteAnimation::SpriteAnimation(SpriteAnimation &&other) noexcept = default;
	/// Transfers private playback state.
	SpriteAnimation &SpriteAnimation::operator=(SpriteAnimation &&other) noexcept = default;

	/// Advances frames while preserving leftover time across frame boundaries.
	void SpriteAnimation::Update(double elapsed_seconds) noexcept
	{
		auto &animation = *impl_;
		if (!animation.playing || !std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0)
			return;
		const double cycle = animation.seconds_per_frame * static_cast<double>(animation.frames.size());
		if (animation.looping)
			elapsed_seconds = std::fmod(elapsed_seconds, cycle);
		else if (elapsed_seconds >= cycle - animation.elapsed)
		{
			animation.frame_index = animation.frames.size() - 1;
			animation.elapsed = 0.0;
			animation.finished = true;
			animation.playing = false;
			return;
		}
		animation.elapsed += elapsed_seconds;
		const auto advance = static_cast<std::size_t>(animation.elapsed / animation.seconds_per_frame);
		animation.elapsed = std::fmod(animation.elapsed, animation.seconds_per_frame);
		const auto remaining = animation.frames.size() - animation.frame_index;
		if (!animation.looping && advance >= remaining)
		{
			animation.frame_index = animation.frames.size() - 1;
			animation.elapsed = 0.0;
			animation.finished = true;
			animation.playing = false;
		}
		else if (advance >= remaining)
			animation.frame_index = advance - remaining;
		else
			animation.frame_index += advance;
	}

	/// Returns the active frame.
	const SpriteRegion &SpriteAnimation::GetCurrentFrame() const { return impl_->frames.at(impl_->frame_index); }
	/// Resets the playback cursor.
	void SpriteAnimation::Reset() noexcept
	{
		impl_->elapsed = 0.0;
		impl_->frame_index = 0;
		impl_->finished = false;
		impl_->playing = true;
	}
	/// Resumes paused playback or restarts a finished sequence.
	void SpriteAnimation::Play() noexcept
	{
		if (impl_->finished)
			Reset();
		impl_->playing = true;
	}
	/// Keeps the active frame while suppressing Update advancement.
	void SpriteAnimation::Pause() noexcept { impl_->playing = false; }
	/// Reports whether Update advances the cursor.
	bool SpriteAnimation::IsPlaying() const noexcept { return impl_->playing; }
	/// Changes the wrap policy without changing the active frame.
	void SpriteAnimation::SetLooping(bool looping) noexcept { impl_->looping = looping; }
	/// Reports whether a non-looping animation has stopped at its last frame.
	bool SpriteAnimation::IsFinished() const noexcept { return impl_->finished; }

	/// Creates an empty packing job.
	SpriteAtlasBuilder::SpriteAtlasBuilder() : impl_(std::make_unique<Impl>()) {}
	/// Releases decoded source images.
	SpriteAtlasBuilder::~SpriteAtlasBuilder() = default;
	/// Deep-copies decoded source images.
	SpriteAtlasBuilder::SpriteAtlasBuilder(const SpriteAtlasBuilder &other)
		: impl_(std::make_unique<Impl>(*other.impl_)) {}
	/// Deep-copies decoded source images.
	SpriteAtlasBuilder &SpriteAtlasBuilder::operator=(const SpriteAtlasBuilder &other)
	{
		if (this != &other)
			impl_ = std::make_unique<Impl>(*other.impl_);
		return *this;
	}
	/// Transfers decoded source images.
	SpriteAtlasBuilder::SpriteAtlasBuilder(SpriteAtlasBuilder &&other) noexcept = default;
	/// Transfers decoded source images.
	SpriteAtlasBuilder &SpriteAtlasBuilder::operator=(SpriteAtlasBuilder &&other) noexcept = default;

	/// Converts packed pixel rectangles to normalized atlas regions.
	SpriteAtlas PackedSpriteAtlas::CreateAtlas(TextureHandle texture) const
	{
		SpriteAtlas atlas(texture, image.width, image.height);
		for (const auto &entry : regions)
			atlas.AddRegion(entry.first, entry.second[0], entry.second[1],
							entry.second[2], entry.second[3]);
		return atlas;
	}

	/// Stores one uniquely named image for the next pack operation.
	void SpriteAtlasBuilder::AddImage(std::string name, DecodedImage image)
	{
		if (name.empty() || !image.width || !image.height ||
			image.width > std::numeric_limits<std::size_t>::max() / image.height / 4u ||
			image.rgba.size() != static_cast<std::size_t>(image.width) * image.height * 4u)
			throw std::invalid_argument("Atlas images require a name and complete RGBA8 pixels");
		if (std::any_of(impl_->entries.begin(), impl_->entries.end(),
						[&](const Impl::Entry &entry)
						{ return entry.name == name; }))
			throw std::invalid_argument("Sprite atlas image names must be unique");
		impl_->entries.push_back({std::move(name), std::move(image)});
	}

	/// Packs images into power-of-two shelves with transparent gutters.
	PackedSpriteAtlas SpriteAtlasBuilder::Build(std::uint32_t maximum_width,
												std::uint32_t padding) const
	{
		if (impl_->entries.empty() || !maximum_width ||
			padding > std::numeric_limits<std::uint32_t>::max() / 2u)
			throw std::invalid_argument("Sprite atlas requires at least one image and a non-zero width");
		std::vector<const Impl::Entry *> ordered;
		ordered.reserve(impl_->entries.size());
		std::uint32_t preferred_width = 1;
		std::uint64_t total_area = 0;
		for (const auto &entry : impl_->entries)
		{
			if (entry.image.width > maximum_width ||
				entry.image.width > std::numeric_limits<std::uint32_t>::max() - padding * 2u ||
				entry.image.height > std::numeric_limits<std::uint32_t>::max() - padding * 2u)
				throw std::length_error("Sprite image exceeds the configured atlas dimensions");
			preferred_width = (std::max)(preferred_width, entry.image.width + padding * 2u);
			const auto area = static_cast<std::uint64_t>(entry.image.width + padding * 2u) *
							  (entry.image.height + padding * 2u);
			if (area > std::numeric_limits<std::uint64_t>::max() - total_area)
				throw std::length_error("Sprite atlas area overflow");
			total_area += area;
			ordered.push_back(&entry);
		}
		std::stable_sort(ordered.begin(), ordered.end(), [](const Impl::Entry *left, const Impl::Entry *right)
						 { return left->image.height > right->image.height; });
		std::uint32_t atlas_width = 1;
		while (atlas_width < maximum_width &&
			   (atlas_width < preferred_width || static_cast<std::uint64_t>(atlas_width) * atlas_width < total_area))
			atlas_width = atlas_width > maximum_width / 2u ? maximum_width : atlas_width * 2u;

		PackedSpriteAtlas result;
		std::uint32_t cursor_x = 0;
		std::uint32_t cursor_y = 0;
		std::uint32_t row_height = 0;
		for (const Impl::Entry *entry : ordered)
		{
			const std::uint32_t padded_width = entry->image.width + padding * 2u;
			const std::uint32_t padded_height = entry->image.height + padding * 2u;
			if (padded_width > atlas_width)
				throw std::length_error("Sprite image does not fit the configured atlas width");
			if (padded_width > atlas_width - cursor_x)
			{
				cursor_x = 0;
				cursor_y += row_height;
				row_height = 0;
			}
			if (cursor_y > std::numeric_limits<std::uint32_t>::max() - padded_height)
				throw std::length_error("Sprite atlas height overflow");
			result.regions.emplace(entry->name, std::array<std::uint32_t, 4>{
													cursor_x + padding, cursor_y + padding, entry->image.width, entry->image.height});
			cursor_x += padded_width;
			row_height = (std::max)(row_height, padded_height);
		}
		const std::uint32_t raw_height = cursor_y + row_height;
		std::uint32_t atlas_height = 1;
		while (atlas_height < raw_height && atlas_height <= (std::numeric_limits<std::uint32_t>::max() >> 1u))
			atlas_height <<= 1u;
		if (atlas_height < raw_height)
			throw std::length_error("Sprite atlas power-of-two height overflow");
		result.image.width = atlas_width;
		result.image.height = atlas_height;
		if (result.image.width > std::numeric_limits<std::size_t>::max() / atlas_height / 4u)
			throw std::length_error("Sprite atlas allocation exceeds addressable memory");
		result.image.rgba.assign(static_cast<std::size_t>(result.image.width) * atlas_height * 4u, 0);
		for (const Impl::Entry *entry : ordered)
		{
			const auto &rectangle = result.regions.at(entry->name);
			for (std::uint32_t row = 0; row < entry->image.height; ++row)
			{
				const std::size_t source_offset = static_cast<std::size_t>(row) * entry->image.width * 4u;
				const std::size_t destination_offset =
					(static_cast<std::size_t>(rectangle[1] + row) * result.image.width + rectangle[0]) * 4u;
				std::copy_n(entry->image.rgba.data() + source_offset,
							static_cast<std::size_t>(entry->image.width) * 4u,
							result.image.rgba.data() + destination_offset);
			}
		}
		return result;
	}
}
