#include "rmlui/HtmlCssParser.h"

#include "rmlui/UIElement.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace BlackLight::UI {

namespace {

	// ============== Constants ==============
	const std::unordered_set<std::string> BLOCK_ELEMENTS = {
		"div", "p", "header", "footer", "main", "section", "article", "nav", "aside",
		"h1", "h2", "h3", "h4", "h5", "h6", "blockquote", "ol", "ul", "li", "form",
		"table", "tr", "td", "th", "thead", "tbody", "tfoot"
	};

	const std::unordered_set<std::string> INLINE_ELEMENTS = {
		"span", "a", "b", "i", "strong", "em", "small", "mark", "code", "kbd", "samp"
	};

	const std::unordered_set<std::string> VOID_ELEMENTS = {
		"area", "base", "br", "col", "embed", "hr", "img", "input",
		"link", "meta", "param", "source", "track", "wbr"
	};

	const std::unordered_map<std::string, UIElementKind> ELEMENT_KIND_MAP = {
		{"button", UIElementKind::Button},
		{"select", UIElementKind::ListBox},
		{"list", UIElementKind::ListView},
		{"table", UIElementKind::DataGrid}
	};

	// ============== Helper Functions ==============

	/// Trims whitespace from both ends of a string.
	std::string_view Trim(std::string_view str) {
		while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
			str.remove_prefix(1);
		}
		while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
			str.remove_suffix(1);
		}
		return str;
	}

	/// Converts a string to lowercase.
	std::string ToLower(std::string_view str) {
		std::string result(str);
		std::transform(result.begin(), result.end(), result.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return result;
	}

	/// Splits a string by a delimiter.
	std::vector<std::string> Split(std::string_view str, char delimiter) {
		std::vector<std::string> result;
		std::string current;
		for (char c : str) {
			if (c == delimiter) {
				if (!current.empty()) {
					result.push_back(current);
					current.clear();
				}
			} else {
				current += c;
			}
		}
		if (!current.empty()) {
			result.push_back(current);
		}
		return result;
	}

	/// Sets error information.
	void SetError(HTMLParseError& error, std::string message, std::size_t line = 0) {
		error.message = std::move(message);
		error.line = line;
	}

	/// Validates a CSS dimension string (e.g., "100px", "50%", "auto")
	bool IsValidCSSUnit(std::string_view value) {
		value = Trim(value);
		if (value == "auto") return true;

		// Must have digits at start
		if (value.empty() || (!std::isdigit(value[0]) && value[0] != '-' && value[0] != '.')) {
			return false;
		}

		// Extract numeric part
		size_t i = (value[0] == '-') ? 1 : 0;
		while (i < value.size() && (std::isdigit(value[i]) || value[i] == '.')) {
			i++;
		}

		if (i == (value[0] == '-' ? 1 : 0)) {
			return false; // No numeric part found
		}

		// Check unit
		std::string_view unit = value.substr(i);
		if (unit.empty()) return true; // No unit is okay, defaults to px

		return unit == "px" || unit == "em" || unit == "rem" || unit == "%" ||
			   unit == "pt" || unit == "cm" || unit == "mm" || unit == "in" ||
			   unit == "ex" || unit == "ch" || unit == "vw" || unit == "vh" ||
			   unit == "vmin" || unit == "vmax";
	}

	/// Validates a hex color
	bool IsValidHexColor(std::string_view value) {
		value = Trim(value);
		if (value.size() < 4 || value[0] != '#') return false;

		size_t hex_len = value.size() - 1;
		if (hex_len != 3 && hex_len != 6 && hex_len != 8) return false;

		for (size_t i = 1; i < value.size(); i++) {
			char c = value[i];
			if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
		}
		return true;
	}

	/// Validates an RGB/RGBA color
	bool IsValidRGBColor(std::string_view value) {
		value = Trim(value);
		std::string lower = ToLower(value);

		if (lower.substr(0, std::min(static_cast<size_t>(4), lower.size())) != "rgba" &&
			lower.substr(0, std::min(static_cast<size_t>(3), lower.size())) != "rgb") {
			return false;
		}

		// Simple check: contains parentheses and commas
		return value.find('(') != std::string::npos && 
			   value.find(')') != std::string::npos;
	}

	/// Checks if value is a valid RmlUi dimension.
	bool IsValidDimension(std::string_view value) {
		return IsValidCSSUnit(value);
	}

	/// Normalizes a CSS dimension to RmlUi format (e.g., "100" -> "100px").
	std::string NormalizeDimension(std::string_view value) {
		value = Trim(value);
		if (value == "auto") return "auto";

		// Check if it's a valid dimension
		if (!IsValidCSSUnit(value)) {
			return std::string(value);
		}

		// If no unit, assume pixels
		std::string str(value);
		if (!str.empty() && std::isdigit(str.back())) {
			return str + "px";
		}

		return str;
	}

	/// Normalizes a CSS color value to RmlUi format.
	std::string NormalizeColor(std::string_view value) {
		value = Trim(value);

		// Hex color
		if (IsValidHexColor(value)) {
			return std::string(value);
		}

		// RGB/RGBA color
		if (IsValidRGBColor(value)) {
			return std::string(value);
		}

		// Named colors - return as-is, RmlUi supports many
		return std::string(value);
	}

	/// Parses CSS inline styles from a style attribute.
	CSSProperties ParseInlineStyles(std::string_view style_attr) {
		CSSProperties props;

		if (style_attr.empty()) {
			return props;
		}

		auto declarations = Split(style_attr, ';');
		for (const auto& decl : declarations) {
			auto colon_pos = decl.find(':');
			if (colon_pos == std::string::npos) continue;

			std::string_view property = Trim(decl.substr(0, colon_pos));
			std::string_view value = Trim(decl.substr(colon_pos + 1));
			std::string prop_lower = ToLower(property);

			// Layout properties
			if (prop_lower == "display") {
				props.display = std::string(value);
			} else if (prop_lower == "width") {
				props.width = NormalizeDimension(value);
			} else if (prop_lower == "height") {
				props.height = NormalizeDimension(value);
			} else if (prop_lower == "padding") {
				props.padding = std::string(value);
			} else if (prop_lower == "margin") {
				props.margin = std::string(value);
			} else if (prop_lower == "position") {
				props.position = std::string(value);
			} else if (prop_lower == "top") {
				props.top = NormalizeDimension(value);
			} else if (prop_lower == "left") {
				props.left = NormalizeDimension(value);
			} else if (prop_lower == "right") {
				props.right = NormalizeDimension(value);
			} else if (prop_lower == "bottom") {
				props.bottom = NormalizeDimension(value);
			} else if (prop_lower == "flex-direction") {
				props.flex_direction = std::string(value);
			} else if (prop_lower == "justify-content") {
				props.justify_content = std::string(value);
			} else if (prop_lower == "align-items") {
				props.align_items = std::string(value);
			} else if (prop_lower == "gap") {
				props.gap = NormalizeDimension(value);
			} else if (prop_lower == "flex-wrap") {
				props.flex_wrap = std::string(value);
			// Typography properties
			} else if (prop_lower == "font-size") {
				props.font_size = NormalizeDimension(value);
			} else if (prop_lower == "font-family") {
				props.font_family = std::string(value);
			} else if (prop_lower == "font-weight") {
				props.font_weight = std::string(value);
			} else if (prop_lower == "font-style") {
				props.font_style = std::string(value);
			} else if (prop_lower == "text-align") {
				props.text_align = std::string(value);
			} else if (prop_lower == "color") {
				props.color = NormalizeColor(value);
			} else if (prop_lower == "text-decoration") {
				props.text_decoration = std::string(value);
			} else if (prop_lower == "line-height") {
				props.line_height = std::string(value);
			// Background & Border properties
			} else if (prop_lower == "background-color") {
				props.background_color = NormalizeColor(value);
			} else if (prop_lower == "background" || prop_lower == "bg") {
				props.background_color = NormalizeColor(value);
			} else if (prop_lower == "border") {
				props.border = std::string(value);
			} else if (prop_lower == "border-width") {
				props.border_width = std::string(value);
			} else if (prop_lower == "border-color") {
				props.border_color = NormalizeColor(value);
			} else if (prop_lower == "border-radius") {
				props.border_radius = std::string(value);
			// Visual effects
			} else if (prop_lower == "opacity") {
				props.opacity = std::string(value);
			} else if (prop_lower == "box-shadow") {
				props.box_shadow = std::string(value);
			} else if (prop_lower == "z-index") {
				props.z_index = std::string(value);
			// Overflow & visibility
			} else if (prop_lower == "overflow") {
				props.overflow = std::string(value);
			} else if (prop_lower == "white-space") {
				props.white_space = std::string(value);
			}
		}

		return props;
	}

	/// Converts CSS properties to RmlUi compatible style string.
	std::string ConvertCSSToRmlUiStyle(const CSSProperties& props) {
		std::ostringstream oss;

		// Layout properties
		if (!props.width.empty() && props.width != "auto") {
			oss << "width: " << props.width << "; ";
		}
		if (!props.height.empty() && props.height != "auto") {
			oss << "height: " << props.height << "; ";
		}
		if (!props.padding.empty()) {
			oss << "padding: " << props.padding << "; ";
		}
		if (!props.margin.empty()) {
			oss << "margin: " << props.margin << "; ";
		}
		if (!props.position.empty()) {
			oss << "position: " << props.position << "; ";
		}
		if (!props.top.empty()) {
			oss << "top: " << props.top << "; ";
		}
		if (!props.left.empty()) {
			oss << "left: " << props.left << "; ";
		}
		if (!props.right.empty()) {
			oss << "right: " << props.right << "; ";
		}
		if (!props.bottom.empty()) {
			oss << "bottom: " << props.bottom << "; ";
		}
		if (!props.flex_direction.empty()) {
			oss << "flex-direction: " << props.flex_direction << "; ";
		}
		if (!props.justify_content.empty()) {
			oss << "justify-content: " << props.justify_content << "; ";
		}
		if (!props.align_items.empty()) {
			oss << "align-items: " << props.align_items << "; ";
		}
		if (!props.gap.empty()) {
			oss << "gap: " << props.gap << "; ";
		}
		if (!props.flex_wrap.empty()) {
			oss << "flex-wrap: " << props.flex_wrap << "; ";
		}

		// Typography properties
		if (!props.font_size.empty()) {
			oss << "font-size: " << props.font_size << "; ";
		}
		if (!props.font_family.empty()) {
			oss << "font-family: " << props.font_family << "; ";
		}
		if (!props.font_weight.empty()) {
			oss << "font-weight: " << props.font_weight << "; ";
		}
		if (!props.font_style.empty()) {
			oss << "font-style: " << props.font_style << "; ";
		}
		if (!props.text_align.empty()) {
			oss << "text-align: " << props.text_align << "; ";
		}
		if (!props.color.empty()) {
			oss << "color: " << props.color << "; ";
		}
		if (!props.text_decoration.empty()) {
			oss << "text-decoration: " << props.text_decoration << "; ";
		}
		if (!props.line_height.empty()) {
			oss << "line-height: " << props.line_height << "; ";
		}

		// Background & Border properties
		if (!props.background_color.empty()) {
			oss << "background-color: " << props.background_color << "; ";
		}
		if (!props.border.empty()) {
			oss << "border: " << props.border << "; ";
		}
		if (!props.border_width.empty()) {
			oss << "border-width: " << props.border_width << "; ";
		}
		if (!props.border_color.empty()) {
			oss << "border-color: " << props.border_color << "; ";
		}
		if (!props.border_radius.empty()) {
			oss << "border-radius: " << props.border_radius << "; ";
		}

		// Visual effects
		if (!props.opacity.empty()) {
			oss << "opacity: " << props.opacity << "; ";
		}
		if (!props.box_shadow.empty()) {
			oss << "box-shadow: " << props.box_shadow << "; ";
		}
		if (!props.z_index.empty()) {
			oss << "z-index: " << props.z_index << "; ";
		}

		// Overflow & visibility
		if (!props.overflow.empty()) {
			oss << "overflow: " << props.overflow << "; ";
		}
		if (!props.white_space.empty()) {
			oss << "white-space: " << props.white_space << "; ";
		}

		std::string result = oss.str();
		if (!result.empty() && result.back() == ' ') {
			result.pop_back();
		}
		return result;
	}

	/// Maps HTML element names to UIElementKind.
	UIElementKind GetElementKind(std::string_view tag_name) {
		std::string lower = ToLower(tag_name);
		auto it = ELEMENT_KIND_MAP.find(lower);
		if (it != ELEMENT_KIND_MAP.end()) {
			return it->second;
		}
		return UIElementKind::Panel; // Default to Panel for most elements
	}

	/// Checks if an element is a void element (self-closing).
	bool IsVoidElement(std::string_view tag_name) {
		return VOID_ELEMENTS.count(ToLower(tag_name)) > 0;
	}

	/// Simple HTML tokenizer and parser
	class HTMLTokenizer {
	public:
		enum class TokenType {
			StartTag,
			EndTag,
			Text,
			Comment,
			Doctype,
			EOF_
		};

		struct Token {
			TokenType type;
			std::string name;           // Tag name or text content
			std::unordered_map<std::string, std::string> attributes;
			std::size_t line = 1;
		};

		HTMLTokenizer(std::string_view html) : html_(html), pos_(0), line_(1) {}

		Token NextToken() {
			SkipWhitespace();
			if (pos_ >= html_.size()) {
				return Token{TokenType::EOF_, "", {}, line_};
			}

			if (html_[pos_] == '<') {
				return ParseTag();
			}

			return ParseText();
		}

	private:
		std::string_view html_;
		std::size_t pos_;
		std::size_t line_;

		void SkipWhitespace() {
			while (pos_ < html_.size() && std::isspace(static_cast<unsigned char>(html_[pos_]))) {
				if (html_[pos_] == '\n') line_++;
				pos_++;
			}
		}

		Token ParseTag() {
			Token token{TokenType::StartTag, "", {}, line_};
			pos_++; // Skip '<'

			if (pos_ >= html_.size()) {
				return token;
			}

			// Check for comment
			if (pos_ + 2 < html_.size() && html_.substr(pos_, 3) == "!--") {
				pos_ += 3;
				while (pos_ + 2 < html_.size() && html_.substr(pos_, 3) != "-->") {
					if (html_[pos_] == '\n') line_++;
					pos_++;
				}
				if (pos_ + 2 < html_.size()) pos_ += 3;
				token.type = TokenType::Comment;
				return token;
			}

			// Check for end tag
			if (html_[pos_] == '/') {
				token.type = TokenType::EndTag;
				pos_++;
			}

			// Get tag name
			std::string tag_name;
			while (pos_ < html_.size() && (std::isalnum(static_cast<unsigned char>(html_[pos_])) || 
					html_[pos_] == '-' || html_[pos_] == ':')) {
				tag_name += html_[pos_++];
			}
			token.name = tag_name;

			// Parse attributes
			while (pos_ < html_.size() && html_[pos_] != '>') {
				SkipWhitespace();
				if (html_[pos_] == '>') break;

				std::string attr_name;
				while (pos_ < html_.size() && html_[pos_] != '=' && html_[pos_] != '>' &&
					   !std::isspace(static_cast<unsigned char>(html_[pos_]))) {
					attr_name += html_[pos_++];
				}

				SkipWhitespace();
				std::string attr_value;

				if (pos_ < html_.size() && html_[pos_] == '=') {
					pos_++;
					SkipWhitespace();
					char quote = 0;
					if (pos_ < html_.size() && (html_[pos_] == '"' || html_[pos_] == '\'')) {
						quote = html_[pos_++];
					}

					while (pos_ < html_.size()) {
						if (quote && html_[pos_] == quote) {
							pos_++;
							break;
						}
						if (!quote && (html_[pos_] == '>' || std::isspace(static_cast<unsigned char>(html_[pos_])))) {
							break;
						}
						attr_value += html_[pos_++];
					}
				}

				if (!attr_name.empty()) {
					token.attributes[ToLower(attr_name)] = attr_value;
				}
			}

			if (pos_ < html_.size() && html_[pos_] == '>') {
				pos_++;
			}

			return token;
		}

		Token ParseText() {
			Token token{TokenType::Text, "", {}, line_};
			while (pos_ < html_.size() && html_[pos_] != '<') {
				if (html_[pos_] == '\n') line_++;
				token.name += html_[pos_++];
			}
			return token;
		}
	};

	/// Recursively parses HTML elements into UIElement tree.
	bool ParseHTMLElement(HTMLTokenizer& tokenizer, std::unique_ptr<UIElement>& output,
		std::unordered_set<std::string>& identifiers, HTMLParseError& error) {
		
		auto token = tokenizer.NextToken();
		
		if (token.type == HTMLTokenizer::TokenType::EOF_ || 
			token.type == HTMLTokenizer::TokenType::EndTag) {
			return true;
		}

		if (token.type == HTMLTokenizer::TokenType::Comment) {
			return ParseHTMLElement(tokenizer, output, identifiers, error);
		}

		if (token.type == HTMLTokenizer::TokenType::Text) {
			std::string_view text = Trim(token.name);
			if (!text.empty()) {
				output = std::make_unique<UIText>(std::string(text));
			}
			return true;
		}

		// It's a start tag
		std::string tag_lower = ToLower(token.name);
		
		// Extract attributes
		std::string id = token.attributes.count("id") ? token.attributes["id"] : "";
		std::string style_attr = token.attributes.count("style") ? token.attributes["style"] : "";

		// Validate identifier uniqueness
		if (!id.empty() && !identifiers.insert(id).second) {
			SetError(error, std::string("Control identifiers must be unique: ") + id, token.line);
			return false;
		}

		// Parse CSS properties from style attribute
		CSSProperties css_props = ParseInlineStyles(style_attr);
		std::string rmlui_style = ConvertCSSToRmlUiStyle(css_props);

		// Create appropriate UIElement
		std::unique_ptr<UIContainer> container;
		UIElementKind kind = GetElementKind(tag_lower);

		// Build layout from CSS properties
		UIElementLayout layout;
		if (!css_props.width.empty() && css_props.width != "auto") {
			layout.width = css_props.width;
		}
		if (!css_props.height.empty() && css_props.height != "auto") {
			layout.height = css_props.height;
		}

		if (tag_lower == "button") {
			container = std::make_unique<UIControl>(UIElementKind::Button, id, layout, rmlui_style);
		} else if (tag_lower == "select") {
			container = std::make_unique<UIControl>(UIElementKind::ListBox, id, layout, rmlui_style);
		} else if (tag_lower == "table") {
			container = std::make_unique<UIControl>(UIElementKind::DataGrid, id, layout, rmlui_style);
		} else {
			// Default to panel for divs and other containers
			container = std::make_unique<UIPanel>(id, layout, rmlui_style);
		}

		// Parse children
		while (true) {
			auto child_token = tokenizer.NextToken();
			
			if (child_token.type == HTMLTokenizer::TokenType::EOF_) {
				break;
			}

			if (child_token.type == HTMLTokenizer::TokenType::EndTag) {
				if (ToLower(child_token.name) == tag_lower) {
					break;
				}
				continue;
			}

			if (child_token.type == HTMLTokenizer::TokenType::Comment) {
				continue;
			}

			if (child_token.type == HTMLTokenizer::TokenType::Text) {
				std::string_view text = Trim(child_token.name);
				if (!text.empty()) {
					auto text_element = std::make_unique<UIText>(std::string(text));
					if (!container->AddChild(std::move(text_element))) {
						SetError(error, "Could not attach text element", child_token.line);
						return false;
					}
				}
				continue;
			}

			// It's a start tag (child element)
			std::unique_ptr<UIElement> child;
			if (!ParseHTMLElement(tokenizer, child, identifiers, error)) {
				return false;
			}

			if (child && !container->AddChild(std::move(child))) {
				SetError(error, "Could not attach child element", child_token.line);
				return false;
			}

			if (IsVoidElement(child_token.name)) {
				// Void elements don't have closing tags
				continue;
			}
		}

		output = std::move(container);
		return true;
	}

	/// Reads file content into a string.
	bool ReadFile(std::string_view file_path, std::string& content, HTMLParseError& error) {
		std::ifstream file(std::string(file_path), std::ios::binary);
		if (!file.is_open()) {
			SetError(error, std::string("Failed to open file: ") + std::string(file_path));
			return false;
		}

		std::ostringstream oss;
		oss << file.rdbuf();
		content = oss.str();
		file.close();
		return true;
	}

} // namespace

bool HtmlCssParser::Parse(std::string_view html, std::string_view css,
	std::unique_ptr<UIScreen>& screen, HTMLParseError& error) {
	
	error = {};

	if (html.empty()) {
		SetError(error, "HTML content is empty");
		return false;
	}

	// Create root screen
	auto root_screen = std::make_unique<UIScreen>();

	// Add CSS as stylesheet if provided
	if (!css.empty()) {
		root_screen->AddStyleSheet("default", std::string(css));
	}

	// Parse HTML
	HTMLTokenizer tokenizer(html);
	std::unordered_set<std::string> identifiers;
	std::unique_ptr<UIElement> parsed_root;

	// Find the body element or root element
	auto token = tokenizer.NextToken();
	while (token.type != HTMLTokenizer::TokenType::EOF_) {
		if (token.type == HTMLTokenizer::TokenType::StartTag) {
			std::string tag_lower = ToLower(token.name);
			if (tag_lower == "body" || tag_lower == "div" || tag_lower == "main") {
				// Parse this as the main container
				if (!ParseHTMLElement(tokenizer, parsed_root, identifiers, error)) {
					return false;
				}
				break;
			}
		}
		token = tokenizer.NextToken();
	}

	// If no main container found, create default structure
	if (!parsed_root) {
		// Create a default container
		parsed_root = std::make_unique<UIPanel>();
	}

	screen = std::move(root_screen);
	return true;
}

bool HtmlCssParser::ParseFromFiles(std::string_view html_file, std::string_view css_file,
	std::unique_ptr<UIScreen>& screen, HTMLParseError& error) {
	
	std::string html_content, css_content;

	if (!ReadFile(html_file, html_content, error)) {
		return false;
	}

	if (!css_file.empty()) {
		if (!ReadFile(css_file, css_content, error)) {
			return false;
		}
	}

	return Parse(html_content, css_content, screen, error);
}

} // namespace BlackLight::UI
