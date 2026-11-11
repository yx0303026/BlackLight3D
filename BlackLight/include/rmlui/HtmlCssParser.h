#pragma once

#include "rmlui/HTMLParser.h"

#include <memory>
#include <string>
#include <string_view>

namespace BlackLight::UI
{

	/// Parses HTML with inline CSS and external stylesheets into an HTML element tree.
	/// Inline styles are normalized to RmlUi-compatible values, and identifiers must be unique.
	class HtmlCssParser final
	{
	public:
		/// Parses HTML and CSS, replacing document only after a complete successful parse.
		/// @param html The HTML content to parse
		/// @param css Optional CSS stylesheet content attached to the document
		/// @param document Output HTML document tree if parse succeeds
		/// @param error Output error information if parse fails
		/// @return true if parse succeeds, false otherwise
		static bool Parse(std::string_view html, std::string_view css,
						  std::unique_ptr<HTMLDocument> &document, HTMLParseError &error);

		/// Parses HTML from a file and optional CSS from another file
		/// @param html_file Path to HTML file
		/// @param css_file Optional path to CSS file
		/// @param document Output HTML document tree if parse succeeds
		/// @param error Output error information if parse fails
		/// @return true if parse succeeds, false otherwise
		static bool ParseFromFiles(std::string_view html_file, std::string_view css_file,
								   std::unique_ptr<HTMLDocument> &document, HTMLParseError &error);
	};

} // namespace BlackLight::UI