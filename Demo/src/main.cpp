#include "engine/Engine.h"
#include "rmlui/BuiLoader.h"
#include "rmlui/BuiTree.h"
#include "blacklight_demo_bui.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/EventListener.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace BlackLight;
using namespace BlackLight::Engine;

namespace
{
	/// Locates the executable directory for logs and loose content.
	std::filesystem::path GetExecutableDirectory()
	{
		std::vector<wchar_t> path(32768);
		const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (!length || length >= path.size())
			throw std::runtime_error("Failed to locate the Demo executable");
		return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
	}

	/// Converts a Windows path to the UTF-8 encoding used by RmlUi.
	std::string ToUtf8(const std::wstring &value)
	{
		const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(),
												 static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (required <= 0)
			throw std::runtime_error("Failed to convert a Windows font path to UTF-8");
		std::string result(static_cast<std::size_t>(required), '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
								result.data(), required, nullptr, nullptr) != required)
			throw std::runtime_error("Failed to convert a Windows font path to UTF-8");
		return result;
	}

	/// Registers regular and bold Arial faces from the active Windows installation.
	void LoadUiFonts()
	{
		std::vector<wchar_t> windows_directory(32768);
		const UINT length = GetWindowsDirectoryW(
			windows_directory.data(), static_cast<UINT>(windows_directory.size()));
		if (!length || length >= windows_directory.size())
			throw std::runtime_error("Failed to locate the Windows Fonts directory");
		const std::filesystem::path fonts =
			std::filesystem::path(std::wstring(windows_directory.data(), length)) / L"Fonts";
		const auto regular_path = ToUtf8((fonts / L"arial.ttf").wstring());
		const auto bold_path = ToUtf8((fonts / L"arialbd.ttf").wstring());
		if (!Rml::LoadFontFace(regular_path, "Arial", Rml::Style::FontStyle::Normal,
							   Rml::Style::FontWeight::Normal) ||
			!Rml::LoadFontFace(bold_path, "Arial", Rml::Style::FontStyle::Normal,
							   Rml::Style::FontWeight::Bold))
			throw std::runtime_error("Failed to load the regular and bold Arial UI fonts");
	}

	/// Adapts one RmlUi click target to a game action.
	class ActionListener final : public Rml::EventListener
	{
	public:
		/// Stores the callback invoked after the attached element is clicked.
		void SetCallback(std::function<void()> callback) { callback_ = std::move(callback); }
		/// Invokes the configured game action.
		void ProcessEvent(Rml::Event &event) override
		{
			if (event == "click" && callback_)
				callback_();
		}

	private:
		std::function<void()> callback_;
	};

	/// Stores one horizontal platform in world coordinates.
	struct Platform
	{
		float x;
		float y;
		float width;
	};

	/// Stores one animated collectible in world coordinates.
	struct Collectible
	{
		Math::Vector2 position;
		bool collected = false;
	};

	/// Stores a hazard rectangle in world coordinates.
	struct Hazard
	{
		float x;
		float y;
		float width;
		float height;
	};
}

/// Runs a compact platform-jumping game on the engine's normalized 2D pipeline.
class PlatformerGame final : public Game
{
public:
	/// Configures the OpenGL game surface and content root.
	PlatformerGame()
		: Game(CreateConfig())
	{
	}

protected:
	/// Loads game textures and UI, then creates a fresh playable level.
	bool OnInitialize() override
	{
		CreateGraphics();
		LoadUiFonts();

		UI::BuiLoadError load_error;
		if (!UI::BuiLoader::Parse(DemoUi::Find("Main.bui"), ui_document_, load_error))
			throw std::runtime_error("Unable to parse the embedded game UI: " + load_error.message);
		UI::BuiBuildError build_error;
		if (!UI::BuiTree::Build(*GetContext(), *ui_document_, ui_tree_, build_error))
			throw std::runtime_error("Unable to build the game UI: " + build_error.message);
		ui_tree_->Show();
		start_listener_.SetCallback([this]()
									{ StartGame(); });
		restart_listener_.SetCallback([this]()
									  { RestartGame(); });
		if (auto *element = ui_tree_->FindById("start_button"))
			element->AddEventListener("click", &start_listener_);
		if (auto *element = ui_tree_->FindById("restart_button"))
			element->AddEventListener("click", &restart_listener_);
		if (auto *element = ui_tree_->FindById("retry_button"))
			element->AddEventListener("click", &restart_listener_);
		if (auto *element = ui_tree_->FindById("play_again_button"))
			element->AddEventListener("click", &restart_listener_);
		ResetGame();
		return true;
	}

	/// Advances menu state and refreshes the interactive HUD.
	void OnUpdate(const GameTime &game_time) override
	{
		if (GetInput().WasKeyPressed(VK_ESCAPE) && started_ && !game_over_)
		{
			paused_ = !paused_;
			SetDisplay("pause_panel", paused_);
		}
		if (GetInput().WasKeyPressed('R') && (game_over_ || won_))
			RestartGame();
		player_moving_ = GetInput().IsKeyDown(VK_LEFT) || GetInput().IsKeyDown(VK_RIGHT) ||
						 GetInput().IsKeyDown('A') || GetInput().IsKeyDown('D');
		if (player_moving_)
			player_animation_->Update(game_time.elapsed_seconds);
		else
			player_animation_->Reset();
		coin_time_ += game_time.elapsed_seconds;
		UpdateUi();
	}

	/// Advances horizontal movement, jumping, collisions, and collectible state at fixed rate.
	void OnFixedUpdate(const GameTime &game_time) override
	{
		if (!started_ || paused_ || game_over_ || won_)
			return;
		const float delta = static_cast<float>(game_time.fixed_elapsed_seconds);
		float direction = 0.0f;
		if (GetInput().IsKeyDown(VK_LEFT) || GetInput().IsKeyDown('A'))
			direction -= 1.0f;
		if (GetInput().IsKeyDown(VK_RIGHT) || GetInput().IsKeyDown('D'))
			direction += 1.0f;
		velocity_.x = direction * kMoveSpeed;
		if (GetInput().WasKeyPressed(VK_SPACE) || GetInput().WasKeyPressed(VK_UP) ||
			GetInput().WasKeyPressed('W'))
		{
			if (grounded_)
			{
				velocity_.y = -kJumpSpeed;
				grounded_ = false;
			}
		}
		const float previous_y = player_position_.y;
		velocity_.y = (std::min)(velocity_.y + kGravity * delta, kMaximumFallSpeed);
		player_position_.x = std::clamp(player_position_.x + velocity_.x * delta,
										0.0f, kWorldWidth - kPlayerWidth);
		player_position_.y += velocity_.y * delta;
		grounded_ = false;
		for (const auto &platform : platforms_)
		{
			const float previous_bottom = previous_y + kPlayerHeight;
			const float current_bottom = player_position_.y + kPlayerHeight;
			const bool horizontal_overlap =
				player_position_.x + kPlayerWidth > platform.x &&
				player_position_.x < platform.x + platform.width;
			if (velocity_.y >= 0.0f && previous_bottom <= platform.y &&
				current_bottom >= platform.y && horizontal_overlap)
			{
				player_position_.y = platform.y - kPlayerHeight;
				velocity_.y = 0.0f;
				grounded_ = true;
			}
		}
		CollectCoins();
		if (TouchesHazard() || player_position_.y > kWorldHeight + 200.0f)
			LoseLife();
		if (player_position_.x > kWorldWidth - 100.0f && player_position_.y < 560.0f)
		{
			won_ = true;
			SetDisplay("win_panel", true);
		}
	}

	/// Renders the world, sprite animation, collectibles, and bitmap debug overlay.
	void OnRender(const GameTime &) override
	{
		auto *device = GetRenderingDevice();
		if (!device || !renderer_)
			return;
		const auto surface = device->GetSurfaceState();
		if (!surface.width || !surface.height)
			return;
		camera_.SetViewport(0.0f, 0.0f, static_cast<float>(surface.width),
							static_cast<float>(surface.height));
		camera_.SetPosition({std::clamp(player_position_.x + kPlayerWidth * 0.5f,
										static_cast<float>(surface.width) / camera_.GetZoom() * 0.5f,
										kWorldWidth - static_cast<float>(surface.width) / camera_.GetZoom() * 0.5f),
							 std::clamp(player_position_.y + kPlayerHeight * 0.5f - 90.0f,
										static_cast<float>(surface.height) / camera_.GetZoom() * 0.5f,
										kWorldHeight - static_cast<float>(surface.height) / camera_.GetZoom() * 0.5f)});
		renderer_->SetCamera(camera_);

		auto &batch = GetSpriteBatch();
		batch.Begin(SpriteSortMode::Deferred);
		DrawSolid({0.0f, 0.0f}, {kWorldWidth, kWorldHeight}, Math::Color::FromRgba8(122, 190, 204));
		DrawSolid({0.0f, 210.0f}, {kWorldWidth, 250.0f}, Math::Color::FromRgba8(91, 160, 166, 180));
		DrawSolid({0.0f, 350.0f}, {kWorldWidth, 300.0f}, Math::Color::FromRgba8(69, 136, 146, 180));
		for (const auto &platform : platforms_)
			DrawPlatform(platform);
		for (const auto &hazard : hazards_)
		{
			SpriteCommand command;
			atlas_->GetRegion("spikes").Apply(command);
			command.position = {hazard.x, hazard.y};
			command.size = {hazard.width, hazard.height};
			batch.Draw(command);
		}
		for (const auto &coin : coins_)
			if (!coin.collected)
				DrawRegion("coin", {coin.position.x, coin.position.y + static_cast<float>(std::sin(coin_time_ * 4.0) * 5.0)}, {40.0f, 40.0f});
		SpriteCommand player_command;
		player_animation_->GetCurrentFrame().Apply(player_command);
		player_command.position = player_position_;
		player_command.size = {kPlayerWidth, kPlayerHeight};
		batch.Draw(player_command);
		DrawSolid({kWorldWidth - 115.0f, 430.0f}, {12.0f, 130.0f},
				  Math::Color::FromRgba8(238, 219, 166));
		DrawRegion("checkpoint", {kWorldWidth - 150.0f, 430.0f}, {72.0f, 48.0f});
		if (text_renderer_)
		{
			const Math::Vector2 label_position = camera_.ScreenToWorld({20.0f, 28.0f});
			text_renderer_->Draw(batch, "BLACKLIGHT PLATFORMER", label_position, 0.8f,
								 Math::Color::FromRgba8(25, 48, 61));
		}
		batch.End();
	}

	/// Removes the UI tree and sprite resources before the rendering context shuts down.
	void OnShutdown() override
	{
		ui_tree_.reset();
		ReleaseGraphics();
	}

	/// Drops game GPU resources before the device replaces its native context.
	void OnDeviceLost() override
	{
		ReleaseGraphics();
	}

	/// Recreates the sprite pipeline and content textures on the recovered device.
	void OnDeviceRestored() override
	{
		CreateGraphics();
	}

private:
	static constexpr float kWorldWidth = 2460.0f;
	static constexpr float kWorldHeight = 1600.0f;
	static constexpr float kPlayerWidth = 40.0f;
	static constexpr float kPlayerHeight = 60.0f;
	static constexpr float kMoveSpeed = 300.0f;
	static constexpr float kJumpSpeed = 610.0f;
	static constexpr float kGravity = 1320.0f;
	static constexpr float kMaximumFallSpeed = 900.0f;

	/// Creates the streaming renderer, content texture, atlas, animation, and font.
	void CreateGraphics()
	{
		auto *device = GetRenderingDevice();
		if (!device)
			throw std::logic_error("Rendering device is unavailable");
		renderer_ = std::make_unique<SpriteBatchRenderer>(*device, &GetFrameStats(), 4096);
		player_texture_ = LoadTexture(GetContent(), *device, "platformer-atlas.png",
									  Rendering::TextureFilter::Nearest);
		atlas_texture_ = renderer_->RegisterTexture(player_texture_);
		atlas_ = std::make_unique<SpriteAtlas>(atlas_texture_, player_texture_.GetWidth(),
											   player_texture_.GetHeight());
		atlas_->AddRegion("idle", 0, 0, 16, 24);
		atlas_->AddRegion("run", 16, 0, 16, 24);
		atlas_->AddRegion("coin", 32, 8, 16, 16);
		atlas_->AddRegion("platform", 48, 0, 32, 16);
		atlas_->AddRegion("spikes", 80, 0, 16, 16);
		atlas_->AddRegion("checkpoint", 96, 0, 24, 16);
		player_animation_ = std::make_unique<SpriteAnimation>(
			std::vector<SpriteRegion>{atlas_->GetRegion("idle"), atlas_->GetRegion("run")},
			0.12, true);
		text_renderer_ = std::make_unique<TextRenderer>(*device, *renderer_, 18);
		camera_.SetZoom(2.0f);
		GetSpriteBatch().SetSubmitCallback(
			[this](const std::vector<SpriteCommand> &commands)
			{ renderer_->Submit(commands); });
	}

	/// Releases borrowed textures and the renderer before device loss or shutdown.
	void ReleaseGraphics()
	{
		GetSpriteBatch().SetSubmitCallback({});
		GetSpriteBatch().Clear();
		text_renderer_.reset();
		player_animation_.reset();
		atlas_.reset();
		if (renderer_ && atlas_texture_)
			renderer_->UnregisterTexture(atlas_texture_);
		atlas_texture_ = 0;
		player_texture_ = Rendering::Texture{};
		renderer_.reset();
	}

	/// Builds a 1280 by 720 DPI-aware OpenGL surface with the Demo content directory.
	static GameConfig CreateConfig()
	{
		GameConfig config;
		const UINT dpi = GetDpiForSystem();
		config.rendering.surface.title = "BlackLight Platformer";
		config.rendering.surface.width = static_cast<std::uint32_t>(
			MulDiv(1280, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI));
		config.rendering.surface.height = static_cast<std::uint32_t>(
			MulDiv(720, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI));
		config.rendering.backend = Rendering::RenderingBackend::OpenGL;
		config.rendering.enable_ui = true;
		config.content_root = GetExecutableDirectory() / L"Content";
		return config;
	}

	/// Rebuilds the level, collectibles, player state, and menu visibility.
	void ResetGame()
	{
		platforms_ = {
			{0.0f, 1420.0f, kWorldWidth},
			{225.0f, 1285.0f, 210.0f},
			{510.0f, 1155.0f, 190.0f},
			{800.0f, 1025.0f, 210.0f},
			{1105.0f, 895.0f, 195.0f},
			{1390.0f, 765.0f, 205.0f},
			{1690.0f, 635.0f, 205.0f},
			{1985.0f, 505.0f, 230.0f},
			{2240.0f, 410.0f, 220.0f}};
		hazards_ = {{850.0f, 993.0f, 45.0f, 32.0f},
					{1450.0f, 733.0f, 45.0f, 32.0f},
					{2040.0f, 473.0f, 45.0f, 32.0f}};
		coins_ = {{{295.0f, 1190.0f}}, {{580.0f, 1060.0f}}, {{875.0f, 930.0f}}, {{1180.0f, 800.0f}}, {{1465.0f, 670.0f}}, {{1765.0f, 540.0f}}, {{2090.0f, 410.0f}}};
		player_position_ = {68.0f, 1420.0f - kPlayerHeight};
		velocity_ = {};
		grounded_ = true;
		lives_ = 3;
		collected_coins_ = 0;
		coin_time_ = 0.0f;
		player_moving_ = false;
		started_ = false;
		paused_ = false;
		game_over_ = false;
		won_ = false;
		SetDisplay("start_panel", true);
		SetDisplay("pause_panel", false);
		SetDisplay("game_over_panel", false);
		SetDisplay("win_panel", false);
	}

	/// Resets the level and immediately returns to active play.
	void RestartGame()
	{
		ResetGame();
		StartGame();
	}

	/// Starts play and closes the title overlay.
	void StartGame()
	{
		started_ = true;
		paused_ = false;
		SetDisplay("start_panel", false);
	}

	/// Draws a solid-color world-space rectangle using the renderer's white texture.
	void DrawSolid(Math::Vector2 position, Math::Vector2 size, Math::Color color)
	{
		SpriteCommand command;
		command.position = position;
		command.size = size;
		command.color = color;
		GetSpriteBatch().Draw(command);
	}

	/// Draws a named atlas region at a requested world-space size.
	void DrawRegion(const std::string &name, Math::Vector2 position, Math::Vector2 size)
	{
		SpriteCommand command;
		atlas_->GetRegion(name).Apply(command);
		command.position = position;
		command.size = size;
		GetSpriteBatch().Draw(command);
	}

	/// Tiles an atlas platform strip across a horizontal platform.
	void DrawPlatform(const Platform &platform)
	{
		const auto &region = atlas_->GetRegion("platform");
		float remaining = platform.width;
		float x = platform.x;
		while (remaining > 0.0f)
		{
			const float width = (std::min)(96.0f, remaining);
			SpriteCommand command;
			region.Apply(command);
			command.position = {x, platform.y};
			command.size = {width, 48.0f};
			command.texture_size.x *= width / 96.0f;
			GetSpriteBatch().Draw(command);
			x += width;
			remaining -= width;
		}
	}

	/// Collects every unclaimed coin overlapping the player bounds.
	void CollectCoins()
	{
		for (auto &coin : coins_)
		{
			if (coin.collected)
				continue;
			const bool overlaps =
				player_position_.x < coin.position.x + 32.0f &&
				player_position_.x + kPlayerWidth > coin.position.x + 8.0f &&
				player_position_.y < coin.position.y + 32.0f &&
				player_position_.y + kPlayerHeight > coin.position.y + 8.0f;
			if (overlaps)
			{
				coin.collected = true;
				++collected_coins_;
			}
		}
	}

	/// Checks the player rectangle against all hazard rectangles.
	bool TouchesHazard() const noexcept
	{
		return std::any_of(hazards_.begin(), hazards_.end(), [&](const Hazard &hazard)
						   { return player_position_.x < hazard.x + hazard.width &&
									player_position_.x + kPlayerWidth > hazard.x &&
									player_position_.y < hazard.y + hazard.height &&
									player_position_.y + kPlayerHeight > hazard.y; });
	}

	/// Removes one life and respawns the player or opens the game-over menu.
	void LoseLife()
	{
		if (--lives_ <= 0)
		{
			game_over_ = true;
			SetDisplay("game_over_panel", true);
			return;
		}
		player_position_ = {68.0f, 1420.0f - kPlayerHeight};
		velocity_ = {};
		grounded_ = true;
	}

	/// Updates the document's live HUD and current overlay state.
	void UpdateUi()
	{
		if (!ui_tree_)
			return;
		if (auto *element = ui_tree_->FindById("score_value"))
			element->SetInnerRML(std::to_string(collected_coins_));
		if (auto *element = ui_tree_->FindById("lives_value"))
			element->SetInnerRML(std::to_string(lives_));
		if (auto *element = ui_tree_->FindById("status_value"))
			element->SetInnerRML(won_ ? "FINISHED" : paused_ ? "PAUSED"
												 : started_	 ? "PLAYING"
															 : "READY");
		const auto stats = GetFrameStats().GetSnapshot();
		if (auto *element = ui_tree_->FindById("fps_value"))
			element->SetInnerRML(std::to_string(static_cast<int>(stats.frames_per_second)));
		if (auto *element = ui_tree_->FindById("frame_value"))
			element->SetInnerRML(std::to_string(stats.cpu_frame_milliseconds).substr(0, 4) + " ms");
		if (auto *element = ui_tree_->FindById("draw_value"))
			element->SetInnerRML(std::to_string(stats.draw_calls));
		if (auto *element = ui_tree_->FindById("vertex_value"))
			element->SetInnerRML(std::to_string(stats.vertices));
		if (auto *element = ui_tree_->FindById("texture_value"))
			element->SetInnerRML(std::to_string(stats.texture_switches));
		if (auto *element = ui_tree_->FindById("gpu_value"))
			element->SetInnerRML(std::to_string(stats.gpu_frame_milliseconds).substr(0, 4) + " ms");
	}

	/// Shows or hides one overlay element by its BUI ID.
	void SetDisplay(const char *id, bool visible)
	{
		if (!ui_tree_)
			return;
		if (auto *element = ui_tree_->FindById(id))
			element->SetProperty("display", visible ? "flex" : "none");
	}

	Camera2D camera_;
	std::unique_ptr<SpriteBatchRenderer> renderer_;
	std::unique_ptr<TextRenderer> text_renderer_;
	std::unique_ptr<SpriteAtlas> atlas_;
	std::unique_ptr<SpriteAnimation> player_animation_;
	Rendering::Texture player_texture_;
	TextureHandle atlas_texture_ = 0;
	std::vector<Platform> platforms_;
	std::vector<Hazard> hazards_;
	std::vector<Collectible> coins_;
	Math::Vector2 player_position_{};
	Math::Vector2 velocity_{};
	std::unique_ptr<UI::BuiDocument> ui_document_;
	ActionListener start_listener_;
	ActionListener restart_listener_;
	std::unique_ptr<UI::BuiTree> ui_tree_;
	double coin_time_ = 0.0;
	std::uint32_t lives_ = 3;
	std::uint32_t collected_coins_ = 0;
	bool grounded_ = false;
	bool started_ = false;
	bool paused_ = false;
	bool game_over_ = false;
	bool won_ = false;
	bool player_moving_ = false;
};

/// Starts the game and records any startup or runtime failures beside the executable.
int main()
{
	try
	{
		const auto directory = GetExecutableDirectory();
		Engine::Logger::Get().SetLogFile(directory / "blacklight_demo.log");
		PlatformerGame game;
		return game.Run();
	}
	catch (const std::exception &error)
	{
		Engine::Logger::Get().Log(Engine::LogLevel::Error, error.what(), "Demo");
		return 1;
	}
}
