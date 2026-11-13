#include "engine/Game.h"
#include "engine/Content.h"
#include "engine/Ecs.h"
#include "engine/SpriteBatch.h"
#include "engine/FrameStats.h"

namespace BlackLight::Engine
{
	/// Owns services that desktop-only applications do not allocate.
	class Game::Impl
	{
	public:
		/// Creates the game clock and rooted content cache.
		explicit Impl(const GameConfig &config)
			: clock(config.fixed_step_seconds, config.maximum_delta_seconds), content(config.content_root) {}
		FrameClock clock;
		World world;
		ContentManager content;
		SpriteBatch sprites;
		FrameStats stats;
	};

	/// Builds the desktop host and the additional game runtime.
	Game::Game(GameConfig config)
		: Window(config), impl_(std::make_unique<Impl>(config)) {}
	/// Destroys game services while the desktop backend still exists.
	Game::~Game() = default;
	/// Returns the most recently sampled game time.
	const GameTime &Game::GetGameTime() const noexcept { return impl_->clock.GetGameTime(); }
	/// Returns the game entity registry.
	World &Game::GetWorld() noexcept { return impl_->world; }
	/// Returns the game content cache.
	ContentManager &Game::GetContent() noexcept { return impl_->content; }
	/// Returns the game sprite collector.
	SpriteBatch &Game::GetSpriteBatch() noexcept { return impl_->sprites; }
	/// Returns the game frame statistics.
	FrameStats &Game::GetFrameStats() noexcept { return impl_->stats; }
	/// Provides optional game initialization.
	bool Game::OnInitialize() { return true; }
	/// Provides optional variable-rate gameplay.
	void Game::OnUpdate(const GameTime & /*game_time*/) {}
	/// Provides optional fixed-rate gameplay.
	void Game::OnFixedUpdate(const GameTime & /*game_time*/) {}
	/// Resets elapsed time for every successful runtime initialization.
	void Game::OnStarting() { impl_->clock.Reset(); }
	/// Starts the CPU frame and clears frame-scoped rendering counters.
	void Game::OnFrameTimingStarted() { impl_->stats.BeginFrame(); }
	/// Ends CPU timing before presentation waits for the display.
	void Game::OnFrameTimingFinished() { impl_->stats.EndFrame(); }
	/// Samples time and dispatches variable and fixed gameplay updates.
	void Game::OnUpdate()
	{
		impl_->clock.Tick();
		OnUpdate(impl_->clock.GetGameTime());
		while (impl_->clock.ConsumeFixedStep())
			OnFixedUpdate(impl_->clock.GetGameTime());
	}
	/// Dispatches game rendering before the host renders UI.
	void Game::OnRender()
	{
		double gpu_milliseconds = 0.0;
		if (auto *device = GetRenderingDevice();
			device && device->TryGetGpuTimeMilliseconds(gpu_milliseconds))
			impl_->stats.SetGpuFrameMilliseconds(gpu_milliseconds);
		OnRender(impl_->clock.GetGameTime());
	}
}
