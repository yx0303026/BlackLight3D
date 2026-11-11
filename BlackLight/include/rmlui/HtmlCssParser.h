#pragma once

#include "rmlui/UIElement.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace BlackLight::UI
{

	/// Carries a human-readable HTML/CSS parse error and its source line.
	struct HTMLParseError
	{
		/// Describes the first parsing or validation failure.
		std::string message;
		/// Contains the one-based source line when available, or zero otherwise.
		std::size_t line = 0;
	};

	/// CSS style properties that can be applied to UI elements.
	/// These are normalized to RmlUi-compatible values.
	struct CSSProperties
	{
		// Layout properties
		std::string display;  // "block", "inline", "flex", etc.
		std::string width;	  // "100%", "320px", "auto", etc.
		std::string height;	  // "200px", "50%", "auto", etc.
		std::string padding;  // "10px", "5px 10px", etc.
		std::string margin;	  // "10px", "5px 10px", etc.
		std::string position; // "relative", "absolute", "fixed"
		std::string top;
		std::string left;
		std::string right;
		std::string bottom;
		std::string flex_direction;	 // "row", "column"
		std::string justify_content; // "flex-start", "center", "flex-end"
		std::string align_items;	 // "flex-start", "center", "flex-end"
		std::string gap;			 // "10px"
		std::string flex_wrap;		 // "wrap", "nowrap"

		// Typography properties
		std::string font_size;		 // "16px", "1.5em", etc.
		std::string font_family;	 // "Arial", "sans-serif", etc.
		std::string font_weight;	 // "bold", "400", "700", etc.
		std::string font_style;		 // "normal", "italic"
		std::string text_align;		 // "left", "center", "right"
		std::string color;			 // "#FF0000", "rgb(255, 0, 0)", etc.
		std::string text_decoration; // "underline", "none", etc.
		std::string line_height;	 // "1.5", "20px", etc.

		// Background & Border properties
		std::string background_color; // "#FFFFFF", "rgb(255, 255, 255)", etc.
		std::string background_image; // "url(...)"
		std::string border;			  // "1px solid black"
		std::string border_width;	  // "1px", "1px 2px", etc.
		std::string border_color;	  // "#000000", etc.
		std::string border_radius;	  // "5px", "5px 10px", etc.

		// Visual effects
		std::string opacity;	// "0.5", "1.0", etc.
		std::string box_shadow; // "0 0 10px rgba(0,0,0,0.5)"
		std::string transform;	// "rotate(45deg)", etc.
		std::string z_index;	// "10", "100", etc.

		// Overflow & visibility
		std::string overflow;	 // "visible", "hidden", "scroll", etc.
		std::string white_space; // "nowrap", "pre-wrap", etc.
	};

	/// Parses HTML with inline CSS and external stylesheets into a UI tree.
	/// Supports standard HTML elements and maps them to BlackLight UI controls.
	class HtmlCssParser final
	{
	public:
		/// Parses HTML and CSS, replaces screen only after a complete successful parse.
		/// @param html The HTML content to parse
		/// @param css Optional CSS stylesheet content
		/// @param screen Output UIScreen if parse succeeds
		/// @param error Output error information if parse fails
		/// @return true if parse succeeds, false otherwise
		static bool Parse(std::string_view html, std::string_view css,
						  std::unique_ptr<UIScreen> &screen, HTMLParseError &error);

		/// Parses HTML from a file and optional CSS from another file
		/// @param html_file Path to HTML file
		/// @param css_file Optional path to CSS file
		/// @param screen Output UIScreen if parse succeeds
		/// @param error Output error information if parse fails
		/// @return true if parse succeeds, false otherwise
		static bool ParseFromFiles(std::string_view html_file, std::string_view css_file,
								   std::unique_ptr<UIScreen> &screen, HTMLParseError &error);
	};

} // namespace BlackLight::UI
