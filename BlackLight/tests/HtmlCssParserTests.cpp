#include "rmlui/HtmlCssParser.h"
#include "rmlui/HTMLElement.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

using BlackLight::UI::HTMLContainer;
using BlackLight::UI::HTMLDocument;
using BlackLight::UI::HTMLElement;
using BlackLight::UI::HTMLElementKind;
using BlackLight::UI::HTMLParseError;
using BlackLight::UI::HtmlCssParser;

namespace {

/// Terminates the test process with the failed expression and source line.
void Check(bool condition, const char* expression, int line) {
	if (!condition) {
		std::fprintf(stderr, "Check failed at line %d: %s\n", line, expression);
		std::abort();
	}
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

/// Verifies that a parsed document owns the nested element tree.
void TestParseNestedElements() {
	const std::string html = R"html(
		<div id="container" class="panel">
			<div id="left" class="pane">Left</div>
			<div id="right" class="pane">Right</div>
		</div>
	)html";

	std::unique_ptr<HTMLDocument> document;
	HTMLParseError error;
	CHECK(HtmlCssParser::Parse(html, "", document, error));
	CHECK(error.message.empty());
	CHECK(document != nullptr);

	HTMLElement* const container = document->FindById("container");
	CHECK(container != nullptr);
	CHECK(container->GetKind() == HTMLElementKind::Container);
	CHECK(document->FindById("left") != nullptr);
	CHECK(document->FindById("right") != nullptr);
	CHECK(document->FindByClass("pane").size() == 2);
}

/// Verifies that inline styles are normalized to RmlUi-compatible values.
void TestInlineStyleNormalization() {
	const std::string html =
		R"html(<div id="box" style="width: 100; height: 50%; background: #FF0000">Text</div>)html";

	std::unique_ptr<HTMLDocument> document;
	HTMLParseError error;
	CHECK(HtmlCssParser::Parse(html, "", document, error));

	HTMLElement* const box = document->FindById("box");
	CHECK(box != nullptr);
	CHECK(box->GetStyle() == "width: 100px; height: 50%; background-color: #FF0000");
	CHECK(box->GetAttribute("style").empty());
}

/// Verifies that standard controls keep their dedicated element kinds.
void TestControlKinds() {
	const std::string html = R"html(
		<div>
			<button id="ok">OK</button>
			<input id="name" type="text" />
			<select id="role"><option>Admin</option></select>
			<textarea id="bio"></textarea>
			<table id="grid"><tr><td>Cell</td></tr></table>
		</div>
	)html";

	std::unique_ptr<HTMLDocument> document;
	HTMLParseError error;
	CHECK(HtmlCssParser::Parse(html, "", document, error));

	CHECK(document->FindById("ok")->GetKind() == HTMLElementKind::Button);
	CHECK(document->FindById("name")->GetKind() == HTMLElementKind::Input);
	CHECK(document->FindById("role")->GetKind() == HTMLElementKind::Select);
	CHECK(document->FindById("bio")->GetKind() == HTMLElementKind::TextArea);
	CHECK(document->FindById("grid")->GetKind() == HTMLElementKind::Table);
}

/// Verifies that an external stylesheet is attached to the parsed document.
void TestExternalStyleSheet() {
	const std::string html = R"html(<div id="main" class="container">Content</div>)html";
	const std::string css = ".container { width: 100%; height: 200px; }";

	std::unique_ptr<HTMLDocument> document;
	HTMLParseError error;
	CHECK(HtmlCssParser::Parse(html, css, document, error));

	const auto& style_sheets = document->GetStyleSheets();
	CHECK(style_sheets.size() == 1);
	CHECK(style_sheets[0].find("container") != std::string::npos);
}

/// Verifies that empty input and duplicate identifiers are rejected without replacing output.
void TestRejectsInvalidInput() {
	std::unique_ptr<HTMLDocument> document;
	HTMLParseError error;

	CHECK(!HtmlCssParser::Parse("", "", document, error));
	CHECK(!error.message.empty());
	CHECK(document == nullptr);

	const std::string duplicate_html =
		R"html(<div id="same"><div id="same"></div></div>)html";
	CHECK(!HtmlCssParser::Parse(duplicate_html, "", document, error));
	CHECK(error.message.find("unique") != std::string::npos);
	CHECK(document == nullptr);

	const std::string valid_html = R"html(<div id="kept"></div>)html";
	CHECK(HtmlCssParser::Parse(valid_html, "", document, error));
	HTMLDocument* const original = document.get();
	CHECK(!HtmlCssParser::Parse(duplicate_html, "", document, error));
	CHECK(document.get() == original);
}

/// Verifies the file-based entry point reads HTML and CSS from disk.
void TestParseFromFiles() {
	const std::string html_path = "blacklight_htmlcssparser_test.html";
	const std::string css_path = "blacklight_htmlcssparser_test.css";

	{
		std::ofstream html_file(html_path, std::ios::binary);
		html_file << R"html(<div id="from_file" style="width: 10">File</div>)html";
	}
	{
		std::ofstream css_file(css_path, std::ios::binary);
		css_file << "#from_file { color: #FFFFFF; }";
	}

	std::unique_ptr<HTMLDocument> document;
	HTMLParseError error;
	const bool parsed = HtmlCssParser::ParseFromFiles(html_path, css_path, document, error);

	std::remove(html_path.c_str());
	std::remove(css_path.c_str());

	CHECK(parsed);
	CHECK(document != nullptr);
	CHECK(document->FindById("from_file") != nullptr);
	CHECK(document->FindById("from_file")->GetStyle() == "width: 10px");
	CHECK(document->GetStyleSheets().size() == 1);
}

} // namespace

int main() {
	TestParseNestedElements();
	TestInlineStyleNormalization();
	TestControlKinds();
	TestExternalStyleSheet();
	TestRejectsInvalidInput();
	TestParseFromFiles();

	std::printf("HtmlCssParser tests passed\n");
	return 0;
}
