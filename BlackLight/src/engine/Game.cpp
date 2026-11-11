#include "engine/Game.h"

#include <utility>

namespace BlackLight::Engine
{

	Game::Game(GameConfig config)
		: config_(std::move(config)), clock_(config_.fixed_step_seconds, config_.maximum_delta_seconds), content_(config_.content_root)
	{
	}

	Game::~Game()
	{
		if (initialized_)
		{
			OnShutdown();
			if (graphics_backend_)
			{
				graphics_backend_->Shutdown();
			}
		}
	}

	GraphicsResult Game::Initialize()
	{
		if (initialized_)
		{
			return GraphicsResult::Success;
		}

		graphics_backend_ = CreateGraphicsBackend(config_.backend_name.c_str());
		if (!graphics_backend_)
		{
			return GraphicsResult::ContextCreationFailed;
		}

		graphics_backend_->SetInputSink(&input_);
		const GraphicsResult result = graphics_backend_->Initialize(config_.window);
		if (result != GraphicsResult::Success)
		{
			graphics_backend_.reset();
			return result;
		}

		clock_.Reset();
		exit_requested_ = false;
		if (!OnInitialize())
		{
			graphics_backend_->Shutdown();
			graphics_backend_.reset();
			return GraphicsResult::NotInitialized;
		}

		initialized_ = true;
		return GraphicsResult::Success;
	}

	int Game::Run()
	{
		if (!initialized_ && Initialize() != GraphicsResult::Success)
		{
			return 1;
		}

		while (!exit_requested_ && !graphics_backend_->ShouldClose())
		{
			if (!RunFrame())
			{
				break;
			}
		}

		OnShutdown();
		graphics_backend_->Shutdown();
		initialized_ = false;
		return 0;
	}

	void Game::Exit() noexcept
	{
		exit_requested_ = true;
	}

	const GameTime &Game::GetGameTime() const noexcept
	{
		return clock_.GetGameTime();
	}

	InputState &Game::GetInput() noexcept
	{
		return input_;
	}

	World &Game::GetWorld() noexcept
	{
		return world_;
	}

	ContentManager &Game::GetContent() noexcept
	{
		return content_;
	}

	IGraphicsBackend *Game::GetGraphicsBackend() noexcept
	{
		return graphics_backend_.get();
	}

	bool Game::OnInitialize()
	{
		return true;
	}

	void Game::OnUpdate(const GameTime & /*game_time*/) {}

	void Game::OnFixedUpdate(const GameTime & /*game_time*/) {}

	void Game::OnDraw(const GameTime & /*game_time*/) {}

	void Game::OnShutdown() {}

	bool Game::RunFrame()
	{
		input_.BeginFrame();
		if (!graphics_backend_->Update())
		{
			return false;
		}

		clock_.Tick();
		OnUpdate(clock_.GetGameTime());
		while (clock_.ConsumeFixedStep())
		{
			OnFixedUpdate(clock_.GetGameTime());
		}

		if (graphics_backend_->Render() != GraphicsResult::Success)
		{
			return false;
		}
		OnDraw(clock_.GetGameTime());
		return graphics_backend_->Present() == GraphicsResult::Success;
	}

} // namespace BlackLight::Engine
