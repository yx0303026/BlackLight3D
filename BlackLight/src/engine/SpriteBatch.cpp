#include "engine/SpriteBatch.h"

#include <algorithm>

namespace BlackLight::Engine
{

	void SpriteBatch::SetSubmitCallback(SubmitCallback callback)
	{
		submit_callback_ = std::move(callback);
	}

	void SpriteBatch::Begin(SpriteSortMode sort_mode)
	{
		if (begun_)
		{
			End();
		}
		sort_mode_ = sort_mode;
		begun_ = true;
		commands_.clear();
	}

	void SpriteBatch::Draw(const SpriteCommand &command)
	{
		if (!begun_)
		{
			return;
		}
		if (sort_mode_ == SpriteSortMode::Immediate)
		{
			if (submit_callback_)
			{
				submit_callback_(std::vector<SpriteCommand>{command});
			}
			return;
		}
		commands_.push_back(command);
	}

	void SpriteBatch::End()
	{
		if (!begun_)
		{
			return;
		}
		SortCommands();
		if (submit_callback_ && !commands_.empty())
		{
			submit_callback_(commands_);
		}
		commands_.clear();
		begun_ = false;
	}

	void SpriteBatch::Clear()
	{
		commands_.clear();
		begun_ = false;
	}

	std::size_t SpriteBatch::GetCommandCount() const noexcept
	{
		return commands_.size();
	}

	bool SpriteBatch::IsBegun() const noexcept
	{
		return begun_;
	}

	void SpriteBatch::SortCommands()
	{
		switch (sort_mode_)
		{
		case SpriteSortMode::Texture:
			std::stable_sort(commands_.begin(), commands_.end(),
							 [](const SpriteCommand &left, const SpriteCommand &right)
							 {
								 return left.texture < right.texture;
							 });
			break;
		case SpriteSortMode::BackToFront:
			std::stable_sort(commands_.begin(), commands_.end(),
							 [](const SpriteCommand &left, const SpriteCommand &right)
							 {
								 return left.depth > right.depth;
							 });
			break;
		case SpriteSortMode::FrontToBack:
			std::stable_sort(commands_.begin(), commands_.end(),
							 [](const SpriteCommand &left, const SpriteCommand &right)
							 {
								 return left.depth < right.depth;
							 });
			break;
		case SpriteSortMode::Deferred:
		case SpriteSortMode::Immediate:
			break;
		}
	}

} // namespace BlackLight::Engine
