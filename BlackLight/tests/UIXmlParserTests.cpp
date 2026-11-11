#include "rmlui/UIXmlParser.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using BlackLight::UI::UIElementKind;
using BlackLight::UI::UIParseError;
using BlackLight::UI::UIPanel;
using BlackLight::UI::UIScreen;
using BlackLight::UI::UIText;
using BlackLight::UI::UIXmlParser;

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
		"<Screen><Button /></Screen>",
		"<Screen c:id=\"same\"><Panel c:id=\"same\" /></Screen>",
		"<Screen id=\"one\" c:id=\"two\" />",
		"<Screen><Panel width=\"-1px\" /></Screen>",
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

/// Runs the parser's focused behavior and validation tests.
int main() {
	TestScreenPanelAndTextTree();
	TestDefaultSizingAndExplicitDimensions();
	TestInvalidMarkupPreservesOutput();
	TestSchemaValidation();
	return 0;
}
