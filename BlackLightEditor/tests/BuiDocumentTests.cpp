#include "editor/uidoc/BuiDocumentModel.h"
#include "editor/uidoc/UiDocumentSessions.h"

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
	std::printf("All Bui document tests passed\n");
	return 0;
}
