#pragma once

#include "rmlui/UIElement.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace BlackLight::UI {

/// Carries a human-readable XML parse error and its source line.
struct UIParseError {
	/// Describes the first parsing or validation failure.
	std::string message;
	/// Contains the one-based source line when available, or zero otherwise.
	std::size_t line = 0;
};

/// Parses the supported Screen/Panel XML format into a UI tree.
class UIXmlParser final {
public:
	/// Parses XML and replaces screen only after a complete successful parse.
	static bool Parse(std::string_view xml, std::unique_ptr<UIScreen>& screen, UIParseError& error);
};

} // namespace BlackLight::UI
