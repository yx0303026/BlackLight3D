#pragma once

#include "rmlui/HTMLElement.h"

#include <memory>
#include <string>
#include <string_view>

namespace BlackLight::UI {

/// Carries a human-readable HTML parse error and its source line.
struct HTMLParseError {
	/// Describes the first parsing or validation failure.
	std::string message;
	/// Contains the one-based source line when available, or zero otherwise.
	std::size_t line = 0;
};

/// Parses HTML and CSS into an HTML element tree.
class HTMLParser final {
public:
	/// Parses HTML and replaces document only after a complete successful parse.
	/// 
	/// @param html The HTML content to parse
	/// @param css Optional CSS stylesheet to apply
	/// @param document Output: the parsed document tree
	/// @param error Output: parse error details if parsing fails
	/// @return true if parsing succeeded, false if an error occurred
	static bool Parse(std::string_view html, std::string_view css,
		std::unique_ptr<HTMLDocument>& document, HTMLParseError& error);

	/// Parses HTML and CSS, keeping the parse result out of document unless parsing succeeds.
	/// HtmlCssParser builds on this entry point and adds identifier validation and file loading.
	///
	/// @param html The HTML content to parse
	/// @param css Optional CSS stylesheet to apply
	/// @param document Output: the parsed document tree
	/// @param error Output: parse error details if parsing fails
	/// @return true if parsing succeeded, false if an error occurred
	static bool ParseWithCss(std::string_view html, std::string_view css,
		std::unique_ptr<HTMLDocument>& document, HTMLParseError& error);

	/// Validates HTML structure without parsing into a tree.
	/// 
	/// @param html The HTML content to validate
	/// @param error Output: validation error details if validation fails
	/// @return true if HTML is valid, false otherwise
	static bool Validate(std::string_view html, HTMLParseError& error);
};

} // namespace BlackLight::UI
