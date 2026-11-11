#include "rmlui/HTMLParser.h"
#include "rmlui/HTMLElement.h"

#include <cctype>
#include <sstream>
#include <regex>
#include <algorithm>
#include <set>

namespace BlackLight::UI {
namespace {

std::string Trim(const std::string& str) {
	auto start = str.begin();
	while (start != str.end() && std::isspace(*start)) {
		start++;
	}

	auto end = str.end();
	do {
		end--;
	} while (std::distance(start, end) > 0 && std::isspace(*end));

	return std::string(start, end + 1);
}

bool IsSelfClosingTag(const std::string& tag) {
	static const std::set<std::string> self_closing = {
		"area", "base", "br", "col", "embed", "hr", "img", "input",
		"link", "meta", "param", "source", "track", "wbr"
	};
	return self_closing.find(tag) != self_closing.end();
}

std::string ExtractTagName(const std::string& tag_str) {
	auto end = tag_str.find_first_of(" \t\n\r>");
	if (end == std::string::npos) {
		return tag_str;
	}
	return tag_str.substr(0, end);
}

std::unordered_map<std::string, std::string> ParseAttributes(const std::string& tag_str) {
	std::unordered_map<std::string, std::string> attributes;
	
	auto attr_start = tag_str.find_first_of(" \t\n\r");
	if (attr_start == std::string::npos) {
		return attributes;
	}

	std::string attr_str = tag_str.substr(attr_start);
	std::regex attr_regex(R"((\w+)(?:="([^"]*)"|='([^']*)')?)", std::regex::icase);
	
	std::sregex_iterator it(attr_str.begin(), attr_str.end(), attr_regex);
	std::sregex_iterator end;

	for (; it != end; ++it) {
		std::string name = it->str(1);
		std::string value = it->str(2).empty() ? it->str(3) : it->str(2);
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);
		attributes[name] = value;
	}

	return attributes;
}

std::unique_ptr<HTMLElement> CreateElementFromTag(const std::string& tag_name,
	const std::unordered_map<std::string, std::string>& attributes)
{
	std::string lower_tag = tag_name;
	std::transform(lower_tag.begin(), lower_tag.end(), lower_tag.begin(), ::tolower);

	std::unique_ptr<HTMLElement> element;

	if (lower_tag == "button") {
		element = std::make_unique<HTMLContainer>(lower_tag, HTMLElementKind::Container);
	} else if (lower_tag == "input") {
		element = std::make_unique<HTMLInput>();
	} else if (lower_tag == "textarea") {
		element = std::make_unique<HTMLTextarea>();
	} else if (lower_tag == "select") {
		element = std::make_unique<HTMLSelect>();
	} else if (lower_tag == "table") {
		element = std::make_unique<HTMLTable>();
	} else if (lower_tag == "tr") {
		element = std::make_unique<HTMLTableRow>();
	} else if (lower_tag == "th" || lower_tag == "td") {
		element = std::make_unique<HTMLTableCell>(lower_tag == "th");
	} else if (lower_tag == "form") {
		element = std::make_unique<HTMLForm>();
	} else {
		element = std::make_unique<HTMLContainer>(lower_tag, HTMLElementKind::Container);
	}

	auto id_it = attributes.find("id");
	if (id_it != attributes.end()) {
		element->SetId(id_it->second);
	}

	auto class_it = attributes.find("class");
	if (class_it != attributes.end()) {
		std::istringstream iss(class_it->second);
		std::string css_class;
		while (iss >> css_class) {
			element->AddClass(css_class);
		}
	}

	auto style_it = attributes.find("style");
	if (style_it != attributes.end()) {
		element->SetStyle(style_it->second);
	}

	return element;
}

}

bool HTMLParser::Parse(const std::string& html, const std::string& base_css,
	std::unique_ptr<HTMLDocument>& document, HTMLParseError& error)
{
	try {
		auto doc = std::make_unique<HTMLDocument>();

		if (!base_css.empty()) {
			doc->AddStyleSheet(base_css);
		}

		std::regex tag_regex(R"(<(/?)(\w+)([^>]*)>)");
		std::sregex_iterator it(html.begin(), html.end(), tag_regex);
		std::sregex_iterator end;

		std::vector<HTMLContainer*> tag_stack;
		tag_stack.push_back(doc.get());

		std::size_t last_pos = 0;

		while (it != end) {
			if (it->position() > last_pos) {
				std::string text = html.substr(last_pos, it->position() - last_pos);
				text = Trim(text);
				if (!text.empty()) {
					if (auto* container = dynamic_cast<HTMLContainer*>(tag_stack.back())) {
						auto text_elem = std::make_unique<HTMLText>(text);
						container->AddChild(std::move(text_elem));
					}
				}
			}

			bool is_closing = !it->str(1).empty();
			std::string tag_name = it->str(2);
			std::string tag_content = it->str(3);

			if (is_closing) {
				if (tag_stack.size() > 1) {
					tag_stack.pop_back();
				}
			} else {
				auto attributes = ParseAttributes(tag_name + " " + tag_content);
				auto element = CreateElementFromTag(tag_name, attributes);

				if (auto* container = tag_stack.back()) {
					HTMLElement* raw_ptr = element.get();
					container->AddChild(std::move(element));

					if (!IsSelfClosingTag(tag_name)) {
						if (auto* new_container = dynamic_cast<HTMLContainer*>(raw_ptr)) {
							tag_stack.push_back(new_container);
						}
					}
				}
			}

			last_pos = it->position() + it->length();
			++it;
		}

		if (last_pos < html.size()) {
			std::string text = html.substr(last_pos);
			text = Trim(text);
			if (!text.empty()) {
				if (auto* container = dynamic_cast<HTMLContainer*>(tag_stack.back())) {
					auto text_elem = std::make_unique<HTMLText>(text);
					container->AddChild(std::move(text_elem));
				}
			}
		}

		document = std::move(doc);
		return true;
	} catch (const std::exception& e) {
		error.message = std::string("Parse error: ") + e.what();
		return false;
	}
}

bool HTMLParser::ParseWithCss(const std::string& html, const std::string& css,
	std::unique_ptr<HTMLDocument>& document, HTMLParseError& error)
{
	if (!Parse(html, css, document, error)) {
		return false;
	}

	if (!css.empty() && document) {
		document->AddStyleSheet(css);
	}

	return true;
}

bool HTMLParser::Validate(const std::string& html, HTMLParseError& error) {
	try {
		std::regex tag_regex(R"(<(/?)(\w+)([^>]*)>)");
		std::sregex_iterator it(html.begin(), html.end(), tag_regex);
		std::sregex_iterator end;

		std::vector<std::string> tag_stack;

		while (it != end) {
			bool is_closing = !it->str(1).empty();
			std::string tag_name = it->str(2);
			std::transform(tag_name.begin(), tag_name.end(), tag_name.begin(), ::tolower);

			if (is_closing) {
				if (tag_stack.empty()) {
					error.message = "Unmatched closing tag: </" + tag_name + ">";
					return false;
				}
				if (tag_stack.back() != tag_name) {
					error.message = "Tag mismatch";
					return false;
				}
				tag_stack.pop_back();
			} else {
				if (!IsSelfClosingTag(tag_name)) {
					tag_stack.push_back(tag_name);
				}
			}

			++it;
		}

		if (!tag_stack.empty()) {
			error.message = "Unclosed tag: <" + tag_stack.back() + ">";
			return false;
		}

		return true;
	} catch (const std::exception& e) {
		error.message = std::string("Validation error: ") + e.what();
		return false;
	}
}

}
