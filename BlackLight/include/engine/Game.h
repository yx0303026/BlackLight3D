#pragma once

#include "../BlackLightApi.h"
#include "Window.h"
#include "Time.h"

#include <filesystem>
#include <memory>

namespace BlackLight::Engine
{
	class World;
	class ContentManager;
	class SpriteBatch;
	class FrameStats;

	/// Configures the host-independent game loop.
	struct GameConfig : WindowConfig
	{
		double fixed_step_seconds = 1.0 / 60.0;
		double maximum_delta_seconds = 0.25;
		std::filesystem::path content_root = "Content";
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Abstract game host adding timing, ECS, content, and sprites to the desktop host.
	class BLACKLIGHT_API Game : public Window
	{
	public:
		/// Creates a game with the supplied runtime configuration.
		explicit Game(GameConfig config = {});

		/// Releases game services while the base desktop context still exists.
		~Game() override;

		/// Prevents copying a game runtime.
		Game(const Game &) = delete;
		/// Prevents assigning a game runtime.
		Game &operator=(const Game &) = delete;
		/// Keeps game callback addresses stable.
		Game(Game &&) = delete;
		/// Keeps game callback addresses stable.
		Game &operator=(Game &&) = delete;

		/// Returns the current game timing snapshot.
		const GameTime &GetGameTime() const noexcept;

		/// Returns the entity world.
		World &GetWorld() noexcept;

		/// Returns the content manager.
		ContentManager &GetContent() noexcept;

		/// Returns the sprite collector; the renderer supplies its submission callback.
		SpriteBatch &GetSpriteBatch() noexcept;
		/// Returns frame timing and submission statistics.
		FrameStats &GetFrameStats() noexcept;

	protected:
		/// Initializes game-owned content and entities after the backend is ready.
		bool OnInitialize() override;

		/// Updates variable-rate gameplay once per rendered frame.
		virtual void OnUpdate(const GameTime &game_time);

		/// Updates deterministic gameplay at the configured fixed rate.
		virtual void OnFixedUpdate(const GameTime &game_time);

		/// Draws the game scene before the UI overlay; every game must implement it.
		virtual void OnRender(const GameTime &game_time) = 0;

	private:
		/// Resets game timing before application initialization.
		void OnStarting() final;
		/// Starts the game frame metrics before native event polling.
		void OnFrameTimingStarted() final;
		/// Finishes game frame metrics after presentation.
		void OnFrameTimingFinished() final;
		/// Samples time and dispatches variable and fixed gameplay updates.
		void OnUpdate() final;
		/// Dispatches rendering with the current timing snapshot.
		void OnRender() final;
		/// Stores game services outside the SDK header.
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine
