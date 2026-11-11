#include "rmlui/BuiLoader.h"
#include "rmlui/BuiTree.h"
#include "engine/Window.h"
#include "engine/Logger.h"
#include "blacklight_demo_bui.h"

#include <RmlUi/Core.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include <vector>

using namespace BlackLight;
using namespace BlackLight::UI;

namespace
{
	/// Locates the executable directory for the demo log.
	std::filesystem::path GetExecutableDirectory()
	{
		std::vector<wchar_t> path(32768);
		const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (!length || length >= path.size())
			throw std::runtime_error("Failed to locate the Demo executable");
		return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
	}

	/// Loads the regular and bold faces explicitly required by the demo stylesheet.
	bool LoadDemoFonts()
	{
		wchar_t directory[MAX_PATH]{};
		const UINT length = GetWindowsDirectoryW(directory, MAX_PATH);
		if (!length || length >= MAX_PATH)
			return false;
		const auto fonts = std::filesystem::path(directory) / "Fonts";
		return Rml::LoadFontFace((fonts / "arial.ttf").u8string()) &&
			Rml::LoadFontFace((fonts / "arialbd.ttf").u8string());
	}
}

/// Displays the embedded .bui document through the engine's OpenGL UI adapter.
class DemoWindow final : public Engine::Window {
public:
	/// Configures the window for the embedded UI.
	DemoWindow(const std::string& title)
		: Engine::Window(CreateConfig(title)) {}

	/// Loads fonts and the UI only after the engine has initialized RmlUi.
	bool OnInitialize() override {
		BL_LOG(::BlackLight::Engine::LogLevel::Debug, "Demo", "Initializing BlackLight3D UI Demo");

		BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "Loading embedded .bui resource: Main.bui");

		// Parse the original resource bytes directly from the executable.
		std::unique_ptr<BuiDocument> bui_document;
		BuiLoadError load_error;

		if (!BuiLoader::Parse(DemoUi::Find("Main.bui"), bui_document, load_error)) {
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Failed to parse embedded .bui: " + load_error.message);
			if (load_error.line > 0) {
				BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Line: " + std::to_string(load_error.line));
			}
			return false;
		}

		BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", ".bui file loaded successfully");

		// Get RmlUi context
		auto context = GetContext();
		if (!context) {
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Failed to get RmlUi context");
			return false;
		}
		if (!LoadDemoFonts()) {
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Failed to load the Arial regular and bold fonts");
			return false;
		}

		// Build RmlUi tree from .bui document
		BuiBuildError build_error;
		if (!BuiTree::Build(*context, *bui_document, ui_tree_, build_error)) {
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Failed to build RmlUi tree: " + build_error.message);
			return false;
		}

		BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "RmlUi tree created successfully");

		// Show the UI
		if (ui_tree_) {
			ui_tree_->Show();

			// Find and log some elements for verification
			if (ui_tree_->FindById("start_button")) {
				BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "Found start_button element");
			}
			if (ui_tree_->FindById("title_panel")) {
				BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "Found title_panel element");
			}
		}

		return true;
	}

	/// Releases the document before the engine shuts down its RmlUi context.
	void OnShutdown() override {
		BL_LOG(::BlackLight::Engine::LogLevel::Debug, "Demo", "Shutting down BlackLight3D UI Demo");
		if (ui_tree_) {
			ui_tree_.reset();
		}
	}

private:
	/// Requests a 1280 by 720 dp window rendered at the primary display's native DPI.
	static Engine::WindowConfig CreateConfig(const std::string& title) {
		Engine::WindowConfig config;
		const UINT dpi = GetDpiForSystem();
		config.rendering.surface.title = title;
		config.rendering.surface.width = static_cast<std::uint32_t>(MulDiv(1280, dpi, USER_DEFAULT_SCREEN_DPI));
		config.rendering.surface.height = static_cast<std::uint32_t>(MulDiv(720, dpi, USER_DEFAULT_SCREEN_DPI));
		config.rendering.backend = Engine::Rendering::RenderingBackend::OpenGL;
		config.rendering.enable_ui = true;
		return config;
	}

	std::unique_ptr<BuiTree> ui_tree_;
};

/// Runs the demo and records initialization failures without uncaught exceptions.
int main() {
	try {
		const auto directory = GetExecutableDirectory();
		Engine::Logger::Get().SetLogFile(directory / "blacklight_demo.log");
		BL_LOG(Engine::LogLevel::Info, "Demo", "Starting BlackLight3D UI Demo");

		DemoWindow window("BlackLight3D - UI Demo");
		return window.Run();
	}
	catch (const std::exception& e) {
		BL_LOG(Engine::LogLevel::Error, "Demo", std::string("Unhandled exception: ") + e.what());
		return 1;
	}
	catch (...) {
		BL_LOG(Engine::LogLevel::Error, "Demo", "Unknown error occurred");
		return 1;
	}
}
