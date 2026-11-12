#include "editor/uidoc/BuiDocumentModel.h"
#include "editor/uidoc/BuiStyleDeclarations.h"
#include "editor/uidoc/BuiStyleSheet.h"
#include "editor/uidoc/UiDocumentSessions.h"
#include "editor/ui/BuiGizmoMath.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

using namespace BlackLight::Editor;

namespace
{

	/// Terminates the test process with the failed expression and source line.
	void Check(bool condition, const char *expression, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "Check failed at line %d: %s\n", line, expression);
			std::abort();
		}
	}

#define CHECK(expression) Check((expression), #expression, __LINE__)

	/// Creates an isolated directory that removes itself after the test.
	class TempDirectory final
	{
	public:
		TempDirectory()
		{
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			path_ = std::filesystem::temp_directory_path() /
				("blacklight_uidoc_tests_" + std::to_string(stamp));
			std::filesystem::create_directories(path_);
		}
		~TempDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(path_, error);
		}
		const std::filesystem::path &GetPath() const noexcept { return path_; }

	private:
		std::filesystem::path path_;
	};

	/// Writes exact UTF-8 bytes to a fixture.
	void WriteFile(const std::filesystem::path &path, const std::string &text)
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		CHECK(output.good());
		output << text;
		CHECK(output.good());
	}

	/// Reads a fixture without newline translation.
	std::string ReadFile(const std::filesystem::path &path)
	{
		std::ifstream input(path, std::ios::binary);
		CHECK(input.good());
		std::ostringstream stream;
		stream << input.rdbuf();
		return stream.str();
	}

	const char *SampleDocument()
	{
		return
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<bui>\n"
			"  <styles>\n"
			"    .main-screen { width: 100%; }\n"
			"    /* untouched comment */\n"
			"  </styles>\n"
			"  <layout>\n"
			"    <div id=\"root\" class=\"main-screen\">\n"
			"      <!-- greeting -->\n"
			"      <button id=\"start\" class=\"primary\">Hello &amp; Light</button>\n"
			"      <br/>\n"
			"      <span>Bye</span>\n"
			"    </div>\n"
			"  </layout>\n"
			"</bui>\n";
	}

	/// Returns the parsed document used by the edit tests.
	BuiDocumentModel ParseSample()
	{
		BuiDocumentModel model;
		BuiParseError warning;
		CHECK(BuiDocumentModel::Parse(SampleDocument(), model, warning));
		return model;
	}

	/// Prints a compact tree for diagnosing parser regressions.
	void DumpTree(const BuiDocumentModel &model, BuiNodeId id, int depth)
	{
		const BuiNode *node = model.Find(id);
		if (!node)
			return;
		std::fprintf(stderr, "%*s%d %s %s\n", depth * 2, "", id,
			node->type == BuiNodeType::Element ? "E" : "T", node->tag.c_str());
		for (const BuiNodeId child : node->children)
			DumpTree(model, child, depth + 1);
	}

	void TestParseAndRoundTrip()
	{
		BuiDocumentModel model;
		BuiParseError warning;
		CHECK(BuiDocumentModel::Parse(SampleDocument(), model, warning));
		if (!warning.message.empty())
			std::fprintf(stderr, "Unexpected parse warning: %s (line %zu)\n",
				warning.message.c_str(), warning.line);
		CHECK(warning.message.empty());
		CHECK(model.GetText() == SampleDocument());
		CHECK(model.GetStyles().find(".main-screen") != std::string::npos);
		CHECK(model.GetLayout().find("<button") != std::string::npos);
		CHECK(model.GetRoot() != InvalidBuiNode);

		const BuiNodeId root = model.NodeAtPath("0");
		CHECK(root != InvalidBuiNode);
		CHECK(model.Find(root)->tag == "div");
		CHECK(model.NodeAtPath("0/0") != InvalidBuiNode);
		CHECK(model.Find(model.NodeAtPath("0/0"))->tag == "button");
		CHECK(model.Find(model.NodeAtPath("0/0"))->children.size() == 1);
		const BuiNode *text = model.Find(model.Find(model.NodeAtPath("0/0"))->children[0]);
		CHECK(text && text->type == BuiNodeType::Text);
		CHECK(text->text == "Hello & Light");
		CHECK(model.Find(model.NodeAtPath("0/1"))->self_closing);
		CHECK(model.NodeAtPath(model.PathOf(model.NodeAtPath("0/0"))) ==
			model.NodeAtPath("0/0"));

		BuiDocumentModel crlf;
		CHECK(BuiDocumentModel::Parse(
			"<bui>\r\n<styles>a{}</styles>\r\n<layout><div></div></layout>\r\n</bui>\r\n",
			crlf, warning));
		CHECK(crlf.UsesCrlf());
	}

	void TestTolerantParsing()
	{
		BuiDocumentModel model;
		BuiParseError warning;
		const std::string malformed =
			"<bui><styles>a { color: red; }</styles><layout>"
			"<!DOCTYPE html><div a=1 b='2'><style>div > span { }</style>"
			"<span>text</span></div></layout></bui>";
		CHECK(BuiDocumentModel::Parse(malformed, model, warning));
		CHECK(model.GetText() == malformed);
		const BuiNode *root = model.Find(model.NodeAtPath("0"));
		CHECK(root && root->tag == "div");
		CHECK(root->FindAttribute("a") != nullptr);
		CHECK(root->FindAttribute("b")->value == "2");

		BuiDocumentModel unclosed;
		CHECK(BuiDocumentModel::Parse(
			"<bui><styles>x</styles><layout><div><span>text</span></layout></bui>",
			unclosed, warning));
		CHECK(!warning.message.empty());

		BuiDocumentModel comments;
		CHECK(BuiDocumentModel::Parse(
			"<bui><styles>x</styles><layout><!-- c --><div></div></layout></bui>",
			comments, warning));
		CHECK(comments.Find(comments.NodeAtPath("0"))->tag == "div");
	}

	void TestAttributeEditing()
	{
		BuiDocumentModel model = ParseSample();
		const BuiNodeId button = model.NodeAtPath("0/0");
		const std::string before = model.GetText();
		BuiEditResult result;
		CHECK(model.SetAttribute(button, "id", "begin", &result));
		CHECK(model.GetText().find("<button id=\"begin\" class=\"primary\">") !=
			std::string::npos);
		CHECK(model.GetText().substr(0, before.find("<button")) ==
			before.substr(0, before.find("<button")));
		CHECK(model.GetText().find("Hello &amp; Light") != std::string::npos);
		CHECK(result.old_text == before);
		CHECK(result.new_text == model.GetText());

		CHECK(model.SetAttribute(button, "title", "Start & go"));
		const BuiNode *updated = model.Find(model.NodeAtPath("0/0"));
		CHECK(updated && updated->FindAttribute("title") &&
			updated->FindAttribute("title")->value == "Start & go");

		CHECK(model.RemoveAttribute(model.NodeAtPath("0/0"), "title"));
		CHECK(model.Find(model.NodeAtPath("0/0"))->FindAttribute("title") == nullptr);

		BuiDocumentModel reloaded;
		BuiParseError warning;
		CHECK(BuiDocumentModel::Parse(model.GetText(), reloaded, warning));
	}

	void TestTextEditing()
	{
		BuiDocumentModel model = ParseSample();
		const BuiNodeId button = model.NodeAtPath("0/0");
		const BuiNodeId text = model.Find(button)->children[0];
		CHECK(model.SetText(text, "Start & <Go>"));
		CHECK(model.GetText().find("Start &amp; &lt;Go&gt;") != std::string::npos);
		const BuiDocumentModel reparsed_check = [&]()
		{
			BuiDocumentModel reparsed;
			BuiParseError warning;
			CHECK(BuiDocumentModel::Parse(model.GetText(), reparsed, warning));
			return reparsed;
		}();
		const BuiNode *node = reparsed_check.Find(reparsed_check.NodeAtPath("0/0"));
		CHECK(node && node->children.size() == 1);
		CHECK(reparsed_check.Find(node->children[0])->text == "Start & <Go>");
	}

	void TestStructureEditing()
	{
		BuiDocumentModel model = ParseSample();
		const BuiNodeId root = model.NodeAtPath("0");
		BuiNodeTemplate container;
		container.tag = "div";
		container.attributes = {{"id", "details"}};
		BuiEditResult result;
		CHECK(model.InsertNode(root, 1, container, &result));
		CHECK(!result.new_selection_path.empty());
		CHECK(model.Find(model.NodeAtPath(result.new_selection_path))->tag == "div");

		BuiNodeTemplate text;
		text.type = BuiNodeType::Text;
		text.text = "Status & ready";
		// Exercise text insertion at an existing sibling boundary.
		CHECK(model.InsertNode(root, 2, text));
		const BuiNode *root_node = model.Find(root);
		CHECK(root_node && root_node->children.size() >= 3);
		BuiNodeId span = InvalidBuiNode;
		for (const BuiNodeId child : root_node->children)
		{
			const BuiNode *node = model.Find(child);
			if (node && node->tag == "span")
				span = child;
		}
		CHECK(span != InvalidBuiNode && model.Find(span)->tag == "span");
		CHECK(!model.MoveNode(model.NodeAtPath("0"), span, 0));
		CHECK(model.GetText().find("<span>Bye</span>") != std::string::npos);

		CHECK(model.MoveNode(span, model.NodeAtPath(result.new_selection_path), 1));
		const BuiNode *container_node = model.Find(
			model.NodeAtPath(result.new_selection_path));
		BuiNodeId moved = InvalidBuiNode;
		if (container_node)
		{
			for (const BuiNodeId child : container_node->children)
			{
				const BuiNode *node = model.Find(child);
				if (node && node->tag == "span")
					moved = child;
			}
		}
		CHECK(moved != InvalidBuiNode && model.Find(moved)->tag == "span");
		CHECK(!model.MoveNode(model.NodeAtPath(result.new_selection_path), moved, 9));

		CHECK(model.RemoveNode(moved));
		BuiDocumentModel reparsed;
		BuiParseError warning;
		CHECK(BuiDocumentModel::Parse(model.GetText(), reparsed, warning));
	}

	void TestSessionUndoAndSave()
	{
		TempDirectory temp;
		const std::filesystem::path path = temp.GetPath() / "Main.bui";
		WriteFile(path, SampleDocument());

		UiDocumentSessions sessions;
		std::string error;
		UiDocumentTab *tab = sessions.Open(path, error);
		CHECK(tab != nullptr);
		const std::string tab_id = tab->id;
		CHECK(!tab->modified);
		const BuiNodeId button = tab->model.NodeAtPath("0/0");

		CHECK(sessions.ApplyEdit(*tab, "Set id", [button](BuiDocumentModel &model,
			BuiEditResult &result)
		{
			return model.SetAttribute(button, "id", "start_button", &result);
		}));
		CHECK(tab->modified);
		CHECK(tab->CanUndo());

		std::string description;
		CHECK(sessions.Undo(description));
		CHECK(description == "Set id");
		CHECK(!tab->modified);
		CHECK(tab->model.GetText() == SampleDocument());
		CHECK(sessions.Redo(description));
		CHECK(tab->modified);

		const std::filesystem::path saved = temp.GetPath() / "Assets" / "Main.bui";
		CHECK(sessions.Save(*tab, saved, error));
		CHECK(!tab->modified);
		CHECK(ReadFile(saved).find("id=\"start_button\"") != std::string::npos);
		CHECK(ReadFile(path) == SampleDocument());

		UiDocumentTab *second = sessions.CreateNew(temp.GetPath() / "Second.bui", error);
		CHECK(second != nullptr);
		CHECK(second->id != tab->id);
		CHECK(second->modified);
		sessions.Activate(tab_id);
		CHECK(sessions.GetActive()->id == tab_id);

		CHECK(sessions.Close(second->id, false, error) == false);
		CHECK(sessions.Close(second->id, true, error));
		CHECK(sessions.Close(tab_id, false, error));
		CHECK(!sessions.HasDocuments());
	}

	void TestPreviewSerialization()
	{
		BuiDocumentModel model = ParseSample();
		const std::string preview = model.BuildPreviewLayout();
		CHECK(preview.find("data-bl-node=\"0\"") != std::string::npos);
		CHECK(preview.find("data-bl-node=\"0/0\"") != std::string::npos);
		CHECK(preview.find("data-bl-node=\"0/1\"") != std::string::npos);
		CHECK(preview.find("data-bl-node=\"0/2\"") != std::string::npos);
		CHECK(preview.find("<br data-bl-node=\"0/1\"/>") != std::string::npos);
		CHECK(model.GetLayout().find("data-bl-node") == std::string::npos);
		CHECK(model.GetText().find("data-bl-node") == std::string::npos);
	}

	void TestMultiTabIsolation()
	{
		TempDirectory temp;
		const std::filesystem::path first_path = temp.GetPath() / "First.bui";
		const std::filesystem::path second_path = temp.GetPath() / "Second.bui";
		WriteFile(first_path, SampleDocument());
		WriteFile(second_path, SampleDocument());

		UiDocumentSessions sessions;
		std::string error;
		UiDocumentTab *first = sessions.Open(first_path, error);
		CHECK(first != nullptr);
		const std::string first_id = first->id;
		UiDocumentTab *second = sessions.Open(second_path, error);
		CHECK(second != nullptr);
		const std::string second_id = second->id;
		CHECK(sessions.GetActive()->id == second_id);

		const BuiNodeId first_button = first->model.NodeAtPath("0/0");
		const BuiNodeId second_button = second->model.NodeAtPath("0/0");
		CHECK(sessions.ApplyEdit(*first, "First edit",
			[first_button](BuiDocumentModel &model, BuiEditResult &result)
			{
				return model.SetAttribute(first_button, "id", "first", &result);
			}));
		CHECK(first->modified);
		CHECK(!second->modified);

		sessions.Activate(second_id);
		CHECK(sessions.ApplyEdit(*second, "Second edit",
			[second_button](BuiDocumentModel &model, BuiEditResult &result)
			{
				return model.SetAttribute(second_button, "id", "second", &result);
			}));
		CHECK(second->modified);
		CHECK(first->modified);
		CHECK(first->model.GetText().find("id=\"first\"") != std::string::npos);
		CHECK(second->model.GetText().find("id=\"second\"") != std::string::npos);

		sessions.Activate(first_id);
		std::string description;
		CHECK(sessions.Undo(description));
		CHECK(first->model.GetText() == SampleDocument());
		CHECK(!first->modified);
		CHECK(sessions.GetActive()->id == first_id);
	}

	void TestStyleDeclarations()
	{
		const std::string text = "  left: 12dp; width:40% ;\n  /* note */ position: absolute;";
		const std::vector<BuiStyleDeclaration> declarations =
			ParseStyleDeclarations(text);
		CHECK(declarations.size() == 3);
		CHECK(declarations[0].name == "left");
		CHECK(declarations[0].value == "12dp");
		CHECK(declarations[1].name == "width");
		CHECK(declarations[1].value == "40%");
		CHECK(declarations[2].name == "position");
		CHECK(declarations[2].value == "absolute");
		CHECK(FindStyleDeclaration(declarations, "WIDTH") != nullptr);

		const std::string updated = SetStyleDeclarationValue(text, "width", "50%");
		CHECK(updated == "  left: 12dp; width:50% ;\n  /* note */ position: absolute;");
		// The trailing semicolon belongs to the original last declaration and is kept.
		CHECK(SetStyleDeclarationValue(text, "top", "4dp") ==
			"  left: 12dp; width:40% ;\n  /* note */ position: absolute; top: 4dp");
		CHECK(RemoveStyleDeclaration(text, "left") ==
			"width:40% ;\n  /* note */ position: absolute;");
		CHECK(RemoveStyleDeclaration(text, "top") == text);
		CHECK(SetStyleDeclarationValue(text, "left", "") ==
			"width:40% ;\n  /* note */ position: absolute;");
		CHECK(SetStyleDeclarationValue("", "left", "8dp") == "left: 8dp");
		CHECK(AppendStyleDeclaration("color: red", "top", "2dp") ==
			"color: red; top: 2dp");
		CHECK(ParseStyleDeclarations("color: rgb(1, 2, 3); background: url(a;b)")
				  .size() == 2);
	}

	void TestStyleSheetRules()
	{
		const std::string styles =
			".main-screen { width: 100%; }\n\n"
			"  /* keep me */\n"
			"#start, .primary {\n  color: red;\n  left: 4dp;\n}\n"
			"@media screen { .x { width: 1dp; } }\n";
		const std::vector<BuiStyleRule> rules = ParseStyleSheet(styles);
		CHECK(rules.size() == 3);
		CHECK(rules[0].selector == ".main-screen");
		CHECK(rules[0].declarations.size() == 1);
		CHECK(rules[1].selector == "#start, .primary");
		CHECK(rules[1].declarations.size() == 2);
		CHECK(rules[1].FindDeclaration("left") != nullptr);
		CHECK(rules[2].at_rule);

		const std::string edited = SetRuleDeclarationValue(styles, rules[1], "left", "9dp");
		CHECK(edited.find("left: 9dp;") != std::string::npos);
		CHECK(edited.find("/* keep me */") != std::string::npos);
		CHECK(edited.find(".main-screen { width: 100%; }") != std::string::npos);
		CHECK(edited.find("@media screen { .x { width: 1dp; } }") != std::string::npos);

		const std::string appended = AppendStyleRule(styles, "#extra",
			"color: blue; top: 1dp;");
		CHECK(appended.find("#extra {") != std::string::npos);
		CHECK(appended.find("  color: blue;") != std::string::npos);
		CHECK(appended.find("  top: 1dp;") != std::string::npos);

		BuiDocumentModel model;
		BuiParseError warning;
		CHECK(BuiDocumentModel::Parse(
			"<bui><styles>" + appended +
				"</styles><layout><div id=\"root\"><span id=\"leaf\" class=\"hot\"/></div></layout></bui>",
			model, warning));
		const BuiNode *leaf = model.Find(model.NodeAtPath("0/0"));
		CHECK(leaf != nullptr);
		CHECK(SelectorOfNode(*leaf) == "#leaf");

		const std::vector<BuiStyleRule> model_rules = ParseStyleSheet(model.GetStyles());
		CHECK(FindRuleForNode(model_rules, *leaf) == nullptr);
		// No rule names the root element, while the appended rule does name #extra.
		CHECK(FindRuleForNode(model_rules, *model.Find(model.NodeAtPath("0"))) == nullptr);
		CHECK(model.GetStyles().find("#extra {") != std::string::npos);
		CHECK(ParseStyleSheet(model.GetStyles()).size() == model_rules.size());
	}

	void TestSelectors()
	{
		BuiDocumentModel model;
		BuiParseError warning;
		CHECK(BuiDocumentModel::Parse(
			"<bui><styles>x</styles><layout>"
			"<div id=\"root\" class=\"main-screen wide\"><span class=\"hot\"/></div>"
			"</layout></bui>",
			model, warning));
		const BuiNode *root = model.Find(model.NodeAtPath("0"));
		CHECK(root != nullptr);
		CHECK(SelectorOfNode(*root) == "#root");
		CHECK(SelectorTargetsNode("#root", *root));
		CHECK(SelectorTargetsNode("div.main-screen", *root));
		CHECK(SelectorTargetsNode("span, .wide", *root));
		CHECK(!SelectorTargetsNode(".narrow", *root));
		CHECK(!SelectorTargetsNode("span", *root));
		CHECK(!SelectorTargetsNode(".main-screen:focus", *root));
		CHECK(!SelectorTargetsNode("div .main-screen", *root));
		CHECK(SplitSelectorList("#a, .b , span").size() == 3);
	}

	void TestStyleModelEdits()
	{
		BuiDocumentModel model = ParseSample();
		const std::string layout_before = model.GetLayout();
		BuiEditResult result;
		const std::uint64_t revision = model.GetRevision();
		CHECK(model.SetStyles("\n    .main-screen { width: 50%; }\n  "));
		CHECK(model.GetRevision() == revision + 1);
		CHECK(model.GetStyles().find("width: 50%") != std::string::npos);
		CHECK(model.GetLayout() == layout_before);
		CHECK(model.GetText().find("<button") != std::string::npos);
		CHECK(result.old_text.empty());

		const BuiNodeId button = model.NodeAtPath("0/0");
		CHECK(model.SetAttribute(button, "on-click", "start_game", &result));
		CHECK(model.GetText().find("on-click=\"start_game\"") != std::string::npos);
		CHECK(model.Find(button)->FindAttribute("on-click") != nullptr);
		CHECK(model.GetStyles().find(".main-screen { width: 50%; }") != std::string::npos);
	}

	void TestGizmoMath()
	{
		BuiGizmoGeometry geometry;
		geometry.x = 10.0f;
		geometry.y = 20.0f;
		geometry.width = 100.0f;
		geometry.height = 50.0f;

		CHECK(HitTestGizmo(geometry, 60.0f, 45.0f, 4.0f) == BuiGizmoHandle::Move);
		CHECK(HitTestGizmo(geometry, 12.0f, 22.0f, 4.0f) == BuiGizmoHandle::NorthWest);
		CHECK(HitTestGizmo(geometry, 108.0f, 68.0f, 4.0f) == BuiGizmoHandle::SouthEast);
		CHECK(HitTestGizmo(geometry, 60.0f, 21.0f, 4.0f) == BuiGizmoHandle::North);
		CHECK(HitTestGizmo(geometry, 109.0f, 45.0f, 4.0f) == BuiGizmoHandle::East);
		CHECK(HitTestGizmo(geometry, 200.0f, 45.0f, 4.0f) == BuiGizmoHandle::None);

		const BuiGizmoGeometry moved =
			ApplyGizmoDrag(geometry, BuiGizmoHandle::Move, 15.0f, -5.0f);
		CHECK(moved.x == 25.0f && moved.y == 15.0f);
		CHECK(moved.width == 100.0f && moved.height == 50.0f);

		const BuiGizmoGeometry resized =
			ApplyGizmoDrag(geometry, BuiGizmoHandle::SouthEast, 20.0f, 10.0f);
		CHECK(resized.x == 10.0f && resized.y == 20.0f);
		CHECK(resized.width == 120.0f && resized.height == 60.0f);

		// Dragging the west edge past the east edge clamps instead of inverting.
		const BuiGizmoGeometry clamped =
			ApplyGizmoDrag(geometry, BuiGizmoHandle::West, 500.0f, 0.0f);
		CHECK(clamped.width == 1.0f);
		CHECK(clamped.x == 109.0f);

		const BuiGizmoGeometry north =
			ApplyGizmoDrag(geometry, BuiGizmoHandle::North, 0.0f, -10.0f);
		CHECK(north.y == 10.0f && north.height == 60.0f);

		CHECK(geometry.NearlyEquals(geometry));
		BuiGizmoGeometry drifted = geometry;
		drifted.x += 30.0f;
		CHECK(!geometry.NearlyEquals(drifted));
	}

} // namespace

int main()
{
	TestParseAndRoundTrip();
	TestTolerantParsing();
	TestAttributeEditing();
	TestTextEditing();
	TestStructureEditing();
	TestSessionUndoAndSave();
	TestPreviewSerialization();
	TestMultiTabIsolation();
	TestStyleDeclarations();
	TestStyleSheetRules();
	TestSelectors();
	TestStyleModelEdits();
	TestGizmoMath();
	std::printf("All Bui document tests passed\n");
	return 0;
}
