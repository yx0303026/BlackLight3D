#include "engine/Window.h"
#include "rmlui/HtmlCssParser.h"
#include "rmlui/RmlUiTree.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace
{
	/// Loads and displays an HTML desktop UI through the lightweight Window host.
	class DemoWindow final : public BlackLight::Engine::Window
	{
	public:
		/// Selects the document before the desktop runtime is initialized.
		explicit DemoWindow(std::string html_path) : Window(CreateConfig()), html_path_(std::move(html_path)) {}

	protected:
		/// Parses the document and builds its UI in the host-owned context.
		bool OnInitialize() override
		{
			std::ifstream file(html_path_);
			if (!file)
			{
				std::cerr << "Could not open UI file: " << html_path_ << '\n';
				return false;
			}
			std::ostringstream buffer;
			buffer << file.rdbuf();
			std::unique_ptr<BlackLight::UI::HTMLDocument> document;
			BlackLight::UI::HTMLParseError parse_error;
			if (!BlackLight::UI::HtmlCssParser::Parse(buffer.str(), "", document, parse_error))
			{
				std::cerr << "Parse error at line " << parse_error.line << ": " << parse_error.message << '\n';
				return false;
			}
			BlackLight::UI::UIBuildError build_error;
			if (!GetContext() ||
				!BlackLight::UI::RmlUiTree::Build(*GetContext(), *document, ui_tree_, build_error))
			{
				std::cerr << "UI build error: " << build_error.message << '\n';
				return false;
			}
			ui_tree_->Show();
			return true;
		}

		/// Destroys the document before Window releases the RmlUi context.
		void OnShutdown() override { ui_tree_.reset(); }

	private:
		/// Sets the desktop window dimensions and title.
		static BlackLight::Engine::WindowConfig CreateConfig()
		{
			BlackLight::Engine::WindowConfig config;
			config.window.width = 1280;
			config.window.height = 720;
			config.window.title = "BlackLight3D UI Demo";
			return config;
		}

		std::string html_path_;
		std::unique_ptr<BlackLight::UI::RmlUiTree> ui_tree_;
	};
}

/// Starts the desktop UI application using the common engine lifecycle.
int main(int argc, char *argv[])
{
	try
	{
		DemoWindow window(argc > 1 ? argv[1] : "UI/Main.html");
		return window.Run();
	}
	catch (const std::exception &error)
	{
		std::cerr << "Desktop runtime failed: " << error.what() << '\n';
		return 1;
	}
}
