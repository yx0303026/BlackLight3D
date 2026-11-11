#pragma once

#include "../BlackLightApi.h"
#include "Math.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace BlackLight::Engine {

/// Selects how deferred sprite commands are ordered before submission.
enum class SpriteSortMode {
	Deferred,
	Texture,
	BackToFront,
	FrontToBack,
	Immediate
};

/// Identifies a texture owned by a graphics implementation.
using TextureHandle = std::uint64_t;

/// Describes one 2D sprite draw command.
struct SpriteCommand {
	TextureHandle texture = 0;
	Math::Vector2 position{};
	Math::Vector2 size{1.0f, 1.0f};
	Math::Vector2 origin{};
	Math::Vector2 scale{1.0f, 1.0f};
	Math::Color color{};
	float rotation = 0.0f;
	float depth = 0.0f;
	std::uint32_t layer = 0;
};

BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

/// Collects sprites and submits them in predictable batches through a host callback.
class BLACKLIGHT_API SpriteBatch final {
public:
	using SubmitCallback = std::function<void(const std::vector<SpriteCommand>&)>;

	/// Creates an empty sprite batch with no submission callback.
	SpriteBatch() = default;

	/// Sets the callback receiving the sorted command list at End.
	void SetSubmitCallback(SubmitCallback callback);

	/// Starts collecting sprite commands using the requested sort mode.
	void Begin(SpriteSortMode sort_mode = SpriteSortMode::Deferred);

	/// Appends one sprite command to the current batch.
	void Draw(const SpriteCommand& command);

	/// Sorts and submits the current command list.
	void End();

	/// Removes all queued commands without submitting them.
	void Clear();

	/// Returns the number of queued commands.
	std::size_t GetCommandCount() const noexcept;

	/// Returns whether the batch is currently collecting commands.
	bool IsBegun() const noexcept;

private:
	/// Sorts commands according to the mode selected by Begin.
	void SortCommands();

	SpriteSortMode sort_mode_ = SpriteSortMode::Deferred;
	bool begun_ = false;
	std::vector<SpriteCommand> commands_;
	SubmitCallback submit_callback_;
};

BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine
