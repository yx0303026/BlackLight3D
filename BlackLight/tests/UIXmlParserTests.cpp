#include "rmlui/UIXmlParser.h"
#include "rmlui/HtmlCssParser.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using BlackLight::UI::UIElementKind;
using BlackLight::UI::UIParseError;
using BlackLight::UI::UIPanel;
using BlackLight::UI::UIScreen;
using BlackLight::UI::UIText;
using BlackLight::UI::UIControl;
using BlackLight::UI::UIXmlParser;
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

/// Verifies the requested Screen/Panel syntax, layout flags, and mixed text order.
void TestScreenPanelAndTextTree() {
	const std::string xml = R"xml(
<Screen xmlns:c="urn:blacklight:ui" c:id="screen_id">
  <Panel c:id="panel_id" auto_size="true" width="320">
    Before <Panel c:id="child_id" height="50%" /> After
  </Panel>
</Screen>)xml";
	std::unique_ptr<UIScreen> screen;
	UIParseError error;
	CHECK(UIXmlParser::Parse(xml, screen, error));
	CHECK(error.message.empty());
	CHECK(screen != nullptr);
	CHECK(screen->GetKind() == UIElementKind::Screen);
	CHECK(screen->GetId() == "screen_id");
	CHECK(screen->GetChildren().size() == 1);

	const auto* panel = dynamic_cast<const UIPanel*>(screen->GetChildren()[0].get());
	CHECK(panel != nullptr);
	CHECK(panel->GetId() == "panel_id");
	CHECK(panel->GetLayout().auto_size);
	CHECK(panel->GetLayout().width == "320px");
	CHECK(!panel->GetLayout().height.has_value());
	CHECK(panel->GetChildren().size() == 3);

	const auto* before = dynamic_cast<const UIText*>(panel->GetChildren()[0].get());
	const auto* child_panel = dynamic_cast<const UIPanel*>(panel->GetChildren()[1].get());
	const auto* after = dynamic_cast<const UIText*>(panel->GetChildren()[2].get());
	CHECK(before != nullptr && before->GetText() == "\n    Before ");
	CHECK(child_panel != nullptr && child_panel->GetId() == "child_id");
	CHECK(child_panel->GetLayout().height == "50%");
	CHECK(after != nullptr && after->GetText() == " After\n  ");
}

/// Verifies that omitted auto_size defaults to filling the parent and explicit dimensions survive.
void TestDefaultSizingAndExplicitDimensions() {
	const std::string xml = R"xml(<Screen c:id="root"><Panel c:id="content" width="42.5em" height="auto" /></Screen>)xml";
	std::unique_ptr<UIScreen> screen;
	UIParseError error;
	CHECK(UIXmlParser::Parse(xml, screen, error));
	const auto* panel = dynamic_cast<const UIPanel*>(screen->GetChildren()[0].get());
	CHECK(panel != nullptr);
	CHECK(!panel->GetLayout().auto_size);
	CHECK(panel->GetLayout().width == "42.5em");
	CHECK(panel->GetLayout().height == "auto");
}

/// Verifies layout stylesheets and the added built-in controls are retained in the UI model.
void TestLayoutStylesAndBuiltInControls() {
	const std::string xml = R"xml(
<layout xmlns:c="urn:blacklight:ui">
  <style c:id="xx">.panel-class { color: white; }</style>
  <Screen c:id="root" style="xx:panel-class">
    <Fill c:id="fill" />
    <Border c:id="border" />
    <Button c:id="button" style="xx:panel-class">Go</Button>
    <ListBox c:id="listbox" />
    <ListView c:id="listview" />
    <DataGrid c:id="grid" />
  </Screen>
</layout>)xml";
	std::unique_ptr<UIScreen> screen;
	UIParseError error;
	CHECK(UIXmlParser::Parse(xml, screen, error));
	CHECK(screen->GetStyle() == "xx:panel-class");
	CHECK(screen->GetStyleSheets().size() == 1);
	CHECK(screen->GetStyleSheets()[0].first == "xx");
	CHECK(screen->GetStyleSheets()[0].second.find(".panel-class") != std::string::npos);
	CHECK(screen->GetChildren().size() == 6);
	const UIElementKind expected[] = {
		UIElementKind::Fill, UIElementKind::Border, UIElementKind::Button,
		UIElementKind::ListBox, UIElementKind::ListView, UIElementKind::DataGrid
	};
	for (std::size_t index = 0; index < std::size(expected); ++index) {
		const auto* control = dynamic_cast<const UIControl*>(screen->GetChildren()[index].get());
		CHECK(control != nullptr);
		CHECK(control->GetKind() == expected[index]);
	}
	CHECK(screen->GetChildren()[2]->GetStyle() == "xx:panel-class");
}

/// Verifies invalid markup returns a useful error without replacing the previous tree.
void TestInvalidMarkupPreservesOutput() {
	std::unique_ptr<UIScreen> screen;
	UIParseError error;
	CHECK(UIXmlParser::Parse("<Screen c:id=\"kept\" />", screen, error));
	UIScreen* const original = screen.get();

	CHECK(!UIXmlParser::Parse("<Screen><Panel auto_size=\"sometimes\" /></Screen>", screen, error));
	CHECK(screen.get() == original);
	CHECK(error.message.find("auto_size") != std::string::npos);
	CHECK(error.line == 1);
}

/// Verifies unknown controls, duplicate IDs, conflicting ID aliases, and invalid dimensions fail.
void TestSchemaValidation() {
	const char* invalid_documents[] = {
		"",
		"<Screen>",
		"<Window />",
		"<Screen c:id=\"same\"><Panel c:id=\"same\" /></Screen>",
		"<Screen id=\"one\" c:id=\"two\" />",
		"<Screen><Panel width=\"-1px\" /></Screen>",
		"<layout><style>.x { color: white; }</style><Screen /></layout>",
		"<layout><style c:id=\"x\">.x { color: white; }</style><style c:id=\"x\">.y { color: red; }</style><Screen /></layout>",
		"<layout><style c:id=\"x\">.x { color: white; }</style><Screen style=\"missing:class\" /></layout>",
		"<layout><style c:id=\"x\">.x { color: white; }</style><Screen><Panel style=\"x:two classes\" /></Screen></layout>",
		"<Screen unexpected=\"value\" />"
	};
	for (const char* xml : invalid_documents) {
		std::unique_ptr<UIScreen> screen;
		UIParseError error;
		CHECK(!UIXmlParser::Parse(xml, screen, error));
		CHECK(!error.message.empty());
		CHECK(screen == nullptr);
	}
}

} // namespace

/// ============== HTML+CSS Parser Tests ==============
namespace {

void TestHtmlParseSimpleDiv() {
	std::string html = R"(
		<div id="main" style="width: 100%; height: 200px;">
			Hello World
		</div>
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(result);
	CHECK(error.message.empty());
	CHECK(screen != nullptr);
}

void TestHtmlParseDivWithNestedElements() {
	std::string html = R"(
		<div id="container" style="width: 500px; height: 300px;">
			<div id="child1" style="width: 50%; background-color: #FF0000;">
				Left Panel
			</div>
			<div id="child2" style="width: 50%; background-color: #00FF00;">
				Right Panel
			</div>
		</div>
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(result);
	CHECK(error.message.empty());
	CHECK(screen != nullptr);
}

void TestHtmlParseWithInlineCSS() {
	std::string html = R"(
		<div id="styled_div"
			 style="color: #FFFFFF; font-size: 16px; text-align: center; background-color: #000000;">
			Styled Content
		</div>
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(result);
	CHECK(error.message.empty());
}

void TestHtmlParseButtonElement() {
	std::string html = R"(
		<button id="submit_btn" style="width: 100px; height: 40px; font-size: 14px;">
			Submit
		</button>
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(result);
	CHECK(error.message.empty());
}

void TestHtmlParseFlexLayout() {
	std::string html = R"(
		<div id="flex_container"
			 style="display: flex; flex-direction: row; gap: 10px; width: 100%; height: 100%;">
			<div style="flex: 1; background-color: red;">Item 1</div>
			<div style="flex: 1; background-color: blue;">Item 2</div>
			<div style="flex: 1; background-color: green;">Item 3</div>
		</div>
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(result);
	CHECK(error.message.empty());
}

void TestHtmlParseWithExternalCSS() {
	std::string html = R"(
		<div id="main" class="container">
			Content
		</div>
	)";
	
	std::string css = R"(
		.container {
			width: 100%;
			height: 200px;
			background-color: #F0F0F0;
			padding: 10px;
			border: 1px solid #CCCCCC;
			border-radius: 5px;
		}
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, css, screen, error);
	
	CHECK(result);
	CHECK(error.message.empty());
	CHECK(screen != nullptr);
	
	const auto& stylesheets = screen->GetStyleSheets();
	CHECK(stylesheets.size() == 1);
	CHECK(stylesheets[0].first == "default");
	CHECK(stylesheets[0].second.find("container") != std::string::npos);
}

void TestHtmlParseEmptyHtmlFails() {
	std::string html = "";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(!result);
	CHECK(!error.message.empty());
}

void TestHtmlDuplicateIdDetection() {
	std::string html = R"(
		<div id="duplicate" style="width: 100px;">
			First
		</div>
		<div id="duplicate" style="width: 100px;">
			Second
		</div>
	)";
	
	std::unique_ptr<UIScreen> screen;
	HTMLParseError error;
	bool result = HtmlCssParser::Parse(html, "", screen, error);
	
	CHECK(!result);
	CHECK(error.message.find("unique") != std::string::npos);
}

} // namespace

/// Runs the parser's focused behavior and validation tests.
int main() {
	TestScreenPanelAndTextTree();
	TestDefaultSizingAndExplicitDimensions();
	TestLayoutStylesAndBuiltInControls();
	TestInvalidMarkupPreservesOutput();
	TestSchemaValidation();
	
	// HTML+CSS Parser Tests
	TestHtmlParseSimpleDiv();
	TestHtmlParseDivWithNestedElements();
	TestHtmlParseWithInlineCSS();
	TestHtmlParseButtonElement();
	TestHtmlParseFlexLayout();
	TestHtmlParseWithExternalCSS();
	TestHtmlParseEmptyHtmlFails();
	TestHtmlDuplicateIdDetection();
	
	return 0;
}
