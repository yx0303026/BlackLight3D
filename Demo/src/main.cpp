#include "rmlui/BuiLoader.h"
#include "rmlui/BuiTree.h"
#include "engine/Window.h"
#include "engine/Logger.h"

#include <RmlUi/Core.h>
#include <iostream>
#include <filesystem>

using namespace BlackLight;
using namespace BlackLight::UI;

class DemoWindow : public Engine::Window {
public:
	DemoWindow(const std::string& title) : Engine::Window(CreateConfig(title)) {}

	bool OnInitialize() override {
		BL_LOG(::BlackLight::Engine::LogLevel::Debug, "Demo", "Initializing BlackLight3D UI Demo");

		// Try to load Main.bui
		std::string bui_path = "UI/Main.bui";

		// Check if file exists
		if (!std::filesystem::exists(bui_path)) {
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "UI file not found: " + bui_path);
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Ensure the UI folder is in the working directory");
			return false;
		}

		BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "Loading .bui file: " + bui_path);

		// Load .bui file
		std::unique_ptr<BuiDocument> bui_document;
		BuiLoadError load_error;

		if (!BuiLoader::LoadFromFile(bui_path, bui_document, load_error)) {
			BL_LOG(::BlackLight::Engine::LogLevel::Error, "Demo", "Failed to load .bui file: " + load_error.message);
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
			if (auto start_btn = ui_tree_->FindById("start_button")) {
				BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "Found start_button element");
			}
			if (auto title_panel = ui_tree_->FindById("title_panel")) {
				BL_LOG(::BlackLight::Engine::LogLevel::Info, "Demo", "Found title_panel element");
			}
		}

		return true;
	}

	void OnShutdown() override {
		BL_LOG(::BlackLight::Engine::LogLevel::Debug, "Demo", "Shutting down BlackLight3D UI Demo");
		if (ui_tree_) {
			ui_tree_->Close();
		}
	}

	void OnUpdate() override {
		// Demo update logic (if needed)
	}

	void OnRender() override {
		// Rendering is handled by the engine
	}

private:
	static Engine::WindowConfig CreateConfig(const std::string& title) {
		Engine::WindowConfig config;
		config.rendering.surface.title = title;
		config.rendering.surface.width = 1280;
		config.rendering.surface.height = 720;
		config.rendering.backend = Engine::Rendering::RenderingBackend::OpenGL;
		config.rendering.enable_ui = true;
		return config;
	}

	std::unique_ptr<BuiTree> ui_tree_;
};

int main() {
	try {
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
