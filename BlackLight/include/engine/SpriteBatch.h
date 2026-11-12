#pragma once

#include "../BlackLightApi.h"
#include "Math.h"
#include "rendering_device/RenderingDevice.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace BlackLight::Engine
{
	class Camera2D;
	class FrameStats;

	/// Selects how deferred sprite commands are ordered before submission.
	enum class SpriteSortMode
	{
		Deferred,
		Texture,
		BackToFront,
		FrontToBack,
		Immediate
	};

	/// Identifies a texture owned by a graphics implementation.
	using TextureHandle = std::uint64_t;

	/// Describes one 2D sprite draw command.
	struct SpriteCommand
	{
		TextureHandle texture = 0;
		Math::Vector2 position{};
		Math::Vector2 size{1.0f, 1.0f};
		Math::Vector2 origin{};
		Math::Vector2 scale{1.0f, 1.0f};
		Math::Vector2 texture_origin{};
		Math::Vector2 texture_size{1.0f, 1.0f};
		Math::Color color{};
		float rotation = 0.0f;
		float depth = 0.0f;
		std::uint32_t layer = 0;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Collects sprites and submits them in predictable batches through a host callback.
	class BLACKLIGHT_API SpriteBatch final
	{
	public:
		using SubmitCallback = std::function<void(const std::vector<SpriteCommand> &)>;

		/// Creates an empty sprite batch with no submission callback.
		SpriteBatch() = default;

		/// Sets the callback receiving the sorted command list at End.
		void SetSubmitCallback(SubmitCallback callback);

		/// Starts collecting sprite commands using the requested sort mode.
		void Begin(SpriteSortMode sort_mode = SpriteSortMode::Deferred);

		/// Appends one sprite command to the current batch.
		void Draw(const SpriteCommand &command);

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

	/// Turns collected sprite commands into textured GPU draw submissions.
	class BLACKLIGHT_API SpriteBatchRenderer final
	{
	public:
		/// Creates the sprite pipeline, streaming vertex buffer, and one-pixel white texture.
		explicit SpriteBatchRenderer(Rendering::RenderingDevice &device,
									 FrameStats *stats = nullptr,
									 std::size_t maximum_sprites = 2048);
		/// Releases sprite GPU resources before the rendering device is shut down.
		~SpriteBatchRenderer();
		/// Prevents copying backend resources.
		SpriteBatchRenderer(const SpriteBatchRenderer &) = delete;
		/// Prevents assigning backend resources.
		SpriteBatchRenderer &operator=(const SpriteBatchRenderer &) = delete;
		/// Returns the texture handle reserved for untextured color sprites.
		static constexpr TextureHandle WhiteTextureHandle() noexcept { return 0; }
		/// Registers a borrowed texture under a stable nonzero sprite handle.
		TextureHandle RegisterTexture(const Rendering::Texture &texture);
		/// Replaces the borrowed texture associated with a sprite handle.
		void SetTexture(TextureHandle handle, const Rendering::Texture &texture);
		/// Removes a borrowed texture handle.
		void UnregisterTexture(TextureHandle handle) noexcept;
		/// Sets the camera used by subsequent submissions.
		void SetCamera(const Camera2D &camera) noexcept;
		/// Returns the renderer-owned opaque white texture.
		const Rendering::Texture &GetWhiteTexture() const noexcept;
		/// Renders a sorted list of sprites, batching adjacent equal textures.
		void Submit(const std::vector<SpriteCommand> &commands);

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine
