#include "rmlui/UIXmlParser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

using namespace BlackLight::UI;

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

/// Prints the UI element tree to console.
void PrintUITree(const UIElement* element, int indent = 0) {
	std::string prefix(indent * 2, ' ');
	
	if (element->GetKind() == UIElementKind::Screen) {
		std::cout << prefix << "[Screen]";
	} else if (element->GetKind() == UIElementKind::Panel) {
		std::cout << prefix << "[Panel]";
	} else if (element->GetKind() == UIElementKind::Text) {
		std::cout << prefix << "[Text]";
	}
	
	if (!element->GetId().empty()) {
		std::cout << " id=\"" << element->GetId() << "\"";
	}
	
	if (element->GetKind() == UIElementKind::Text) {
		const auto* text_elem = dynamic_cast<const UIText*>(element);
		if (text_elem) {
			std::cout << " content=\"" << text_elem->GetText() << "\"";
		}
	}
	
	if (element->GetKind() != UIElementKind::Text) {
		const auto* container = dynamic_cast<const UIContainer*>(element);
		if (container) {
			const auto& layout = container->GetLayout();
			if (layout.auto_size) {
				std::cout << " auto_size=true";
			}
			if (layout.width.has_value()) {
				std::cout << " width=" << layout.width.value();
			}
			if (layout.height.has_value()) {
				std::cout << " height=" << layout.height.value();
			}
		}
	}
	
	std::cout << std::endl;
	
	if (element->GetKind() != UIElementKind::Text) {
		const auto* container = dynamic_cast<const UIContainer*>(element);
		if (container) {
			for (const auto& child : container->GetChildren()) {
				PrintUITree(child.get(), indent + 1);
			}
		}
	}
}

/// Validates and prints the parsed UI tree or error information.
void PrintParseResult(const UIParseError& error, const std::unique_ptr<UIScreen>& screen) {
	if (!error.message.empty()) {
		std::cerr << "Parse Error at line " << error.line << ": " << error.message << std::endl;
		return;
	}
	
	if (!screen) {
		std::cerr << "Error: No screen was created." << std::endl;
		return;
	}
	
	std::cout << "\n=== UI Tree Structure ===" << std::endl;
	PrintUITree(screen.get());
	std::cout << "========================\n" << std::endl;
}

int main(int argc, char* argv[]) {
	std::cout << "BlackLight3D UI Demo - XML Parser Test\n" << std::endl;
	
	// Determine XML file path
	std::string xml_file = "UI\\Main.xml";
	if (argc > 1) {
		xml_file = argv[1];
	}
	
	// Load XML file
	std::string xml_content = LoadXmlFile(xml_file.c_str());
	if (xml_content.empty()) {
		return 1;
	}
	
	// Parse XML
	std::unique_ptr<UIScreen> screen;
	UIParseError error;
	
	std::cout << "Parsing XML from: " << xml_file << std::endl;
	std::cout << "---" << std::endl;
	
	bool success = UIXmlParser::Parse(xml_content, screen, error);
	
	PrintParseResult(error, screen);
	
	if (success && screen) {
		std::cout << "? Parse successful!" << std::endl;
		std::cout << "Screen ID: " << (screen->GetId().empty() ? "(none)" : screen->GetId()) << std::endl;
		std::cout << "Children count: " << screen->GetChildren().size() << std::endl;
		return 0;
	} else {
		std::cout << "? Parse failed!" << std::endl;
		return 1;
	}
}
