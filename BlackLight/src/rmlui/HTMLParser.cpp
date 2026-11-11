#include "rmlui/HTMLParser.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BlackLight::UI {

namespace {

bool IsSpace(char value)
{
	return std::isspace(static_cast<unsigned char>(value)) != 0;
}

std::string_view Trim(std::string_view value)
{
	while (!value.empty() && IsSpace(value.front())) {
		value.remove_prefix(1);
	}
	while (!value.empty() && IsSpace(value.back())) {
		value.remove_suffix(1);
	}
	return value;
}

std::string ToLower(std::string_view value)
{
	std::string result(value);
	for (char& character : result) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	return result;
}

std::size_t GetLineNumber(std::string_view html, std::size_t position)
{
	std::size_t line = 1;
	for (std::size_t index = 0; index < position && index < html.size(); ++index) {
		if (html[index] == '\n') {
			++line;
		}
	}
	return line;
}

bool IsVoidTag(std::string_view tag_name)
{
	return tag_name == "area" || tag_name == "base" || tag_name == "br" || tag_name == "col"
		|| tag_name == "embed" || tag_name == "hr" || tag_name == "img" || tag_name == "input"
		|| tag_name == "link" || tag_name == "meta" || tag_name == "param" || tag_name == "source"
		|| tag_name == "track" || tag_name == "wbr";
}

std::string_view ExtractTagName(std::string_view tag)
{
	std::size_t end = 0;
	while (end < tag.size() && !IsSpace(tag[end]) && tag[end] != '/' && tag[end] != '>') {
		++end;
	}
	return tag.substr(0, end);
}

std::unordered_map<std::string, std::string> ParseAttributes(std::string_view text)
{
	std::unordered_map<std::string, std::string> attributes;

	std::size_t position = 0;
	while (position < text.size()) {
		while (position < text.size() && IsSpace(text[position])) {
			++position;
		}
		if (position >= text.size() || text[position] == '/') {
			break;
		}

		const std::size_t name_start = position;
		while (position < text.size() && !IsSpace(text[position]) && text[position] != '='
			&& text[position] != '/' && text[position] != '>') {
			++position;
		}

		if (position == name_start) {
			++position;
			continue;
		}

		std::string name = ToLower(text.substr(name_start, position - name_start));

		while (position < text.size() && IsSpace(text[position])) {
			++position;
		}

		std::string value;
		if (position < text.size() && text[position] == '=') {
			++position;
			while (position < text.size() && IsSpace(text[position])) {
				++position;
			}

			if (position < text.size() && (text[position] == '"' || text[position] == '\'')) {
				const char quote = text[position++];
				const std::size_t value_start = position;
				while (position < text.size() && text[position] != quote) {
					++position;
				}
				value.assign(text.substr(value_start, position - value_start));
				if (position < text.size()) {
					++position;
				}
			} else {
				const std::size_t value_start = position;
				while (position < text.size() && !IsSpace(text[position]) && text[position] != '>') {
					++position;
				}
				value.assign(text.substr(value_start, position - value_start));
			}
		}

		attributes[std::move(name)] = std::move(value);
	}

	return attributes;
}

HTMLElementKind GetKindForTag(std::string_view tag_name)
{
	if (tag_name == "button") {
		return HTMLElementKind::Button;
	}
	if (tag_name == "input") {
		return HTMLElementKind::Input;
	}
	if (tag_name == "select") {
		return HTMLElementKind::Select;
	}
	if (tag_name == "textarea") {
		return HTMLElementKind::TextArea;
	}
	if (tag_name == "label") {
		return HTMLElementKind::Label;
	}
	if (tag_name == "a") {
		return HTMLElementKind::Link;
	}
	if (tag_name == "img") {
		return HTMLElementKind::Image;
	}
	if (tag_name == "ul" || tag_name == "ol") {
		return HTMLElementKind::List;
	}
	if (tag_name == "li") {
		return HTMLElementKind::ListItem;
	}
	if (tag_name == "table") {
		return HTMLElementKind::Table;
	}
	if (tag_name == "tr") {
		return HTMLElementKind::TableRow;
	}
	if (tag_name == "td" || tag_name == "th") {
		return HTMLElementKind::TableCell;
	}
	return HTMLElementKind::Container;
}

bool IsDimensionProperty(const std::string& property)
{
	static const std::string_view kDimensionProperties[] = {
		"width", "height", "min-width", "max-width", "min-height", "max-height",
		"top", "left", "right", "bottom", "font-size", "gap", "row-gap", "column-gap",
		"padding", "margin", "border-radius", "border-width", "letter-spacing", "text-indent"
	};
	return std::find(std::begin(kDimensionProperties), std::end(kDimensionProperties), property)
		!= std::end(kDimensionProperties);
}

std::string NormalizeDimension(std::string_view value)
{
	if (value.empty() || value == "auto" || value == "inherit" || value == "initial") {
		return std::string(value);
	}

	std::size_t index = 0;
	if (value.front() == '-' || value.front() == '+') {
		++index;
	}

	bool has_number = false;
	while (index < value.size() && (std::isdigit(static_cast<unsigned char>(value[index])) || value[index] == '.')) {
		has_number = true;
		++index;
	}

	if (!has_number) {
		return std::string(value);
	}

	const std::string_view unit = value.substr(index);
	if (!unit.empty()) {
		return std::string(value);
	}

	return std::string(value) + "px";
}

std::string NormalizeDimensions(std::string_view value)
{
	std::istringstream stream{std::string(value)};
	std::string token;
	std::string result;
	while (stream >> token) {
		if (!result.empty()) {
			result.push_back(' ');
		}
		result += NormalizeDimension(token);
	}
	return result;
}

std::string NormalizeStyle(std::string_view style)
{
	std::string result;

	std::size_t position = 0;
	while (position <= style.size()) {
		const std::size_t end = style.find(';', position);
		const std::string_view declaration = Trim(style.substr(position,
			end == std::string_view::npos ? std::string_view::npos : end - position));
		position = end == std::string_view::npos ? style.size() + 1 : end + 1;

		const std::size_t colon = declaration.find(':');
		if (colon == std::string_view::npos) {
			continue;
		}

		std::string property = ToLower(Trim(declaration.substr(0, colon)));
		if (property.empty()) {
			continue;
		}
		if (property == "background" || property == "bg") {
			property = "background-color";
		}

		const std::string_view value = Trim(declaration.substr(colon + 1));
		if (value.empty()) {
			continue;
		}

		if (!result.empty()) {
			result += "; ";
		}
		result += property;
		result += ": ";
		result += IsDimensionProperty(property) ? NormalizeDimensions(value) : std::string(value);
	}

	return result;
}

HTMLElementAttributes CreateAttributes(const std::string& tag_name,
	const std::unordered_map<std::string, std::string>& parsed_attributes)
{
	HTMLElementAttributes attributes;
	attributes.tag_name = tag_name;

	if (const auto id = parsed_attributes.find("id"); id != parsed_attributes.end()) {
		attributes.id = id->second;
	}

	if (const auto classes = parsed_attributes.find("class"); classes != parsed_attributes.end()) {
		std::istringstream stream(classes->second);
		std::string css_class;
		while (stream >> css_class) {
			attributes.classes.push_back(std::move(css_class));
		}
	}

	if (const auto style = parsed_attributes.find("style"); style != parsed_attributes.end()) {
		attributes.style = NormalizeStyle(style->second);
	}

	for (const auto& [name, value] : parsed_attributes) {
		if (name != "id" && name != "class" && name != "style") {
			attributes.custom_attributes[name] = value;
		}
	}

	return attributes;
}

class ParsedContainer final : public HTMLContainer
{
public:
	ParsedContainer(HTMLElementKind kind, const HTMLElementAttributes& attributes)
		: HTMLContainer(kind, attributes)
	{
	}
};

std::unique_ptr<HTMLElement> CreateElement(std::string_view tag_name,
	const std::unordered_map<std::string, std::string>& parsed_attributes)
{
	const std::string lower_tag = ToLower(tag_name);
	return std::make_unique<ParsedContainer>(
		GetKindForTag(lower_tag), CreateAttributes(lower_tag, parsed_attributes));
}

void AddTextNode(HTMLContainer& parent, std::string_view text, HTMLParseError& error)
{
	const std::string_view trimmed = Trim(text);
	if (trimmed.empty()) {
		return;
	}

	auto text_node = std::make_unique<HTMLText>(std::string(trimmed));
	if (!parent.AddChild(std::move(text_node))) {
		error.message = "Failed to append text node";
	}
}

bool IsCommentStart(std::string_view html, std::size_t position)
{
	return html.compare(position, 4, "<!--") == 0;
}

} // namespace

bool HTMLParser::Parse(std::string_view html, std::string_view css,
	std::unique_ptr<HTMLDocument>& document, HTMLParseError& error)
{
	try {
		auto doc = std::make_unique<HTMLDocument>();
		if (!css.empty()) {
			doc->AddStyleSheet(std::string(css));
		}

		std::vector<HTMLContainer*> tag_stack;
		tag_stack.push_back(doc.get());

		std::size_t position = 0;
		while (position < html.size()) {
			const std::size_t tag_start = html.find('<', position);
			if (tag_start == std::string_view::npos) {
				AddTextNode(*tag_stack.back(), html.substr(position), error);
				break;
			}

			if (tag_start > position) {
				AddTextNode(*tag_stack.back(), html.substr(position, tag_start - position), error);
			}

			if (IsCommentStart(html, tag_start)) {
				const std::size_t comment_end = html.find("-->", tag_start + 4);
				if (comment_end == std::string_view::npos) {
					error.message = "Unterminated HTML comment";
					error.line = GetLineNumber(html, tag_start);
					return false;
				}
				position = comment_end + 3;
				continue;
			}

			const std::size_t tag_end = html.find('>', tag_start + 1);
			if (tag_end == std::string_view::npos) {
				error.message = "Unterminated tag";
				error.line = GetLineNumber(html, tag_start);
				return false;
			}

			std::string_view tag = html.substr(tag_start + 1, tag_end - tag_start - 1);
			position = tag_end + 1;

			if (tag.empty() || tag.front() == '!' || tag.front() == '?') {
				continue;
			}

			if (tag.front() == '/') {
				const std::string closing_name = ToLower(ExtractTagName(Trim(tag.substr(1))));
				if (closing_name.empty()) {
					continue;
				}

				for (std::size_t index = tag_stack.size(); index-- > 1;) {
					if (tag_stack[index]->GetTagName() == closing_name) {
						tag_stack.resize(index);
						break;
					}
				}
				continue;
			}

			tag = Trim(tag);
			bool self_closing = false;
			if (!tag.empty() && tag.back() == '/') {
				self_closing = true;
				tag = Trim(tag.substr(0, tag.size() - 1));
			}

			const std::string_view tag_name = ExtractTagName(tag);
			if (tag_name.empty()) {
				continue;
			}

			const std::string_view attribute_text = tag.substr(tag_name.size());
			const std::string lower_tag = ToLower(tag_name);
			const auto parsed_attributes = ParseAttributes(attribute_text);

			auto element = CreateElement(lower_tag, parsed_attributes);
			HTMLElement* const element_ptr = element.get();

			if (!tag_stack.back()->AddChild(std::move(element))) {
				error.message = "Failed to append element: " + lower_tag;
				error.line = GetLineNumber(html, tag_start);
				return false;
			}

			if (!self_closing && !IsVoidTag(lower_tag)) {
				if (auto* container = dynamic_cast<HTMLContainer*>(element_ptr)) {
					tag_stack.push_back(container);
				}
			}
		}

		document = std::move(doc);
		return true;
	} catch (const std::exception& exception) {
		error.message = std::string("Parse error: ") + exception.what();
		return false;
	}
}

bool HTMLParser::ParseWithCss(std::string_view html, std::string_view css,
	std::unique_ptr<HTMLDocument>& document, HTMLParseError& error)
{
	return Parse(html, css, document, error);
}

bool HTMLParser::Validate(std::string_view html, HTMLParseError& error)
{
	try {
		std::vector<std::string> tag_stack;

		std::size_t position = 0;
		while (position < html.size()) {
			const std::size_t tag_start = html.find('<', position);
			if (tag_start == std::string_view::npos) {
				break;
			}

			if (IsCommentStart(html, tag_start)) {
				const std::size_t comment_end = html.find("-->", tag_start + 4);
				if (comment_end == std::string_view::npos) {
					error.message = "Unterminated HTML comment";
					error.line = GetLineNumber(html, tag_start);
					return false;
				}
				position = comment_end + 3;
				continue;
			}

			const std::size_t tag_end = html.find('>', tag_start + 1);
			if (tag_end == std::string_view::npos) {
				error.message = "Unterminated tag";
				error.line = GetLineNumber(html, tag_start);
				return false;
			}

			std::string_view tag = html.substr(tag_start + 1, tag_end - tag_start - 1);
			position = tag_end + 1;

			if (tag.empty() || tag.front() == '!' || tag.front() == '?') {
				continue;
			}

			if (tag.front() == '/') {
				const std::string closing_name = ToLower(ExtractTagName(Trim(tag.substr(1))));
				if (closing_name.empty()) {
					continue;
				}
				if (tag_stack.empty()) {
					error.message = "Unmatched closing tag: </" + closing_name + ">";
					error.line = GetLineNumber(html, tag_start);
					return false;
				}
				if (tag_stack.back() != closing_name) {
					error.message = "Tag mismatch: expected </" + tag_stack.back() + "> but found </"
						+ closing_name + ">";
					error.line = GetLineNumber(html, tag_start);
					return false;
				}
				tag_stack.pop_back();
				continue;
			}

			tag = Trim(tag);
			bool self_closing = false;
			if (!tag.empty() && tag.back() == '/') {
				self_closing = true;
				tag = Trim(tag.substr(0, tag.size() - 1));
			}

			const std::string tag_name = ToLower(ExtractTagName(tag));
			if (tag_name.empty() || self_closing || IsVoidTag(tag_name)) {
				continue;
			}

			tag_stack.push_back(tag_name);
		}

		if (!tag_stack.empty()) {
			error.message = "Unclosed tag: <" + tag_stack.back() + ">";
			return false;
		}

		return true;
	} catch (const std::exception& exception) {
		error.message = std::string("Validation error: ") + exception.what();
		return false;
	}
}

} // namespace BlackLight::UI