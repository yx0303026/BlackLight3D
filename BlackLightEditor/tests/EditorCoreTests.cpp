#include "editor/core/CommandHistory.h"
#include "editor/core/CommandRegistry.h"
#include "editor/core/Commands.h"
#include "editor/core/EditorConfig.h"
#include "editor/core/EditorEvent.h"
#include "editor/core/EditorState.h"
#include "editor/core/EditorTheme.h"
#include "editor/project/ProjectFile.h"
#include "editor/project/ProjectGenerator.h"
#include "editor/project/ProjectManager.h"
#include "editor/project/ProjectTemplates.h"
#include "editor/project/ProjectTypes.h"
#include "editor/project/ProjectValidator.h"
#include "editor/project/RecentProjects.h"
#include "editor/project/TemplateEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

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

	/// Creates and removes an isolated directory for file-based tests.
	class TempDirectory final
	{
	public:
		TempDirectory()
		{
			const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
			path_ = std::filesystem::temp_directory_path() /
				("blacklight_editor_tests_" + std::to_string(stamp));
			std::filesystem::create_directories(path_);
		}

		~TempDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(path_, error);
		}

		TempDirectory(const TempDirectory &) = delete;
		TempDirectory &operator=(const TempDirectory &) = delete;

		const std::filesystem::path &GetPath() const noexcept { return path_; }

	private:
		std::filesystem::path path_;
	};

	/// Writes exact text to a test file.
	void WriteFile(const std::filesystem::path &path, const std::string &text)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		CHECK(output.good());
		output << text;
		CHECK(output.good());
	}

	/// Reads a whole file for content assertions.
	std::string ReadFile(const std::filesystem::path &path)
	{
		std::ifstream input(path, std::ios::binary);
		CHECK(input.good());
		std::ostringstream stream;
		stream << input.rdbuf();
		return stream.str();
	}

	/// Creates the minimum engine checkout that project generation and validation require.
	void CreateEngineStub(const std::filesystem::path &root)
	{
		WriteFile(root / "BlackLight" / "CMakeLists.txt",
			"cmake_minimum_required(VERSION 3.25)\nproject(BlackLight)\n");
	}

	/// Builds metadata that passes every validation rule for the supplied directories.
	ProjectMetadata MakeValidMetadata(const std::filesystem::path &engine_root)
	{
		ProjectMetadata metadata;
		metadata.name = "Validation Project";
		metadata.id = "{11111111-2222-4333-8444-555555555555}";
		metadata.name_space = "ValidationProject";
		metadata.kind = ProjectKind::WindowsApp;
		metadata.graphics_api = GraphicsApi::OpenGL;
		metadata.engine_root = engine_root;
		metadata.engine_version = "0.1.0";
		metadata.template_version = "1.0.0";
		metadata.created = "2026-11-11";
		metadata.dependencies.push_back(
			{"BlackLight", "0.1.0", engine_root.generic_u8string()});
		return metadata;
	}

	void TestEventBus()
	{
		EventBus bus;
		int typed = 0;
		int all = 0;
		int last_typed = 0;
		const EventBus::Token typed_token = bus.Subscribe(EventType::StatusMessage,
			[&](const EditorEvent &event)
			{
				++typed;
				last_typed = event.type == EventType::StatusMessage ? 1 : 0;
				CHECK(event.name == "hello");
			});
		const EventBus::Token all_token = bus.SubscribeAll([&](const EditorEvent &)
		{
			++all;
		});
		CHECK(bus.GetSubscriberCount() == 2);

		bus.Emit({EventType::StatusMessage, "hello", {}});
		CHECK(typed == 1);
		CHECK(all == 1);
		CHECK(last_typed == 1);

		bus.Emit({EventType::DocumentChanged, "doc", {}});
		CHECK(typed == 1);
		CHECK(all == 2);

		bus.Unsubscribe(typed_token);
		bus.Unsubscribe(all_token);
		bus.Unsubscribe(all_token);
		CHECK(bus.GetSubscriberCount() == 0);
		bus.Emit({EventType::StatusMessage, "hello", {}});
		CHECK(typed == 1);
		CHECK(all == 2);

		// Unsubscribing from inside a callback must not invalidate the current emission.
		int calls = 0;
		EventBus::Token self_token = EventBus::InvalidToken;
		self_token = bus.SubscribeAll([&](const EditorEvent &)
		{
			++calls;
			bus.Unsubscribe(self_token);
		});
		bus.Emit({EventType::SelectionChanged, {}, {}});
		bus.Emit({EventType::SelectionChanged, {}, {}});
		CHECK(calls == 1);

		bus.Clear();
		CHECK(bus.GetSubscriberCount() == 0);
	}

	void TestConfig()
	{
		TempDirectory temp;
		const std::filesystem::path path = temp.GetPath() / "nested" / "editor.ini";

		EditorConfig config(path);
		config.Set("editor", "theme", "dark");
		config.SetInt("editor", "width", 1280);
		config.SetBool("editor", "auto_save", true);
		config.AddRecentFile("C:/Work/Alpha.blw");
		config.AddRecentFile("C:/Work/Beta.blw");
		config.AddRecentFile("C:/Work/Alpha.blw");
		CHECK(config.Save());

		EditorConfig loaded(path);
		CHECK(loaded.Load());
		CHECK(loaded.Get("editor", "theme") == "dark");
		CHECK(loaded.GetInt("editor", "width", 0) == 1280);
		CHECK(loaded.GetBool("editor", "auto_save", false));
		CHECK(!loaded.GetBool("editor", "missing", false));
		CHECK(loaded.GetInt("missing", "key", 7) == 7);
		CHECK(loaded.Get("missing", "key", "fallback") == "fallback");

		const std::vector<std::string> recent = loaded.GetRecentFiles();
		CHECK(recent.size() == 2);
		CHECK(recent[0] == "C:/Work/Alpha.blw");
		CHECK(recent[1] == "C:/Work/Beta.blw");

		CHECK(loaded.Remove("editor", "theme"));
		CHECK(loaded.Get("editor", "theme", "light") == "light");
		CHECK(!loaded.Remove("editor", "theme"));

		const std::filesystem::path hand_written = temp.GetPath() / "hand_written.ini";
		WriteFile(hand_written, "; comment\n[ Editor ]\n  theme = Light \nflag=yes\ncount = 42\n");
		EditorConfig parsed(hand_written);
		CHECK(parsed.Load());
		CHECK(parsed.Get("editor", "theme") == "Light");
		CHECK(parsed.GetBool("editor", "flag", false));
		CHECK(parsed.GetInt("editor", "count", 0) == 42);

		loaded.ClearRecentFiles();
		CHECK(loaded.GetRecentFiles().empty());
	}

	void TestThemes()
	{
		ThemeRegistry registry;
		CHECK(registry.GetThemes().size() >= 3);
		CHECK(registry.Find("dark") != nullptr);
		CHECK(registry.Find("light") != nullptr);
		CHECK(registry.Find("midnight") != nullptr);
		CHECK(registry.Find("missing") == nullptr);

		const EditorTheme *dark = registry.Find("dark");
		CHECK(dark != nullptr);
		const std::string css =
			"a { color: @text; background-color: @surface-alt; border-color: @border-strong; }\n"
			"b { accent-color: @accent-hover; outline-color: @accent-active; }\n"
			"c { background-color: @surface; border-color: @border; color: @text-muted; }\n"
			"d { background-color: @bg; selection: @selection; hover: @hover; }\n"
			"e { warning: @warning; error: @error; success: @success; elevated: @elevated; }";
		const std::string applied = ThemeRegistry::Apply(*dark, css);
		CHECK(applied.find('@') == std::string::npos);
		CHECK(applied.find(dark->text) != std::string::npos);
		CHECK(applied.find(dark->surface_alt) != std::string::npos);
		CHECK(applied.find(dark->border_strong) != std::string::npos);
	}

	void TestStateAndCommands()
	{
		TempDirectory temp;
		EventBus bus;
		EditorState state(bus);
		CommandHistory history(state, bus);

		CHECK(!state.HasDocument());
		CHECK(!history.CanUndo());
		CHECK(history.Execute(std::make_unique<NewWorkspaceCommand>("Test Workspace")));
		CHECK(state.HasDocument());
		CHECK(state.GetDocumentName() == "Test Workspace");
		CHECK(history.GetUndoCount() == 0);
		CHECK(!history.IsModified());

		CHECK(history.Execute(std::make_unique<AddObjectCommand>(ObjectKind::Sprite)));
		CHECK(state.GetObjects().size() == 1);
		CHECK(state.GetSelection().kind == SelectionKind::Object);
		const std::string sprite_id = state.GetObjects().front().id;
		const std::string sprite_name = state.GetObjects().front().name;
		CHECK(!sprite_id.empty());
		CHECK(!sprite_name.empty());
		CHECK(history.CanUndo());
		CHECK(history.IsModified());
		CHECK(history.GetUndoName() == "Add Object");

		CHECK(history.Undo());
		CHECK(state.GetObjects().empty());
		CHECK(!history.IsModified());
		CHECK(history.CanRedo());
		CHECK(history.Redo());
		CHECK(state.GetObjects().size() == 1);
		CHECK(state.GetObjects().front().id == sprite_id);
		CHECK(state.GetObjects().front().name == sprite_name);
		CHECK(history.IsModified());

		CHECK(history.Execute(std::make_unique<AddObjectCommand>(ObjectKind::Camera)));
		CHECK(state.GetObjects().size() == 2);
		const std::string camera_id = state.GetObjects()[1].id;
		CHECK(history.Execute(std::make_unique<DeleteObjectCommand>(camera_id)));
		CHECK(state.GetObjects().size() == 1);
		CHECK(history.Undo());
		CHECK(state.GetObjects().size() == 2);
		CHECK(state.GetObjects()[1].id == camera_id);
		CHECK(state.GetSelection().id == camera_id);

		const std::string original_name = state.GetObjects().front().name;
		CHECK(history.Execute(std::make_unique<RenameObjectCommand>(sprite_id, "Renamed Sprite")));
		CHECK(state.FindObject(sprite_id)->name == "Renamed Sprite");
		CHECK(history.Undo());
		CHECK(state.FindObject(sprite_id)->name == original_name);

		const std::filesystem::path workspace = temp.GetPath() / "workspace.blw";
		CHECK(history.Execute(std::make_unique<SaveWorkspaceCommand>(workspace)));
		CHECK(!history.IsModified());
		CHECK(state.GetDocumentPath() == workspace);

		CHECK(history.Execute(std::make_unique<AddObjectCommand>(ObjectKind::Light)));
		CHECK(history.IsModified());
		CHECK(history.Execute(std::make_unique<SaveWorkspaceCommand>(workspace)));
		CHECK(!history.IsModified());
		CHECK(history.Undo());
		CHECK(history.IsModified());

		CHECK(history.Execute(std::make_unique<CloseWorkspaceCommand>()));
		CHECK(!state.HasDocument());
		CHECK(!history.CanUndo());
		CHECK(!history.CanRedo());

		// Failed commands leave the history untouched.
		CHECK(!history.Execute(std::make_unique<OpenWorkspaceCommand>(temp.GetPath() / "missing.blw")));
		CHECK(!history.CanUndo());
		CHECK(state.GetStatus().find("Failed") != std::string::npos);

		// Selection validation rejects unknown objects.
		EditorState selection_state(bus);
		selection_state.NewDocument("Selection");
		selection_state.SetSelection(SelectionKind::Object, "obj-missing");
		CHECK(selection_state.GetSelection().kind == SelectionKind::None);
	}

	void TestHistoryLimit()
	{
		EventBus bus;
		EditorState state(bus);
		CommandHistory history(state, bus, 2);
		CHECK(history.Execute(std::make_unique<NewWorkspaceCommand>("Limited")));
		CHECK(history.Execute(std::make_unique<AddObjectCommand>()));
		CHECK(history.Execute(std::make_unique<AddObjectCommand>()));
		CHECK(history.Execute(std::make_unique<AddObjectCommand>()));
		CHECK(history.GetUndoCount() == 2);
		CHECK(state.GetObjects().size() == 3);
		CHECK(history.Undo());
		CHECK(history.Undo());
		CHECK(!history.CanUndo());
		CHECK(state.GetObjects().size() == 1);

		history.Clear();
		CHECK(!history.CanUndo());
		CHECK(!history.CanRedo());
		CHECK(!history.IsModified());

		history.SetLimit(0);
		CHECK(history.GetLimit() == 0);
	}

	void TestWorkspaceFiles()
	{
		TempDirectory temp;
		EventBus bus;
		EditorState state(bus);

		state.NewDocument("Round Trip Workspace");
		CHECK(state.AddObject({"", "First Object", ObjectKind::Sprite}));
		CHECK(state.AddObject({"", "Second Object", ObjectKind::Camera}));
		CHECK(state.IsModified());

		const std::filesystem::path path = temp.GetPath() / "roundtrip.blw";
		CHECK(state.SaveDocument(path));
		CHECK(!state.IsModified());

		EditorState loaded(bus);
		CHECK(loaded.OpenDocument(path));
		CHECK(loaded.GetDocumentName() == "Round Trip Workspace");
		CHECK(loaded.GetObjects().size() == 2);
		CHECK(loaded.GetObjects()[0].name == "First Object");
		CHECK(loaded.GetObjects()[0].kind == ObjectKind::Sprite);
		CHECK(loaded.GetObjects()[1].name == "Second Object");
		CHECK(loaded.GetObjects()[1].kind == ObjectKind::Camera);
		CHECK(!loaded.IsModified());

		const std::filesystem::path malformed = temp.GetPath() / "malformed.blw";
		WriteFile(malformed, "not-a-workspace\n");
		EditorState rejected(bus);
		CHECK(!rejected.OpenDocument(malformed));
		CHECK(!rejected.HasDocument());

		const std::filesystem::path duplicate = temp.GetPath() / "duplicate.blw";
		WriteFile(duplicate,
			"blacklight-workspace 1\n"
			"name Duplicate\n"
			"object obj-1 Group First\n"
			"object obj-1 Group Second\n");
		CHECK(!rejected.OpenDocument(duplicate));

		const std::filesystem::path unknown_directive = temp.GetPath() / "unknown.blw";
		WriteFile(unknown_directive,
			"blacklight-workspace 1\n"
			"name Unknown Directives\n"
			"future-data 1 2 3\n"
			"object obj-7 Audio Music\n");
		CHECK(rejected.OpenDocument(unknown_directive));
		CHECK(rejected.GetObjects().size() == 1);
		CHECK(rejected.GetObjects().front().kind == ObjectKind::Audio);
	}

	void TestCommandRegistry()
	{
		CommandRegistry registry;
		int calls = 0;
		CHECK(registry.Register(CommandInfo("demo.run", "Run Demo", "Demo",
			[&calls]
			{
				++calls;
				return true;
			}).Shortcut('R', true)));
		CHECK(!registry.Register(CommandInfo("demo.run", "Duplicate", "Demo",
			[] { return true; })));
		CHECK(!registry.Register(CommandInfo("", "Empty", "Demo", [] { return true; })));

		CHECK(registry.Find("demo.run") != nullptr);
		CHECK(registry.Find("missing") == nullptr);
		CHECK(registry.Invoke("demo.run"));
		CHECK(calls == 1);
		CHECK(registry.Invoke("missing") == false);

		const std::vector<const CommandInfo *> matches = registry.MatchShortcuts('R', true, false, false);
		CHECK(matches.size() == 1);
		CHECK(matches.front()->id == "demo.run");
		CHECK(registry.MatchShortcuts('R', false, false, false).empty());
		CHECK(registry.MatchShortcuts('Q', true, false, false).empty());

		CHECK(CommandRegistry::FormatShortcut('S', true, true, false) == "Ctrl+Shift+S");
		CHECK(CommandRegistry::FormatShortcut(0x70, false, false, false) == "F1");
		CHECK(CommandRegistry::FormatShortcut(0x2E, false, false, false) == "Delete");
		CHECK(CommandRegistry::FormatShortcut(0, true, false, false).empty());

		CommandRegistry disabled_registry;
		CHECK(disabled_registry.Register(CommandInfo("demo.disabled", "Disabled", "Demo",
			[&calls]
			{
				++calls;
				return true;
			}).Shortcut('D', true).Enabled([] { return false; })));
		CHECK(!disabled_registry.IsEnabled("demo.disabled"));
		CHECK(!disabled_registry.Invoke("demo.disabled"));
		CHECK(disabled_registry.MatchShortcuts('D', true, false, false).empty());
		CHECK(calls == 1);
	}

	void TestProjectTypes()
	{
		CHECK(IsValidProjectName("MyGame"));
		CHECK(IsValidProjectName("My Game 2"));
		CHECK(IsValidProjectName("my_game"));
		CHECK(!IsValidProjectName(""));
		CHECK(!IsValidProjectName("1Game"));
		CHECK(!IsValidProjectName("My/Game"));
		CHECK(!IsValidProjectName("con"));
		CHECK(!IsValidProjectName("My.Game"));

		CHECK(DeriveNamespace("My Game") == "MyGame");
		CHECK(DeriveNamespace("my_game") == "MyGame");
		CHECK(DeriveNamespace("my-game") == "MyGame");
		CHECK(DeriveNamespace("123 game") == "P123Game");
		CHECK(DeriveNamespace("") == "Project");
		CHECK(IsValidNamespace(DeriveNamespace("2D Game")));

		CHECK(SanitizeTargetName("My Game") == "my_game");
		CHECK(SanitizeTargetName("2D Game") == "_2d_game");

		const std::string id = GenerateProjectId();
		CHECK(id.size() == 38);
		CHECK(id.front() == '{');
		CHECK(id.back() == '}');
		CHECK(id[15] == '4');
		CHECK(std::string("89ab").find(id[20]) != std::string::npos);
		CHECK(GenerateProjectId() != id);

		CHECK(std::string(ToString(ProjectKind::BlackLightGame)) == "BlackLightGame");
		CHECK(std::string(ToString(GraphicsApi::DirectX11)) == "DirectX11");

		ProjectKind kind = ProjectKind::WindowsApp;
		CHECK(ProjectKindFromString("blocklight-ui-inject", kind));
		CHECK(kind == ProjectKind::BlockLightUIInject);
		GraphicsApi api = GraphicsApi::OpenGL;
		CHECK(GraphicsApiFromString("dx12", api));
		CHECK(api == GraphicsApi::DirectX12);
		CHECK(std::string(GetEngineBackend(GraphicsApi::DirectX12)) == "Direct3D11");
	}

	void TestTemplateEngine()
	{
		TemplateContext context;
		context.Set("NAME", "Alpha");
		context.Set("VERSION", "1.2.3");

		std::string error;
		CHECK(context.Render("Hello ${NAME} ${VERSION}!", &error) == "Hello Alpha 1.2.3!");
		CHECK(error.empty());
		CHECK(context.Render("$${NAME}") == "${NAME}");
		CHECK(context.Contains("NAME"));
		CHECK(!context.Contains("MISSING"));

		CHECK(context.Render("${MISSING}", &error).empty());
		CHECK(error.find("MISSING") != std::string::npos);
		CHECK(context.Render("${BROKEN", &error).empty());
		CHECK(!error.empty());
	}

	void TestProjectFile()
	{
		TempDirectory temp;
		ProjectMetadata metadata = MakeValidMetadata(temp.GetPath() / "engine");
		metadata.name = "Sample Game";
		metadata.kind = ProjectKind::BlackLightGame;
		metadata.graphics_api = GraphicsApi::DirectX11;

		const std::filesystem::path file = temp.GetPath() / "project.blacklight";
		std::string error;
		CHECK(SaveProjectFile(file, metadata, error));
		CHECK(error.empty());

		ProjectMetadata loaded;
		CHECK(LoadProjectFile(file, loaded, error));
		CHECK(loaded.name == "Sample Game");
		CHECK(loaded.id == metadata.id);
		CHECK(loaded.name_space == metadata.name_space);
		CHECK(loaded.kind == ProjectKind::BlackLightGame);
		CHECK(loaded.graphics_api == GraphicsApi::DirectX11);
		CHECK(loaded.engine_root == metadata.engine_root);
		CHECK(loaded.engine_version == metadata.engine_version);
		CHECK(loaded.template_version == metadata.template_version);
		CHECK(loaded.created == metadata.created);
		CHECK(loaded.dependencies.size() == 1);
		CHECK(loaded.dependencies.front().name == "BlackLight");

		const std::filesystem::path malformed = temp.GetPath() / "malformed.blacklight";
		WriteFile(malformed, "not-a-project\n");
		CHECK(!LoadProjectFile(malformed, loaded, error));
		CHECK(!error.empty());

		const std::filesystem::path unknown_kind = temp.GetPath() / "unknown.blacklight";
		WriteFile(unknown_kind,
			"blacklight-project 1\n"
			"name = Broken\n"
			"id = {id}\n"
			"kind = Spaceship\n");
		CHECK(!LoadProjectFile(unknown_kind, loaded, error));

		const std::filesystem::path missing_id = temp.GetPath() / "missing_id.blacklight";
		WriteFile(missing_id,
			"blacklight-project 1\n"
			"name = Broken\n");
		CHECK(!LoadProjectFile(missing_id, loaded, error));
	}

	void TestProjectValidation()
	{
		TempDirectory temp;
		const std::filesystem::path engine = temp.GetPath() / "engine";
		CreateEngineStub(engine);
		const ProjectMetadata metadata = MakeValidMetadata(engine);

		CHECK(!ProjectValidator::HasErrors(ProjectValidator::ValidateMetadata(metadata)));

		const std::filesystem::path root = temp.GetPath() / "ValidProject";
		WriteFile(root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.25)\n");
		WriteFile(root / "src" / "main.cpp", "int main() { return 0; }\n");
		std::filesystem::create_directories(root / "assets");
		std::string error;
		CHECK(SaveProjectFile(root / "project.blacklight", metadata, error));

		const std::vector<ProjectIssue> issues =
			ProjectValidator::ValidateDirectory(root, metadata, "1.0.0");
		CHECK(!ProjectValidator::HasErrors(issues));
		CHECK(issues.empty());

		const std::vector<ProjectIssue> outdated =
			ProjectValidator::ValidateDirectory(root, metadata, "2.0.0");
		CHECK(!ProjectValidator::HasErrors(outdated));
		CHECK(ProjectValidator::HasWarnings(outdated));

		ProjectMetadata invalid = metadata;
		invalid.name = "bad/name";
		invalid.name_space = "1bad";
		invalid.engine_root.clear();
		invalid.dependencies.clear();
		const std::vector<ProjectIssue> invalid_issues =
			ProjectValidator::ValidateMetadata(invalid);
		CHECK(ProjectValidator::HasErrors(invalid_issues));
		CHECK(ProjectValidator::HasWarnings(invalid_issues));
		CHECK(ProjectValidator::FormatIssues(invalid_issues).find("[Error]") !=
			std::string::npos);

		const std::vector<ProjectIssue> missing_project =
			ProjectValidator::ValidateDirectory(temp.GetPath() / "MissingProject", metadata);
		CHECK(ProjectValidator::HasErrors(missing_project));
	}

	void TestProjectGeneration()
	{
		TempDirectory temp;
		const std::filesystem::path engine = temp.GetPath() / "engine";
		CreateEngineStub(engine);
		const std::filesystem::path projects = temp.GetPath() / "projects";

		ProjectGenerator generator;
		const ProjectKind kinds[] = {
			ProjectKind::WindowsApp, ProjectKind::BlackLightGame,
			ProjectKind::BlockLightUIInject};
		const char *names[] = {"SampleApp", "SampleGame", "SampleInject"};
		int index = 0;

		for (const ProjectKind kind : kinds)
		{
			ProjectOptions options;
			options.name = names[index];
			options.directory = projects;
			options.engine_root = engine;
			options.kind = kind;
			options.graphics_api = kind == ProjectKind::BlockLightUIInject
				? GraphicsApi::DirectX11
				: GraphicsApi::OpenGL;
			++index;

			const ProjectGenerationResult result = generator.Generate(options);
			CHECK(result.success);
			CHECK(result.error.empty());
			CHECK(!result.project_root.empty());
			CHECK(std::filesystem::is_directory(result.project_root));
			CHECK(std::filesystem::is_regular_file(
				result.project_root / "project.blacklight"));
			CHECK(result.metadata.kind == kind);
			CHECK(result.metadata.graphics_api == options.graphics_api);
			CHECK(result.metadata.name == options.name);
			CHECK(result.metadata.name_space == DeriveNamespace(options.name));
			CHECK(result.metadata.template_version ==
				generator.GetTemplates().GetVersion(kind));
			CHECK(result.metadata.dependencies.size() == 1);
			CHECK(result.metadata.dependencies.front().name == "BlackLight");
			CHECK(result.metadata.dependencies.front().version ==
				generator.GetTemplates().GetEngineVersion());
			CHECK(!result.files.empty());

			std::string error;
			ProjectMetadata loaded;
			CHECK(LoadProjectFile(result.project_root / "project.blacklight", loaded, error));
			CHECK(loaded.kind == kind);
			CHECK(loaded.graphics_api == options.graphics_api);
			CHECK(loaded.engine_root == std::filesystem::weakly_canonical(engine));

			const std::string cmake = ReadFile(result.project_root / "CMakeLists.txt");
			CHECK(cmake.find("project(\"" + options.name + "\"") != std::string::npos);
			CHECK(cmake.find("${CMAKE_BINARY_DIR}") != std::string::npos);
			CHECK(cmake.find("${PROJECT_") == std::string::npos);

			const std::string presets =
				ReadFile(result.project_root / "CMakePresets.json");
			CHECK(presets.find("${sourceDir}") != std::string::npos);
			CHECK(presets.find("${PROJECT_") == std::string::npos);

			for (const GeneratedFile &file : result.files)
			{
				const std::string text = ReadFile(file.path);
				CHECK(text.find("${PROJECT_") == std::string::npos);
				CHECK(text.find("${GRAPHICS_") == std::string::npos);
				CHECK(text.find("${ENGINE_") == std::string::npos);
				CHECK(text.find("${TEMPLATE_") == std::string::npos);
				CHECK(text.find("${CREATED_") == std::string::npos);
				CHECK(text.find("${TARGET_ARCH}") == std::string::npos);
				CHECK(text.find("${YEAR}") == std::string::npos);
				CHECK(text.find("${INCLUDE_GUARD}") == std::string::npos);
			}

			if (kind == ProjectKind::BlockLightUIInject)
			{
				CHECK(std::filesystem::is_regular_file(
					result.project_root / "include" / "PluginApi.h"));
				CHECK(std::filesystem::is_regular_file(
					result.project_root / "src" / "plugin.cpp"));
				CHECK(std::filesystem::is_regular_file(result.project_root / "src" /
					(SanitizeTargetName(options.name) + ".def")));
				const std::string plugin =
					ReadFile(result.project_root / "src" / "plugin.cpp");
				CHECK(plugin.find(options.name) != std::string::npos);
				CHECK(plugin.find("${") == std::string::npos);
			}
			else
			{
				CHECK(std::filesystem::is_regular_file(
					result.project_root / "src" / "main.cpp"));
				const std::string main = ReadFile(result.project_root / "src" / "main.cpp");
				CHECK(main.find("namespace " + options.name_space) != std::string::npos);
				CHECK(main.find(options.name) != std::string::npos);
				CHECK(main.find("${") == std::string::npos);
			}
		}

		ProjectOptions conflict;
		conflict.name = "SampleApp";
		conflict.directory = projects;
		conflict.engine_root = engine;
		const ProjectGenerationResult duplicate = generator.Generate(conflict);
		CHECK(!duplicate.success);
		CHECK(!duplicate.error.empty());

		ProjectOptions invalid;
		invalid.name = "1Invalid";
		invalid.directory = projects;
		invalid.engine_root = engine;
		const ProjectGenerationResult rejected = generator.Generate(invalid);
		CHECK(!rejected.success);

		// Updating scaffolding rewrites managed files but preserves user sources.
		const std::filesystem::path root = projects / "SampleApp";
		WriteFile(root / "src" / "main.cpp", "// user edit\n");
		WriteFile(root / "CMakeLists.txt", "stale\n");

		ProjectMetadata outdated;
		std::string error;
		CHECK(LoadProjectFile(root / "project.blacklight", outdated, error));
		outdated.template_version = "0.9.0";

		const ProjectGenerationResult updated = generator.UpdateScaffolding(outdated, root);
		CHECK(updated.success);
		CHECK(updated.metadata.template_version ==
			generator.GetTemplates().GetVersion(ProjectKind::WindowsApp));
		CHECK(ReadFile(root / "src" / "main.cpp") == "// user edit\n");
		CHECK(ReadFile(root / "CMakeLists.txt").find("cmake_minimum_required") !=
			std::string::npos);
	}

	void TestProjectManagerAndRecentProjects()
	{
		TempDirectory temp;
		const std::filesystem::path engine = temp.GetPath() / "engine";
		CreateEngineStub(engine);

		EventBus bus;
		int project_events = 0;
		bus.Subscribe(EventType::ProjectChanged, [&](const EditorEvent &)
		{
			++project_events;
		});

		ProjectOptions options;
		options.name = "ManagedProject";
		options.directory = temp.GetPath() / "projects";
		options.engine_root = engine;
		options.kind = ProjectKind::BlackLightGame;

		ProjectManager manager(bus);
		CHECK(!manager.HasProject());
		std::string error;
		CHECK(manager.Create(options, error));
		CHECK(error.empty());
		CHECK(manager.HasProject());
		CHECK(manager.GetMetadata().name == "ManagedProject");
		CHECK(manager.GetProjectRoot() ==
			std::filesystem::weakly_canonical(options.directory) / "ManagedProject");
		CHECK(project_events >= 1);
		const std::vector<ProjectIssue> issues = manager.Validate();
		CHECK(!ProjectValidator::HasErrors(issues));
		CHECK(manager.Save(error));

		manager.Close();
		CHECK(!manager.HasProject());

		CHECK(!manager.Open(temp.GetPath() / "missing.blacklight", error));
		CHECK(!error.empty());

		const std::filesystem::path project_file =
			std::filesystem::weakly_canonical(options.directory) / "ManagedProject" /
			"project.blacklight";
		CHECK(manager.Open(project_file, error));
		CHECK(manager.HasProject());
		CHECK(manager.GetMetadata().name == "ManagedProject");
		manager.Close();

		RecentProjects recent(3);
		CHECK(recent.Add({"A", "C:/Work/A.blacklight", "WindowsApp", "2026-11-11"}));
		CHECK(recent.Add({"B", "C:/Work/B.blacklight", "BlackLightGame", "2026-11-11"}));
		CHECK(recent.Add({"C", "C:/Work/C.blacklight", "BlockLightUIInject", "2026-11-11"}));
		CHECK(recent.Add({"A2", "c:/work/a.blacklight", "WindowsApp", "2026-11-12"}));
		CHECK(recent.GetEntries().size() == 3);
		CHECK(recent.GetEntries().front().name == "A2");
		CHECK(recent.GetEntries().back().name == "B");
		CHECK(recent.Remove("C:/Work/C.blacklight"));
		CHECK(recent.GetEntries().size() == 2);

		EditorConfig config(temp.GetPath() / "editor.ini");
		recent.Save(config);
		CHECK(config.Save());

		EditorConfig loaded_config(temp.GetPath() / "editor.ini");
		CHECK(loaded_config.Load());
		RecentProjects restored(3);
		restored.Load(loaded_config);
		CHECK(restored.GetEntries().size() == 2);
		CHECK(restored.GetEntries().front().path == "c:/work/a.blacklight");

		restored.Clear();
		CHECK(restored.GetEntries().empty());
		CHECK(!restored.Add({}));
	}

} // namespace

int main()
{
	TestEventBus();
	TestConfig();
	TestThemes();
	TestStateAndCommands();
	TestHistoryLimit();
	TestWorkspaceFiles();
	TestCommandRegistry();
	TestProjectTypes();
	TestTemplateEngine();
	TestProjectFile();
	TestProjectValidation();
	TestProjectGeneration();
	TestProjectManagerAndRecentProjects();

	std::printf("All editor core tests passed\n");
	return 0;
}
