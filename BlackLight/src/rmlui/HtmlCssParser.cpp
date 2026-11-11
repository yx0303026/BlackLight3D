#include "rmlui/HtmlCssParser.h"

#include <cstddef>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace BlackLight::UI {

namespace {

/// Records the first parse or validation error.
void SetError(HTMLParseError& error, std::string message, std::size_t line = 0)
{
	error.message = std::move(message);
	error.line = line;
}

/// Collects identifiers in document order and reports the first duplicate.
bool CollectIdentifiers(const HTMLElement& element, std::unordered_set<std::string>& identifiers,
	std::string& duplicate)
{
	const std::string& id = element.GetId();
	if (!id.empty() && !identifiers.insert(id).second) {
		duplicate = id;
		return false;
	}

	if (const auto* container = dynamic_cast<const HTMLContainer*>(&element)) {
		for (const auto& child : container->GetChildren()) {
			if (!CollectIdentifiers(*child, identifiers, duplicate)) {
				return false;
			}
		}
	}

	return true;
}

/// Reads file content into a string.
bool ReadFile(std::string_view file_path, std::string& content, HTMLParseError& error)
{
	std::ifstream file(std::string(file_path), std::ios::binary);
	if (!file.is_open()) {
		SetError(error, std::string("Failed to open file: ") + std::string(file_path));
		return false;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	content = stream.str();
	return true;
}

} // namespace

bool HtmlCssParser::Parse(std::string_view html, std::string_view css,
	std::unique_ptr<HTMLDocument>& document, HTMLParseError& error)
{
	error = {};

	if (html.empty()) {
		SetError(error, "HTML content is empty");
		return false;
	}

	std::unique_ptr<HTMLDocument> parsed_document;
	if (!HTMLParser::Parse(html, css, parsed_document, error)) {
		return false;
	}

	if (!parsed_document) {
		SetError(error, "HTML parser did not produce a document");
		return false;
	}

	std::unordered_set<std::string> identifiers;
	std::string duplicate;
	if (!CollectIdentifiers(*parsed_document, identifiers, duplicate)) {
		SetError(error, "Control identifiers must be unique: " + duplicate);
		return false;
	}

	document = std::move(parsed_document);
	return true;
}

bool HtmlCssParser::ParseFromFiles(std::string_view html_file, std::string_view css_file,
	std::unique_ptr<HTMLDocument>& document, HTMLParseError& error)
{
	error = {};

	std::string html_content;
	if (!ReadFile(html_file, html_content, error)) {
		return false;
	}

	std::string css_content;
	if (!css_file.empty() && !ReadFile(css_file, css_content, error)) {
		return false;
	}

	return Parse(html_content, css_content, document, error);
}

} // namespace BlackLight::UI