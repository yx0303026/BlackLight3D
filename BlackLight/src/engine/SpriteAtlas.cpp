#include "engine/SpriteAtlas.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace BlackLight::Engine
{
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
		: texture_(texture), width_(width), height_(height)
	{
		if (!width_ || !height_)
			throw std::invalid_argument("Sprite atlas dimensions must be non-zero");
	}

	/// Adds a pixel rectangle after checking it remains inside the atlas.
	void SpriteAtlas::AddRegion(std::string name, std::uint32_t x, std::uint32_t y,
								std::uint32_t width, std::uint32_t height)
	{
		if (name.empty() || !width || !height || x > width_ || y > height_ ||
			width > width_ - x || height > height_ - y)
			throw std::invalid_argument("Sprite atlas region is empty or outside its texture");
		SpriteRegion region;
		region.texture = texture_;
		region.uv_origin = {static_cast<float>(x) / width_, static_cast<float>(y) / height_};
		region.uv_size = {static_cast<float>(width) / width_, static_cast<float>(height) / height_};
		region.pixel_size = {static_cast<float>(width), static_cast<float>(height)};
		regions_[std::move(name)] = region;
	}

	/// Resolves a region by its stable name.
	const SpriteRegion &SpriteAtlas::GetRegion(const std::string &name) const
	{
		const auto found = regions_.find(name);
		if (found == regions_.end())
			throw std::out_of_range("Sprite atlas region was not found: " + name);
		return found->second;
	}

	/// Validates frame data and the playback interval.
	SpriteAnimation::SpriteAnimation(std::vector<SpriteRegion> frames, double seconds_per_frame, bool looping)
		: frames_(std::move(frames)), seconds_per_frame_(seconds_per_frame), looping_(looping)
	{
		if (frames_.empty() || seconds_per_frame_ <= 0.0)
			throw std::invalid_argument("Sprite animation requires frames and a positive frame duration");
	}

	/// Advances frames while preserving leftover time across frame boundaries.
	void SpriteAnimation::Update(double elapsed_seconds) noexcept
	{
		if (finished_ || elapsed_seconds <= 0.0)
			return;
		elapsed_ += elapsed_seconds;
		while (elapsed_ >= seconds_per_frame_)
		{
			elapsed_ -= seconds_per_frame_;
			if (frame_index_ + 1 < frames_.size())
				++frame_index_;
			else if (looping_)
				frame_index_ = 0;
			else
			{
				elapsed_ = 0.0;
				finished_ = true;
				break;
			}
		}
	}

	/// Returns the active frame.
	const SpriteRegion &SpriteAnimation::GetCurrentFrame() const { return frames_.at(frame_index_); }
	/// Resets the playback cursor.
	void SpriteAnimation::Reset() noexcept { elapsed_ = 0.0; frame_index_ = 0; finished_ = false; }
	/// Reports whether a non-looping animation has stopped at its last frame.
	bool SpriteAnimation::IsFinished() const noexcept { return finished_; }

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
		if (std::any_of(entries_.begin(), entries_.end(),
						[&](const Entry &entry) { return entry.name == name; }))
			throw std::invalid_argument("Sprite atlas image names must be unique");
		entries_.push_back({std::move(name), std::move(image)});
	}

	/// Packs images into power-of-two shelves with transparent gutters.
	PackedSpriteAtlas SpriteAtlasBuilder::Build(std::uint32_t maximum_width,
												std::uint32_t padding) const
	{
		if (entries_.empty() || !maximum_width ||
			padding > std::numeric_limits<std::uint32_t>::max() / 2u)
			throw std::invalid_argument("Sprite atlas requires at least one image and a non-zero width");
		std::vector<const Entry *> ordered;
		ordered.reserve(entries_.size());
		std::uint32_t preferred_width = 1;
		for (const auto &entry : entries_)
		{
			if (entry.image.width > maximum_width ||
				entry.image.width > std::numeric_limits<std::uint32_t>::max() - padding * 2u ||
				entry.image.height > std::numeric_limits<std::uint32_t>::max() - padding * 2u)
				throw std::length_error("Sprite image exceeds the configured atlas dimensions");
			preferred_width = (std::max)(preferred_width, entry.image.width + padding * 2u);
			ordered.push_back(&entry);
		}
		std::sort(ordered.begin(), ordered.end(), [](const Entry *left, const Entry *right)
		{
			return left->image.height > right->image.height;
		});
		std::uint32_t atlas_width = 1;
		while (atlas_width < preferred_width && atlas_width < maximum_width)
			atlas_width <<= 1u;
		atlas_width = (std::min)(atlas_width, maximum_width);

		PackedSpriteAtlas result;
		std::uint32_t cursor_x = padding;
		std::uint32_t cursor_y = padding;
		std::uint32_t row_height = 0;
		std::uint32_t used_width = 0;
		for (const Entry *entry : ordered)
		{
			const std::uint32_t padded_width = entry->image.width + padding * 2u;
			const std::uint32_t padded_height = entry->image.height + padding * 2u;
			if (padded_width > atlas_width)
				throw std::length_error("Sprite image does not fit the configured atlas width");
			if (cursor_x > atlas_width || padded_width > atlas_width - cursor_x)
			{
				cursor_x = padding;
				cursor_y += row_height;
				row_height = 0;
			}
			if (cursor_y > std::numeric_limits<std::uint32_t>::max() - padded_height)
				throw std::length_error("Sprite atlas height overflow");
			result.regions.emplace(entry->name, std::array<std::uint32_t, 4>{
				cursor_x, cursor_y, entry->image.width, entry->image.height});
			cursor_x += padded_width;
			row_height = (std::max)(row_height, padded_height);
			used_width = (std::max)(used_width, cursor_x);
		}
		const std::uint32_t raw_height = cursor_y + row_height;
		std::uint32_t atlas_height = 1;
		while (atlas_height < raw_height && atlas_height <= (std::numeric_limits<std::uint32_t>::max() >> 1u))
			atlas_height <<= 1u;
		result.image.width = (std::max)(1u, used_width);
		result.image.height = atlas_height;
		if (result.image.width > std::numeric_limits<std::size_t>::max() / atlas_height / 4u)
			throw std::length_error("Sprite atlas allocation exceeds addressable memory");
		result.image.rgba.assign(static_cast<std::size_t>(result.image.width) * atlas_height * 4u, 0);
		for (const Entry *entry : ordered)
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
