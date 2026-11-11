#pragma once

#include "../BlackLightApi.h"
#include "Content.h"
#include "Ecs.h"
#include "IGraphicsBackend.h"
#include "Input.h"
#include "Time.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace BlackLight::Engine
{

	/// Configures the host-independent game loop.
	struct GameConfig
	{
		GraphicsWindow window{};
		std::string backend_name = "windows-opengl";
		double fixed_step_seconds = 1.0 / 60.0;
		double maximum_delta_seconds = 0.25;
		std::filesystem::path content_root = "Content";
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Owns engine services and drives initialization, update, draw, and shutdown.
	class BLACKLIGHT_API Game
	{
	public:
		/// Creates a game with the supplied runtime configuration.
		explicit Game(GameConfig config = {});

		/// Shuts down the game and releases its backend.
		virtual ~Game();

		Game(const Game &) = delete;
		Game &operator=(const Game &) = delete;
		Game(Game &&) = delete;
		Game &operator=(Game &&) = delete;

		/// Initializes the backend and invokes the game initialization hook.
		GraphicsResult Initialize();

		/// Runs the game loop until Exit is called or the window closes.
		int Run();

		/// Requests loop termination after the current frame.
		void Exit() noexcept;

		/// Returns the current game timing snapshot.
		const GameTime &GetGameTime() const noexcept;

		/// Returns the mutable input state.
		InputState &GetInput() noexcept;

		/// Returns the entity world.
		World &GetWorld() noexcept;

		/// Returns the content manager.
		ContentManager &GetContent() noexcept;

		/// Returns the active graphics backend.
		IGraphicsBackend *GetGraphicsBackend() noexcept;

	protected:
		/// Initializes game-owned content and entities after the backend is ready.
		virtual bool OnInitialize();

		/// Updates variable-rate gameplay once per rendered frame.
		virtual void OnUpdate(const GameTime &game_time);

		/// Updates deterministic gameplay at the configured fixed rate.
		virtual void OnFixedUpdate(const GameTime &game_time);

		/// Draws the current game state between backend Render and Present.
		virtual void OnDraw(const GameTime &game_time);

		/// Releases game-owned resources before the backend is shut down.
		virtual void OnShutdown();

	private:
		/// Runs one frame and returns false when the loop should terminate.
		bool RunFrame();

		GameConfig config_;
		FrameClock clock_;
		InputState input_;
		World world_;
		ContentManager content_;
		std::unique_ptr<IGraphicsBackend> graphics_backend_;
		bool initialized_ = false;
		bool exit_requested_ = false;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine
