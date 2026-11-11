#include "rmlui/UIXmlParser.h"
#include "engine/IGraphicsBackend.h"
#include "engine/backends/WindowsOpenGLBackend.h"
#include "rmlui/RmlUiTree.h"

#include <RmlUi/Core.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

#if defined(_WIN32)
#include <conio.h>
#endif

using namespace BlackLight::UI;
using namespace BlackLight::Engine;

namespace {

/// Keeps the console window open so the final output stays readable.
void WaitForAnyKey() {
	std::cout << "\nPress any key to continue . . ." << std::flush;
#if defined(_WIN32)
	_getch();
#else
	std::cin.get();
#endif
	std::cout << std::endl;
}

} // namespace

/// Loads XML content from a file.
std::string LoadXmlFile(const char* filepath) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "Error: Could not open file: " << filepath << std::endl;
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

int main(int argc, char* argv[]) {
	std::cout << "BlackLight3D - UI Rendering Demo\n" << std::endl;

	// Initialize graphics backend
	std::unique_ptr<IGraphicsBackend> backend = CreateGraphicsBackend("windows-opengl");
	if (!backend) {
		std::cerr << "Error: Failed to create graphics backend" << std::endl;
		WaitForAnyKey();
		return 1;
	}

	GraphicsWindow window_config;
	window_config.width = 1280;
	window_config.height = 720;
	window_config.title = "BlackLight3D UI Demo";

	GraphicsResult result = backend->Initialize(window_config);
	if (result != GraphicsResult::Success) {
		std::cerr << "Error: Failed to initialize graphics backend" << std::endl;
		WaitForAnyKey();
		return 1;
	}

	Rml::Context* rml_context = backend->GetContext();
	if (!rml_context) {
		std::cerr << "Error: Failed to get RmlUi context" << std::endl;
		WaitForAnyKey();
		return 1;
	}

	// Load and parse XML
	std::string xml_file = "UI\\Main.xml";
	if (argc > 1) {
		xml_file = argv[1];
	}

	std::cout << "Loading UI from: " << xml_file << std::endl;
	std::string xml_content = LoadXmlFile(xml_file.c_str());
	if (xml_content.empty()) {
		backend->Shutdown();
		WaitForAnyKey();
		return 1;
	}

	// Parse UI
	std::unique_ptr<UIScreen> screen;
	UIParseError parse_error;

	if (!UIXmlParser::Parse(xml_content, screen, parse_error)) {
		std::cerr << "Parse Error at line " << parse_error.line << ": " << parse_error.message << std::endl;
		backend->Shutdown();
		WaitForAnyKey();
		return 1;
	}

	// Build RmlUi tree
	std::unique_ptr<RmlUiTree> ui_tree;
	UIBuildError build_error;

	if (!RmlUiTree::Build(*rml_context, *screen, ui_tree, build_error)) {
		std::cerr << "Build Error: " << build_error.message << std::endl;
		backend->Shutdown();
		WaitForAnyKey();
		return 1;
	}

	ui_tree->Show();
	std::cout << "UI loaded successfully. Starting render loop...\n" << std::endl;

	// Render loop
	while (backend->Update() && !backend->ShouldClose()) {
		backend->Render();
		backend->Present();
	}

	std::cout << "\nShutting down..." << std::endl;
	ui_tree.reset();
	backend->Shutdown();

	std::cout << "Demo completed successfully" << std::endl;
	WaitForAnyKey();
	return 0;
}
