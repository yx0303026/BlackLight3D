#include "editor/ui/EditorApplication.h"

#include "blacklight_editor_bui.h"
#include "editor/core/Commands.h"
#include "editor/ui/EditorIcons.h"
#include "editor/ui/Win32Dialogs.h"
#include "engine/Input.h"
#include "engine/Logger.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <utility>

using BlackLight::Engine::Logger;
using BlackLight::Engine::LogLevel;

namespace BlackLight::Editor
{

	namespace
	{

		constexpr const char *EditorVersion = "0.1.0";
		constexpr std::size_t MaxFolderDepth = 16;

		/// Locates the executable directory for configuration and logs.
		std::filesystem::path GetExecutableDirectory()
		{
			std::wstring buffer(32768, L'\0');
			const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
													static_cast<DWORD>(buffer.size()));
			if (!length || length >= buffer.size())
				throw std::runtime_error("Failed to locate the editor executable");
			buffer.resize(length);
			return std::filesystem::path(buffer).parent_path();
		}

		/// Returns the default directory offered by the project wizard.
		std::filesystem::path GetDefaultProjectsDirectory()
		{
			wchar_t buffer[MAX_PATH]{};
			const DWORD length = GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH);
			if (length > 0 && length < MAX_PATH)
				return std::filesystem::path(buffer) / "Documents" / "BlackLight Projects";
			return GetExecutableDirectory() / "Projects";
		}

		/// Loads the regular and bold faces used by the editor stylesheet.
		bool LoadEditorFonts()
		{
			wchar_t directory[MAX_PATH]{};
			const UINT length = GetWindowsDirectoryW(directory, MAX_PATH);
			if (!length || length >= MAX_PATH)
				return false;
			const std::filesystem::path fonts = std::filesystem::path(directory) / "Fonts";

			// Segoe UI is the native Windows UI face; dialogs prefer it when it is present.
			Rml::LoadFontFace((fonts / "segoeui.ttf").u8string(), "Segoe UI");
			Rml::LoadFontFace((fonts / "segoeuib.ttf").u8string(), "Segoe UI",
							  Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold);

			return Rml::LoadFontFace((fonts / "arial.ttf").u8string()) &&
				   Rml::LoadFontFace((fonts / "arialbd.ttf").u8string());
		}

		/// Loads the Windows icon font under a stable family name for UI glyphs.
		bool LoadEditorIconFont()
		{
			wchar_t directory[MAX_PATH]{};
			const UINT length = GetWindowsDirectoryW(directory, MAX_PATH);
			if (!length || length >= MAX_PATH)
				return false;

			// Segoe Fluent Icons ships with Windows 11; Segoe MDL2 Assets with Windows 10.
			const std::filesystem::path fonts = std::filesystem::path(directory) / "Fonts";
			for (const wchar_t *file_name : {L"SegoeIcons.ttf", L"segmdl2.ttf"})
			{
				const std::filesystem::path path = fonts / file_name;
				std::error_code error;
				if (!std::filesystem::exists(path, error))
					continue;
				if (Rml::LoadFontFace(path.u8string(), Icons::Family,
									  Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal))
				{
					return true;
				}
			}
			return false;
		}

		/// Returns the CSS class used by one log level in the Output panel.
		const char *LogLevelClass(LogLevel level) noexcept
		{
			switch (level)
			{
			case LogLevel::Warning:
				return "log-warning";
			case LogLevel::Error:
			case LogLevel::Critical:
				return "log-error";
			case LogLevel::Trace:
			case LogLevel::Debug:
				return "log-trace";
			default:
				return "log-info";
			}
		}

		/// Returns the lower-case extension of a path without the dot.
		std::string FileExtension(const std::filesystem::path &path)
		{
			std::string extension = path.extension().u8string();
			if (!extension.empty() && extension.front() == '.')
				extension.erase(extension.begin());
			for (char &character : extension)
				character = static_cast<char>(std::tolower(
					static_cast<unsigned char>(character)));
			return extension;
		}

		/// Returns whether a directory name is generated or hidden editor noise.
		bool IsIgnoredDirectory(std::string_view name)
		{
			if (name == "build" || name == ".git" || name == ".vs" ||
				name == "CMakeFiles" || name == "_CPack_Packages")
			{
				return true;
			}
			return name.rfind("cmake-build-", 0) == 0;
		}

		/// Escapes one browser action payload as a single RML attribute value.
		std::string ActionValue(std::string_view prefix,
								const std::filesystem::path &path)
		{
			return std::string(prefix) + std::string(path.u8string());
		}

		/// Returns a display glyph for a file type.
		const char *FileGlyph(const std::filesystem::path &path, bool directory)
		{
			if (directory)
				return ">";
			const std::string extension = FileExtension(path);
			if (extension == "bui")
				return "UI";
			if (extension == "blacklight")
				return "P";
			if (extension == "cpp" || extension == "h" || extension == "hpp" ||
				extension == "c")
			{
				return "C";
			}
			if (extension == "md")
				return "M";
			return "-";
		}

	} // namespace

	EditorApplication::EditorApplication(Engine::WindowConfig window_config,
										 std::filesystem::path config_path)
		: Engine::Window(std::move(window_config)),
		  config_path_(std::move(config_path)),
		  state_(events_),
		  history_(state_, events_, 128),
		  panels_(events_),
		  menus_(commands_),
		  project_(events_),
		  recent_projects_(10, &events_)
	{
		config_.Load(config_path_);
		theme_id_ = config_.Get("editor", "theme", "dark");
		history_.SetLimit(static_cast<std::size_t>(
			std::max(1, config_.GetInt("editor", "history_limit", 128))));

		default_project_location_ = std::filesystem::u8path(config_.Get("project",
																		"default_location", GetDefaultProjectsDirectory().u8string()));
		if (default_project_location_.empty())
			default_project_location_ = GetDefaultProjectsDirectory();
		recent_projects_.SetLimit(static_cast<std::size_t>(
			std::max(1, config_.GetInt("project", "recent_limit", 10))));
		recent_projects_.Load(config_);

		assets_ = {
			{"Editor.bui", "UI Layout"},
			{"Scripts", "Folder"},
			{"Textures", "Folder"},
			{"Models", "Folder"},
			{"Audio", "Folder"},
		};
	}

	EditorApplication::~EditorApplication() = default;

	bool EditorApplication::OnInitialize()
	{
		Logger::Get().AddSink([this](const Engine::LogRecord &record)
							  { logs_.Add(record); });
		BL_LOG(LogLevel::Info, "Editor",
			   std::string("Starting BlackLightEditor ") + EditorVersion);

		Rml::Context *context = GetContext();
		if (!context)
		{
			BL_LOG(LogLevel::Error, "Editor", "The rendering device did not create an RmlUi context");
			return false;
		}
		if (!LoadEditorFonts())
		{
			BL_LOG(LogLevel::Error, "Editor", "Failed to load the Arial regular and bold fonts");
			return false;
		}
		icon_font_loaded_ = LoadEditorIconFont();
		if (!icon_font_loaded_)
		{
			BL_LOG(LogLevel::Warning, "Editor",
				   "No Segoe icon font was found; tab close buttons use a text fallback");
		}

		const EditorTheme *theme = themes_.Find(theme_id_);
		if (!theme)
		{
			theme_id_ = "dark";
			theme = themes_.Find(theme_id_);
		}

		std::string ui_error;
		if (!ui_.Initialize(*context, EditorResources::Find("Editor.bui"), *theme, ui_error))
		{
			BL_LOG(LogLevel::Error, "Editor", "Failed to load the editor UI: " + ui_error);
			return false;
		}

		ui_.SetClass("editor_root", "icon-font", icon_font_loaded_);
		panels_.SetCloseGlyph(icon_font_loaded_ ? Icons::Close : Icons::CloseFallback);

		events_.SubscribeAll([this](const EditorEvent &event)
							 { OnEditorEvent(event); });
		RegisterCommands();
		RegisterPanels();
		RegisterMenus();

		wizard_.Initialize(ui_);
		wizard_.SetCreateHandler([this](const ProjectOptions &options, std::string &error)
								 { return CreateProject(options, error); });
		wizard_.SetBrowseHandler([](const std::filesystem::path &initial)
								 { return ShowBrowseFolderDialog(initial, L"Select the project location"); });

		panels_.SetActionHandler([this](std::string_view action)
								 { HandlePanelAction(action); });
		preview_.SetSelectionHandler([this](std::string_view path)
									 { SelectUiNode(path); });
		panels_.ApplyConfig(config_);
		menus_.Initialize(ui_);
		panels_.Initialize(ui_);
		history_.SetUiDocuments(&ui_docs_);
		ui_.AddListener("toolbar", "toolbar", "click",
						[this](Rml::Event &event)
						{ HandleToolbarClick(event); });

		last_log_revision_ = logs_.GetRevision();
		state_.SetStatus("Ready - UI editor core");
		ui_dirty_ = true;

		BL_LOG(LogLevel::Info, "Editor", "Editor UI initialized");
		return true;
	}

	void EditorApplication::OnUpdate()
	{
		wizard_.Update();
		ProcessShortcuts();

		if (logs_.GetRevision() != last_log_revision_)
		{
			last_log_revision_ = logs_.GetRevision();
			ui_dirty_ = true;
		}
		if (panels_.ConsumeRefreshRequest())
			ui_dirty_ = true;
		UpdateUiPreview();

		if (!ui_dirty_)
			return;
		ui_dirty_ = false;
		UpdateUi();
	}

	void EditorApplication::OnShutdown()
	{
		Logger::Get().ClearSinks();
		SaveEditorConfig();
		ui_.ClearListeners("ui_preview");
		preview_.Close();
		wizard_.Shutdown(ui_);
		menus_.Shutdown(ui_);
		panels_.Shutdown(ui_);
		ui_.Shutdown();
		BL_LOG(LogLevel::Info, "Editor", "BlackLightEditor shutdown complete");
		Logger::Get().Flush();
	}

	void EditorApplication::RegisterCommands()
	{
		const auto add = [this](CommandInfo info)
		{ commands_.Register(std::move(info)); };

		add(CommandInfo("file.new", "New Workspace", "File",
						[this]
						{ return NewWorkspace(); })
				.Shortcut('N', true));
		add(CommandInfo("file.open", "Open Workspace...", "File",
						[this]
						{ return OpenWorkspaceDialog(); })
				.Shortcut('O', true));
		add(CommandInfo("file.save", "Save Workspace", "File",
						[this]
						{ return SaveWorkspace(false); })
				.Shortcut('S', true)
				.Enabled([this]
						 { return state_.HasDocument(); }));
		add(CommandInfo("file.save_as", "Save Workspace As...", "File",
						[this]
						{ return SaveWorkspace(true); })
				.Shortcut('S', true, true)
				.Enabled([this]
						 { return state_.HasDocument(); }));
		add(CommandInfo("file.close", "Close Workspace", "File",
						[this]
						{ return CloseWorkspace(); })
				.Shortcut('W', true)
				.Enabled([this]
						 { return state_.HasDocument(); }));

		add(CommandInfo("project.new", "New Project...", "File", [this]
						{
			ShowCreateProjectWizard();
			return true; })
				.Shortcut('N', true, true));
		add(CommandInfo("project.open", "Open Project...", "File",
						[this]
						{ return OpenProjectDialog(); })
				.Shortcut('O', true, true));
		add(CommandInfo("project.save", "Save Project", "File",
						[this]
						{ return SaveProject(); })
				.Enabled([this]
						 { return project_.HasProject(); }));
		add(CommandInfo("project.close", "Close Project", "File",
						[this]
						{ return CloseProject(); })
				.Enabled([this]
						 { return project_.HasProject(); }));

		add(CommandInfo("app.quit", "Exit", "File", [this]
						{
			if (!ConfirmDiscardIfNeeded() || !ConfirmDiscardUiTabs())
				return false;
			events_.Emit({EventType::QuitRequested, {}, {}});
			return true; })
				.Shortcut('Q', true));

		add(CommandInfo("edit.undo", "Undo", "Edit",
						[this]
						{ return HasActiveUiDocument() ? UndoUiDocument() : history_.Undo(); })
				.Shortcut('Z', true)
				.Enabled([this]
						 {
				const UiDocumentTab *tab = ui_docs_.GetActive();
				return tab ? tab->CanUndo() : history_.CanUndo(); }));
		add(CommandInfo("edit.redo", "Redo", "Edit",
						[this]
						{ return HasActiveUiDocument() ? RedoUiDocument() : history_.Redo(); })
				.Shortcut('Y', true)
				.Enabled([this]
						 {
				const UiDocumentTab *tab = ui_docs_.GetActive();
				return tab ? tab->CanRedo() : history_.CanRedo(); }));
		add(CommandInfo("edit.redo_alternate", "Redo", "Edit",
						[this]
						{ return HasActiveUiDocument() ? RedoUiDocument() : history_.Redo(); })
				.Shortcut('Z', true, true)
				.Enabled([this]
						 {
				const UiDocumentTab *tab = ui_docs_.GetActive();
				return tab ? tab->CanRedo() : history_.CanRedo(); }));
		add(CommandInfo("edit.clear_history", "Clear Command History", "Tools", [this]
						{
			history_.Clear();
			state_.SetStatus("Command history cleared");
			return true; }));

		add(CommandInfo("object.add", "Add Object", "Edit",
						[this]
						{ return AddObject(); })
				.Enabled([this]
						 { return state_.HasDocument(); }));
		add(CommandInfo("object.delete", "Delete Selected Object", "Edit",
						[this]
						{
							return HasUiSelection() && HasActiveUiDocument()
									   ? DeleteUiNode()
									   : DeleteSelected();
						})
				.Shortcut(VK_DELETE)
				.Enabled([this]
						 { return HasSelectedObject() || HasUiSelection(); }));

		add(CommandInfo("output.clear", "Clear Output", "Tools", [this]
						{
			logs_.Clear();
			state_.SetStatus("Output cleared");
			return true; }));

		add(CommandInfo("view.panel.scene", "Scene Panel", "View",
						[this]
						{ return panels_.TogglePanel("scene"); })
				.Shortcut('1', true)
				.Checked([this]
						 { return panels_.IsVisible("scene"); }));
		add(CommandInfo("view.panel.assets", "Assets Panel", "View",
						[this]
						{ return panels_.TogglePanel("assets"); })
				.Shortcut('2', true)
				.Checked([this]
						 { return panels_.IsVisible("assets"); }));
		add(CommandInfo("view.panel.inspector", "Inspector Panel", "View",
						[this]
						{ return panels_.TogglePanel("inspector"); })
				.Shortcut('3', true)
				.Checked([this]
						 { return panels_.IsVisible("inspector"); }));
		add(CommandInfo("view.panel.output", "Output Panel", "View",
						[this]
						{ return panels_.TogglePanel("output"); })
				.Shortcut('4', true)
				.Checked([this]
						 { return panels_.IsVisible("output"); }));
		add(CommandInfo("view.panel.history", "History Panel", "View",
						[this]
						{ return panels_.TogglePanel("history"); })
				.Shortcut('5', true)
				.Checked([this]
						 { return panels_.IsVisible("history"); }));

		for (const auto &theme : themes_.GetThemes())
		{
			const std::string theme_id = theme.id;
			add(CommandInfo("theme." + theme_id, "Theme: " + theme.name, "View",
							[this, theme_id]
							{
								ApplyTheme(theme_id, true);
								return true;
							})
					.Checked([this, theme_id]
							 { return theme_id_ == theme_id; }));
		}

		add(CommandInfo("ui.new", "New UI Document", "UI",
						[this]
						{ return NewUiDocument(); })
				.Shortcut('N', true, false, true));
		add(CommandInfo("ui.open", "Open UI Document...", "UI",
						[this]
						{ return OpenUiDocumentDialog(); })
				.Shortcut('O', true, false, true));
		add(CommandInfo("ui.save", "Save UI Document", "UI",
						[this]
						{ return SaveUiDocument(false); })
				.Shortcut('S', true, false, true)
				.Enabled([this]
						 { return HasActiveUiDocument(); }));
		add(CommandInfo("ui.save_as", "Save UI Document As...", "UI",
						[this]
						{ return SaveUiDocument(true); })
				.Shortcut('S', true, true, true)
				.Enabled([this]
						 { return HasActiveUiDocument(); }));
		add(CommandInfo("ui.close", "Close UI Document", "UI",
						[this]
						{ return CloseUiDocument(); })
				.Shortcut('W', true, false, true)
				.Enabled([this]
						 { return HasActiveUiDocument(); }));
		add(CommandInfo("ui.node.add_container", "Add Container", "UI",
						[this]
						{ return AddUiNode("div"); })
				.Enabled([this]
						 { return HasActiveUiDocument(); }));
		add(CommandInfo("ui.node.add_button", "Add Button", "UI",
						[this]
						{ return AddUiNode("button"); })
				.Enabled([this]
						 { return HasActiveUiDocument(); }));
		add(CommandInfo("ui.node.add_text", "Add Text", "UI",
						[this]
						{ return AddUiNode("text"); })
				.Enabled([this]
						 { return HasActiveUiDocument(); }));
		add(CommandInfo("ui.node.delete", "Delete UI Node", "UI",
						[this]
						{ return DeleteUiNode(); })
				.Enabled([this]
						 { return HasUiSelection(); }));
		add(CommandInfo("ui.node.move_up", "Move UI Node Up", "UI",
						[this]
						{ return MoveUiNode(-1); })
				.Shortcut(VK_UP, false, false, true)
				.Enabled([this]
						 { return HasUiSelection(); }));
		add(CommandInfo("ui.node.move_down", "Move UI Node Down", "UI",
						[this]
						{ return MoveUiNode(1); })
				.Shortcut(VK_DOWN, false, false, true)
				.Enabled([this]
						 { return HasUiSelection(); }));
		add(CommandInfo("view.panel.folder", "Folder Panel", "View",
						[this]
						{ return panels_.TogglePanel("folder"); })
				.Checked([this]
						 { return panels_.IsVisible("folder"); }));
		add(CommandInfo("view.panel.hierarchy", "Hierarchy Panel", "View",
						[this]
						{ return panels_.TogglePanel("hierarchy"); })
				.Checked([this]
						 { return panels_.IsVisible("hierarchy"); }));

		add(CommandInfo("layout.reset", "Reset Layout", "View", [this]
						{
			panels_.ResetLayout();
			state_.SetStatus("Layout reset to defaults");
			return true; }));

		add(CommandInfo("mode.edit", "Edit Mode", "Tools", [this]
						{
			state_.SetMode(EditorMode::Edit);
			state_.SetStatus("Mode: Edit");
			return true; })
				.Checked([this]
						 { return state_.GetMode() == EditorMode::Edit; }));
		add(CommandInfo("mode.play", "Play Mode", "Tools", [this]
						{
			state_.SetMode(EditorMode::Play);
			state_.SetStatus("Mode: Play");
			return true; })
				.Checked([this]
						 { return state_.GetMode() == EditorMode::Play; }));
		add(CommandInfo("mode.pause", "Pause Mode", "Tools", [this]
						{
			state_.SetMode(EditorMode::Paused);
			state_.SetStatus("Mode: Paused");
			return true; })
				.Checked([this]
						 { return state_.GetMode() == EditorMode::Paused; }));

		add(CommandInfo("project.validate", "Validate Project", "Build",
						[this]
						{ return ValidateProject(); })
				.Enabled([this]
						 { return project_.HasProject(); }));
		add(CommandInfo("project.update_template", "Update Project Scaffolding", "Tools",
						[this]
						{ return UpdateProjectScaffolding(); })
				.Enabled([this]
						 { return project_.HasProject(); }));

		add(CommandInfo("build.configure", "Configure Build", "Build", [this]
						{
			BL_LOG(LogLevel::Info, "Build", "Build configuration arrives in stage 4");
			state_.SetStatus("Build configuration arrives in stage 4");
			return true; }));
		add(CommandInfo("build.build", "Build Project", "Build", [this]
						{
			BL_LOG(LogLevel::Info, "Build", "CMake and Ninja integration arrives in stage 4");
			state_.SetStatus("CMake and Ninja integration arrives in stage 4");
			return true; }));
		add(CommandInfo("build.run", "Run Project", "Build", [this]
						{
			BL_LOG(LogLevel::Info, "Build", "Runtime preview arrives in stage 3");
			state_.SetStatus("Runtime preview arrives in stage 3");
			return true; }));

		add(CommandInfo("help.about", "About BlackLightEditor", "Help", [this]
						{
			BL_LOG(LogLevel::Info, "Editor",
				std::string("BlackLightEditor ") + EditorVersion + " - Stage 1 editor core framework");
			BL_LOG(LogLevel::Info, "Editor",
				"Application shell, event bus, configuration, editor state, commands, "
				"command history, menus, toolbar, shortcuts, dockable panels, and themes");
			state_.SetStatus(std::string("BlackLightEditor ") + EditorVersion +
				" - Stage 1 editor core framework");
			return true; }));
	}

	void EditorApplication::RegisterPanels()
	{
		panels_.RegisterPanel({"scene", "Scene", DockRegion::Left, true, true,
							   [this]
							   { return RenderScenePanel(); }});
		panels_.RegisterPanel({"assets", "Assets", DockRegion::Left, true, true,
							   [this]
							   { return RenderAssetsPanel(); }});
		panels_.RegisterPanel({"folder", "Folder", DockRegion::Left, true, true,
							   [this]
							   { return RenderFolderPanel(); }});
		panels_.RegisterPanel({"hierarchy", "Hierarchy", DockRegion::Left, true, true,
							   [this]
							   { return RenderHierarchyPanel(); }});
		panels_.RegisterPanel({"inspector", "Inspector", DockRegion::Right, true, true,
							   [this]
							   { return RenderInspectorPanel(); }});
		panels_.RegisterPanel({"output", "Output", DockRegion::Bottom, true, true,
							   [this]
							   { return RenderOutputPanel(); }});
		panels_.RegisterPanel({"history", "History", DockRegion::Bottom, true, true,
							   [this]
							   { return RenderHistoryPanel(); }});
		panels_.RegisterPanel({"viewport", "Viewport", DockRegion::Center, true, false,
							   [this]
							   { return RenderViewportPanel(); }});
	}

	void EditorApplication::RegisterMenus()
	{
		menus_.AddMenu("file", "File",
					   {"project.new", "project.open", "", "file.new", "file.open", "",
						"file.save", "file.save_as", "", "file.close", "project.save", "project.close",
						"", "app.quit"});
		menus_.AddMenu("edit", "Edit",
					   {"edit.undo", "edit.redo", "", "object.add", "object.delete"});
		menus_.AddMenu("ui", "UI",
					   {"ui.new", "ui.open", "", "ui.save", "ui.save_as", "ui.close", "",
						"ui.node.add_container", "ui.node.add_button", "ui.node.add_text", "",
						"ui.node.delete", "ui.node.move_up", "ui.node.move_down"});
		menus_.AddMenu("view", "View",
					   {"view.panel.scene", "view.panel.assets", "view.panel.inspector",
						"view.panel.output", "view.panel.history", "", "view.panel.folder",
						"view.panel.hierarchy", "",
						"theme.dark", "theme.light", "theme.midnight", "", "layout.reset"});
		menus_.AddMenu("build", "Build",
					   {"project.validate", "", "build.configure", "build.build", "build.run"});
		menus_.AddMenu("tools", "Tools",
					   {"output.clear", "edit.clear_history", "project.update_template", "",
						"mode.edit", "mode.play", "mode.pause"});
		menus_.AddMenu("help", "Help", {"help.about"});
	}

	void EditorApplication::OnEditorEvent(const EditorEvent &event)
	{
		if (event.type == EventType::QuitRequested)
		{
			if (!ConfirmDiscardUiTabs())
			{
				ui_dirty_ = true;
				return;
			}
			Exit();
			return;
		}
		ui_dirty_ = true;
	}

	void EditorApplication::ProcessShortcuts()
	{
		Engine::InputState &input = GetInput();
		if (wizard_.IsVisible())
		{
			if (input.WasKeyPressed(VK_ESCAPE))
			{
				wizard_.Hide();
				ui_dirty_ = true;
			}
			return;
		}
		if (menus_.IsOpen() && input.WasKeyPressed(VK_ESCAPE))
		{
			menus_.CloseAll(ui_);
			ui_dirty_ = true;
		}

		const bool ctrl = input.IsKeyDown(VK_CONTROL);
		const bool shift = input.IsKeyDown(VK_SHIFT);
		const bool alt = input.IsKeyDown(VK_MENU);
		for (std::uint32_t key = 1; key < 256; ++key)
		{
			if (!input.WasKeyPressed(key))
				continue;
			const std::vector<const CommandInfo *> matches =
				commands_.MatchShortcuts(key, ctrl, shift, alt);
			for (const CommandInfo *command : matches)
				commands_.Invoke(command->id);
		}
	}

	void EditorApplication::UpdateUi()
	{
		panels_.Refresh(ui_);
		BindPanelInteractions();
		UpdateStatusBar();
		UpdateToolbarState();
	}

	void EditorApplication::UpdateStatusBar()
	{
		ui_.SetText("status_message", state_.GetStatus());
		const UiDocumentTab *ui_tab = ui_docs_.GetActive();
		const std::string undo_text = ui_tab
										  ? (ui_tab->CanUndo()
												 ? "Undo: " + ui_tab->history[ui_tab->undo_position - 1].description
												 : std::string("Undo: -"))
										  : (history_.CanUndo()
												 ? "Undo: " + history_.GetUndoName()
												 : std::string("Undo: -"));
		ui_.SetText("status_undo", undo_text);

		std::string document = "No workspace";
		if (state_.HasDocument())
			document = state_.GetDocumentName() + (state_.IsModified() ? " *" : "");
		ui_.SetText("status_document", document);
		if (ui_tab)
		{
			std::string ui_document = ui_tab->title + (ui_tab->modified ? " *" : "");
			ui_.SetText("status_document", ui_document);
		}
		ui_.SetText("status_project",
					project_.HasProject() ? project_.GetMetadata().name : std::string("No project"));

		const std::size_t object_count = state_.GetObjects().size();
		ui_.SetText("status_objects", std::to_string(object_count) +
										  (object_count == 1 ? " object" : " objects"));
		ui_.SetText("status_mode", ToString(state_.GetMode()));
		ui_.SetText("status_theme", CurrentTheme().name);

		std::string ui_status = "No UI document";
		if (ui_tab)
		{
			std::string node_path = ui_tab->selection_path;
			const BuiNode *node = ui_docs_.GetSelectedNode(*ui_tab);
			ui_status = "UI: " + ui_tab->title + (ui_tab->modified ? " *" : "");
			if (node)
			{
				ui_status += " - ";
				ui_status += node->type == BuiNodeType::Text ? "text" : "<" + node->tag;
				if (const BuiAttribute *id = node->type == BuiNodeType::Element
												 ? node->FindAttribute("id")
												 : nullptr)
				{
					ui_status += "#" + id->value;
				}
				if (node->type == BuiNodeType::Element)
					ui_status += ">";
			}
		}
		ui_.SetText("status_ui", ui_status);

		const EditorMode mode = state_.GetMode();
		ui_.SetClass("status_mode", "play", mode == EditorMode::Play);
		ui_.SetClass("status_mode", "pause", mode == EditorMode::Paused);
	}

	void EditorApplication::UpdateToolbarState()
	{
		ui_.SetClass("tool_save", "disabled",
					 !state_.HasDocument() && !HasActiveUiDocument());
		ui_.SetClass("tool_add", "disabled", !state_.HasDocument());
		ui_.SetClass("tool_delete", "disabled",
					 !HasSelectedObject() && !HasUiSelection());
		const UiDocumentTab *ui_tab = ui_docs_.GetActive();
		ui_.SetClass("tool_undo", "disabled",
					 ui_tab ? !ui_tab->CanUndo() : !history_.CanUndo());
		ui_.SetClass("tool_redo", "disabled",
					 ui_tab ? !ui_tab->CanRedo() : !history_.CanRedo());

		const EditorMode mode = state_.GetMode();
		ui_.SetClass("tool_mode_edit", "mode-active", mode == EditorMode::Edit);
		ui_.SetClass("tool_mode_play", "mode-active", mode == EditorMode::Play);
		ui_.SetClass("tool_mode_pause", "mode-active", mode == EditorMode::Paused);
	}

	void EditorApplication::ApplyTheme(std::string_view theme_id, bool announce)
	{
		const EditorTheme *theme = themes_.Find(theme_id);
		if (!theme)
		{
			state_.SetStatus("Unknown theme: " + std::string(theme_id));
			return;
		}

		const std::string previous = theme_id_;
		theme_id_ = theme->id;
		ui_.SetTheme(*theme);
		config_.Set("editor", "theme", theme_id_);
		if (announce)
			state_.SetStatus("Theme: " + theme->name);
		if (announce || previous != theme_id_)
			events_.Emit({EventType::ThemeChanged, theme_id_, {}});
		ui_dirty_ = true;
	}

	const EditorTheme &EditorApplication::CurrentTheme() const
	{
		const EditorTheme *theme = themes_.Find(theme_id_);
		if (!theme)
			theme = themes_.Find("dark");
		return *theme;
	}

	bool EditorApplication::NewWorkspace()
	{
		if (!ConfirmDiscardIfNeeded())
			return false;
		const bool executed =
			history_.Execute(std::make_unique<NewWorkspaceCommand>("Untitled Workspace"));
		if (executed)
			state_.SetStatus("Created Untitled Workspace");
		return executed;
	}

	bool EditorApplication::OpenWorkspaceDialog()
	{
		const std::filesystem::path path = ShowOpenWorkspaceDialog(GetExecutableDirectory());
		if (path.empty())
			return false;
		return OpenWorkspace(path);
	}

	bool EditorApplication::OpenWorkspace(const std::filesystem::path &path)
	{
		if (path.empty() || !ConfirmDiscardIfNeeded())
			return false;
		if (!history_.Execute(std::make_unique<OpenWorkspaceCommand>(path)))
			return false;

		config_.AddRecentFile(path.u8string());
		config_.Save();
		state_.SetStatus("Opened " + state_.GetDocumentName());
		events_.Emit({EventType::RecentProjectsChanged, path.u8string(), {}});
		return true;
	}

	bool EditorApplication::SaveWorkspace(bool save_as)
	{
		if (!state_.HasDocument())
		{
			state_.SetStatus("There is no workspace to save");
			return false;
		}

		std::filesystem::path path = state_.GetDocumentPath();
		if (save_as || path.empty())
		{
			std::filesystem::path suggestion = path;
			if (suggestion.empty())
				suggestion = GetExecutableDirectory() / (state_.GetDocumentName() + ".blw");
			path = ShowSaveWorkspaceDialog(suggestion);
			if (path.empty())
				return false;
		}

		if (!history_.Execute(std::make_unique<SaveWorkspaceCommand>(path)))
			return false;

		config_.AddRecentFile(path.u8string());
		config_.Save();
		state_.SetStatus("Saved " + path.filename().u8string());
		events_.Emit({EventType::RecentProjectsChanged, path.u8string(), {}});
		return true;
	}

	bool EditorApplication::CloseWorkspace()
	{
		if (!state_.HasDocument() || !ConfirmDiscardIfNeeded())
			return false;
		if (!history_.Execute(std::make_unique<CloseWorkspaceCommand>()))
			return false;
		state_.SetStatus("Workspace closed");
		return true;
	}

	bool EditorApplication::AddObject()
	{
		if (!history_.Execute(std::make_unique<AddObjectCommand>(ObjectKind::Group)))
			return false;
		if (const SceneObject *object = state_.FindObject(state_.GetSelection().id))
			state_.SetStatus("Added " + object->name);
		else
			state_.SetStatus("Added object");
		return true;
	}

	bool EditorApplication::DeleteSelected()
	{
		if (!HasSelectedObject())
			return false;
		const std::string id = state_.GetSelection().id;
		const SceneObject *object = state_.FindObject(id);
		const std::string name = object ? object->name : id;
		if (!history_.Execute(std::make_unique<DeleteObjectCommand>(id)))
			return false;
		state_.SetStatus("Deleted " + name);
		return true;
	}

	bool EditorApplication::ConfirmDiscardIfNeeded()
	{
		if (!state_.HasDocument() || !state_.IsModified())
			return true;
		return ConfirmDiscardChanges(state_.GetDocumentName());
	}

	bool EditorApplication::ConfirmDiscardUiTabs()
	{
		for (const UiDocumentTab &tab : ui_docs_.GetTabs())
		{
			if (tab.modified && !ConfirmDiscardChanges(tab.title))
				return false;
		}
		return true;
	}

	bool EditorApplication::HasSelectedObject() const
	{
		const Selection &selection = state_.GetSelection();
		return selection.kind == SelectionKind::Object &&
			   state_.FindObject(selection.id) != nullptr;
	}

	void EditorApplication::HandlePanelAction(std::string_view action)
	{
		if (action.rfind("ui-open:", 0) == 0)
		{
			OpenUiDocument(std::filesystem::u8path(std::string(action.substr(8))));
			return;
		}
		if (action.rfind("ui-select:", 0) == 0)
		{
			SelectUiNode(action.substr(10));
			return;
		}
		if (action.rfind("ui-toggle:", 0) == 0)
		{
			ToggleUiNode(action.substr(10));
			return;
		}
		if (action.rfind("ui-tab-close:", 0) == 0)
		{
			const std::string id(action.substr(13));
			if (ui_docs_.Activate(id))
				CloseUiDocument();
			return;
		}
		if (action.rfind("ui-tab:", 0) == 0)
		{
			const std::string id(action.substr(7));
			if (ui_docs_.Activate(id))
			{
				state_.SetStatus("Activated " + id);
				ui_dirty_ = true;
			}
			return;
		}
		if (action.rfind("folder:", 0) == 0)
		{
			const std::filesystem::path path =
				std::filesystem::u8path(std::string(action.substr(7)));
			if (folder_expanded_.count(path))
				folder_expanded_.erase(path);
			else
				folder_expanded_.insert(path);
			ui_dirty_ = true;
			return;
		}
		if (action.rfind("not-editable:", 0) == 0)
		{
			state_.SetStatus("Not editable here: " +
							 std::string(action.substr(13)));
			ui_dirty_ = true;
			return;
		}
		const std::size_t separator = action.find(':');
		const std::string_view verb =
			separator == std::string_view::npos ? action : action.substr(0, separator);
		const std::string_view value =
			separator == std::string_view::npos ? std::string_view() : action.substr(separator + 1);

		if (verb == "command")
		{
			commands_.Invoke(value);
			return;
		}
		if (verb == "select-object")
		{
			state_.SetSelection(SelectionKind::Object, std::string(value));
			const SceneObject *object = state_.FindObject(value);
			state_.SetStatus(object ? "Selected " + object->name : "Selection cleared");
			return;
		}
		if (verb == "select-asset")
		{
			state_.SetSelection(SelectionKind::Asset, std::string(value));
			state_.SetStatus("Selected asset: " + std::string(value));
			return;
		}
		if (verb == "open-recent")
		{
			OpenWorkspace(std::filesystem::u8path(std::string(value)));
			return;
		}
		if (verb == "open-project")
		{
			OpenProject(std::filesystem::u8path(std::string(value)));
			return;
		}
	}

	void EditorApplication::HandleToolbarClick(Rml::Event &event)
	{
		Rml::Element *toolbar = ui_.Find("toolbar");
		const std::string command = EditorUi::FindAttribute(event.GetTargetElement(), toolbar,
															"data-command");
		if (!command.empty())
		{
			if (command == "file.save" && HasActiveUiDocument())
				SaveUiDocument(false);
			else
				commands_.Invoke(command);
		}
	}

	void EditorApplication::BindPanelInteractions()
	{
		Rml::Element *inspector = ui_.Find("panel_inspector_body");
		if (!inspector)
			return;
		const auto bind_change = [this, inspector](std::string_view id,
												   std::string_view property)
		{
			ui_.AddListener("ui_inspector", id, "change",
							[this, property](Rml::Event &)
							{
								ApplyUiProperty(property);
							});
		};
		ui_.ClearListeners("ui_inspector");
		bind_change("ui_prop_id", "id");
		bind_change("ui_prop_class", "class");
		bind_change("ui_prop_text", "text");

		Rml::Element *hierarchy = ui_.Find("panel_hierarchy_body");
		ui_.ClearListeners("ui_hierarchy_drag");
		if (!hierarchy)
			return;
		ui_.AddListener("ui_hierarchy_drag", hierarchy, "mousedown",
						[this, hierarchy](Rml::Event &event)
						{
							const std::string path = EditorUi::FindAttribute(
								event.GetTargetElement(), hierarchy, "data-ui-node");
							if (!path.empty())
							{
								ui_drag_source_ = path;
								event.StopPropagation();
							}
						});
		ui_.AddListener("ui_hierarchy_drag", hierarchy, "mousemove",
						[this](Rml::Event &event)
						{ UpdateUiDrag(event); });
		ui_.AddListener("ui_hierarchy_drag", hierarchy, "mouseup",
						[this](Rml::Event &event)
						{ EndUiDrag(event); });
	}

	void EditorApplication::ApplyUiProperty(std::string_view property)
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
			return;
		Rml::ElementFormControl *control = rmlui_dynamic_cast<Rml::ElementFormControl *>(
			ui_.Find(std::string("ui_prop_") + std::string(property)));
		if (!control)
			return;
		const std::string value = control->GetValue();
		const BuiNodeId id = tab->model.NodeAtPath(tab->selection_path);
		if (id == InvalidBuiNode)
			return;

		const bool applied = ui_docs_.ApplyEdit(*tab,
												property == "text" ? "Set text" : "Set " + std::string(property),
												[id, property, value](BuiDocumentModel &model, BuiEditResult &result)
												{
													if (property == "text")
														return model.SetElementText(id, value, &result) ||
															   model.SetText(id, value, &result);
													if (value.empty())
														return model.RemoveAttribute(id, property, &result);
													return model.SetAttribute(id, property, value, &result);
												});
		if (applied)
		{
			state_.SetStatus("Updated UI " + std::string(property));
			events_.Emit({EventType::UiDocumentModified, tab->id,
						  "Set " + std::string(property)});
			ui_dirty_ = true;
		}
	}

	void EditorApplication::BeginUiDrag(std::string_view path)
	{
		ui_drag_source_ = std::string(path);
	}

	void EditorApplication::UpdateUiDrag(Rml::Event &event)
	{
		if (ui_drag_source_.empty())
			return;
		Rml::Element *hierarchy = ui_.Find("panel_hierarchy_body");
		if (!hierarchy)
			return;
		const std::string path = EditorUi::FindAttribute(
			event.GetTargetElement(), hierarchy, "data-ui-node");
		if (path.empty())
			return;
		Rml::Element *row = event.GetTargetElement();
		if (ui_drag_over_ && ui_drag_over_ != row)
			ui_drag_over_->SetClass("drag-over", false);
		ui_drag_over_ = row;
		row->SetClass("drag-over", true);
	}

	void EditorApplication::EndUiDrag(Rml::Event &event)
	{
		const std::string source = ui_drag_source_;
		ui_drag_source_.clear();
		if (ui_drag_over_)
		{
			ui_drag_over_->SetClass("drag-over", false);
			ui_drag_over_ = nullptr;
		}
		if (source.empty())
			return;
		Rml::Element *hierarchy = ui_.Find("panel_hierarchy_body");
		if (!hierarchy)
			return;
		const std::string target = EditorUi::FindAttribute(
			event.GetTargetElement(), hierarchy, "data-ui-node");
		if (target.empty() || target == source)
			return;
		// Drop into the target element; the lower half appends as the last child.
		ApplyUiDrop(source, target, 0);
	}

	void EditorApplication::ApplyUiDrop(std::string_view source,
										std::string_view target, int /*mode*/)
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
			return;
		const BuiNodeId source_id = tab->model.NodeAtPath(source);
		const BuiNodeId target_id = tab->model.NodeAtPath(target);
		const BuiNode *target_node = tab->model.Find(target_id);
		if (source_id == InvalidBuiNode || !target_node ||
			target_node->type != BuiNodeType::Element)
		{
			return;
		}
		const std::size_t index = target_node->children.size();
		const bool applied = ui_docs_.ApplyEdit(*tab, "Move UI Node",
												[source_id, target_id, index](BuiDocumentModel &model,
																			  BuiEditResult &result)
												{
													return model.MoveNode(source_id, target_id, index, &result);
												});
		if (applied)
		{
			state_.SetStatus("Moved UI node");
			events_.Emit({EventType::UiDocumentModified, tab->id, "Move"});
			ui_dirty_ = true;
		}
	}

	void EditorApplication::UpdateUiPreview(bool force)
	{
		Rml::Context *context = GetContext();
		const UiDocumentTab *tab = ui_docs_.GetActive();
		if (!context || !tab)
		{
			if (preview_.IsOpen())
			{
				ui_.ClearListeners("ui_preview");
				preview_.Close();
			}
			preview_revision_ = 0;
			preview_tab_id_.clear();
			return;
		}

		if (preview_tab_id_ != tab->id)
			ui_tree_expanded_.clear();

		if (force || preview_tab_id_ != tab->id ||
			preview_revision_ != tab->model.GetRevision() || !preview_.IsOpen())
		{
			ui_.ClearListeners("ui_preview");
			std::string error;
			if (!preview_.Rebuild(*context, tab->model, error))
			{
				BL_LOG(LogLevel::Error, "UI", "Preview failed: " + error);
				state_.SetStatus("UI preview failed: " + error);
				preview_.Close();
				preview_revision_ = 0;
				preview_tab_id_.clear();
				return;
			}
			preview_tab_id_ = tab->id;
			preview_revision_ = tab->model.GetRevision();
			Rml::ElementDocument *document = preview_.GetDocument();
			if (document)
			{
				ui_.AddListener("ui_preview", document, "click",
								[this](Rml::Event &event)
								{ preview_.HandleClick(event); });
			}
		}
		preview_.AnchorTo(ui_.Find("viewport_preview_frame"));
	}

	std::string EditorApplication::RenderScenePanel() const
	{
		if (!state_.HasDocument())
			return "<div class=\"empty\">No workspace is open.<br/>Use File &gt; New Workspace to begin.</div>";

		const auto &objects = state_.GetObjects();
		if (objects.empty())
			return "<div class=\"empty\">This workspace has no objects yet.<br/>Use Edit &gt; Add Object.</div>";

		const Selection &selection = state_.GetSelection();
		std::string html;
		for (const auto &object : objects)
		{
			const bool selected =
				selection.kind == SelectionKind::Object && selection.id == object.id;
			html += "<div class=\"scene-row";
			if (selected)
				html += " selected";
			html += "\" data-action=\"select-object:" + EditorUi::Escape(object.id) + "\">";
			html += "<span class=\"scene-name\">" + EditorUi::Escape(object.name) + "</span>";
			html += "<span class=\"kind\">" + EditorUi::Escape(ToString(object.kind)) + "</span>";
			html += "</div>";
		}
		return html;
	}

	std::string EditorApplication::RenderAssetsPanel() const
	{
		const Selection &selection = state_.GetSelection();
		std::string html;
		for (const auto &asset : assets_)
		{
			const bool selected =
				selection.kind == SelectionKind::Asset && selection.id == asset.name;
			html += "<div class=\"asset-row";
			if (selected)
				html += " selected";
			html += "\" data-action=\"select-asset:" + EditorUi::Escape(asset.name) + "\">";
			html += "<span class=\"asset-name\">" + EditorUi::Escape(asset.name) + "</span>";
			html += "<span class=\"kind\">" + EditorUi::Escape(asset.kind) + "</span>";
			html += "</div>";
		}
		html += "<div class=\"panel-hint\">Stage 3 replaces this placeholder with the asset browser.</div>";
		return html;
	}

	std::string EditorApplication::RenderFolderPanel() const
	{
		if (!project_.HasProject())
		{
			return "<div class=\"empty\">No project is open.<br/><br/>"
				   "<span class=\"palette-button\" data-action=\"command:project.new\">"
				   "New Project...</span></div>";
		}

		std::string html;
		std::error_code error;
		const auto render_directory = [&](const auto &self,
										  const std::filesystem::path &directory, int depth) -> void
		{
			if (depth > 12)
				return;
			std::vector<std::filesystem::directory_entry> entries;
			for (std::filesystem::directory_iterator it(directory, error), end;
				 it != end && !error; it.increment(error))
			{
				if (it->is_directory(error))
				{
					if (!IsIgnoredDirectory(it->path().filename().u8string()))
						entries.push_back(*it);
				}
				else if (it->is_regular_file(error))
				{
					entries.push_back(*it);
				}
			}
			std::sort(entries.begin(), entries.end(),
					  [](const auto &left, const auto &right)
					  {
						  std::error_code left_error;
						  std::error_code right_error;
						  const bool left_directory = left.is_directory(left_error);
						  const bool right_directory = right.is_directory(right_error);
						  if (left_directory != right_directory)
							  return left_directory;
						  return left.path().filename().u8string() <
								 right.path().filename().u8string();
					  });

			for (const auto &entry : entries)
			{
				std::error_code entry_error;
				const bool is_directory = entry.is_directory(entry_error);
				const std::filesystem::path path = entry.path();
				const bool expanded = is_directory && folder_expanded_.count(path) != 0;
				const std::string indent = std::to_string(4 + depth * 14);
				html += "<div class=\"folder-row\"";
				if (is_directory)
				{
					html += " data-action=\"folder:" +
							EditorUi::Escape(path.u8string()) + "\"";
				}
				else if (FileExtension(path) == "bui")
				{
					html += " data-action=\"ui-open:" +
							EditorUi::Escape(path.u8string()) + "\"";
				}
				else if (FileExtension(path) == "blacklight")
				{
					html += " data-action=\"open-project:" +
							EditorUi::Escape(path.u8string()) + "\"";
				}
				else
				{
					html += " data-action=\"not-editable:" +
							EditorUi::Escape(path.u8string()) + "\"";
				}
				html += " style=\"padding-left: " + indent + "dp;\">";
				html += "<span class=\"folder-glyph\">";
				html += is_directory ? (expanded ? "v" : ">") : FileGlyph(path, false);
				html += "</span><span class=\"folder-name\">" +
						EditorUi::Escape(path.filename().u8string()) + "</span>";
				if (!is_directory)
				{
					html += "<span class=\"kind\">" +
							EditorUi::Escape(FileExtension(path)) + "</span>";
				}
				html += "</div>";
				if (expanded)
					self(self, path, depth + 1);
			}
		};

		render_directory(render_directory, project_.GetProjectRoot(), 0);
		if (html.empty())
			html = "<div class=\"empty\">The project directory is empty.</div>";
		return html;
	}

	std::string EditorApplication::RenderHierarchyPanel() const
	{
		const UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
		{
			return "<div class=\"empty\">Open a .bui document from Folder or "
				   "use UI &gt; New UI Document.</div>";
		}

		std::string html =
			"<div class=\"ui-doc-tabs\">";
		for (const UiDocumentTab &document : ui_docs_.GetTabs())
		{
			html += "<div class=\"ui-doc-tab";
			if (document.id == tab->id)
				html += " active";
			html += "\" data-action=\"ui-tab:" + EditorUi::Escape(document.id) + "\">" +
					EditorUi::Escape(document.title) + (document.modified ? " *" : "") +
					"<span class=\"ui-doc-tab-close\" data-action=\"ui-tab-close:" +
					EditorUi::Escape(document.id) + "\">x</span></div>";
		}
		html += "</div>";
		html += "<div class=\"ui-palette\">";
		html += "<span class=\"palette-button\" data-action=\"command:ui.node.add_container\">"
				"+ Container</span>";
		html += "<span class=\"palette-button\" data-action=\"command:ui.node.add_button\">"
				"+ Button</span>";
		html += "<span class=\"palette-button\" data-action=\"command:ui.node.add_text\">"
				"+ Text</span>";
		html += "<span class=\"palette-button\" data-action=\"command:ui.node.delete\">"
				"Delete</span>";
		html += "<span class=\"palette-button\" data-action=\"command:ui.node.move_up\">"
				"Up</span>";
		html += "<span class=\"palette-button\" data-action=\"command:ui.node.move_down\">"
				"Down</span>";
		html += "</div>";
		if (!tab->model.GetWarning().message.empty())
		{
			html += "<div class=\"panel-hint\">Parse warning: " +
					EditorUi::Escape(tab->model.GetWarning().message) + "</div>";
		}

		const auto render_node = [&](const auto &self, BuiNodeId id, int depth) -> void
		{
			const BuiNode *node = tab->model.Find(id);
			if (!node)
				return;
			const std::string path = tab->model.PathOf(id);
			const bool expanded = ui_tree_expanded_.count(path) != 0;
			const bool selected = tab->selection_path == path;
			html += "<div class=\"tree-row";
			if (selected)
				html += " selected";
			html += "\" data-ui-node=\"" + EditorUi::Escape(path) +
					"\" data-action=\"ui-select:" + EditorUi::Escape(path) + "\"";
			html += " style=\"padding-left: " + std::to_string(4 + depth * 14) + "dp;\">";
			if (node->type == BuiNodeType::Element && !node->children.empty())
			{
				html += "<span class=\"tree-toggle\" data-action=\"ui-toggle:" +
						EditorUi::Escape(path) + "\">" + (expanded ? "v" : ">") + "</span>";
			}
			else
			{
				html += "<span class=\"tree-toggle\"></span>";
			}

			if (node->type == BuiNodeType::Text)
			{
				std::string preview = node->text.substr(0, 24);
				if (node->text.size() > preview.size())
					preview += "...";
				html += "<span class=\"tree-text\">\"" +
						EditorUi::Escape(preview) + "\"</span>";
			}
			else
			{
				html += "<span class=\"tree-tag\">" + EditorUi::Escape(node->tag) + "</span>";
				if (const BuiAttribute *id_attribute = node->FindAttribute("id"))
					html += "<span class=\"tree-id\">#" +
							EditorUi::Escape(id_attribute->value) + "</span>";
				if (const BuiAttribute *class_attribute = node->FindAttribute("class"))
				{
					const std::string classes = class_attribute->value;
					std::size_t begin = 0;
					while (begin < classes.size())
					{
						const std::size_t space = classes.find(' ', begin);
						const std::string_view item = std::string_view(classes).substr(begin,
																					   space == std::string::npos ? std::string::npos : space - begin);
						if (!item.empty())
							html += "<span class=\"tree-class\">." +
									EditorUi::Escape(item) + "</span>";
						if (space == std::string::npos)
							break;
						begin = space + 1;
					}
				}
			}
			html += "</div>";
			if (expanded)
			{
				for (const BuiNodeId child : node->children)
					self(self, child, depth + 1);
			}
		};

		const BuiNode *root = tab->model.Find(tab->model.GetRoot());
		if (!root)
			return html + "<div class=\"empty\">The document tree is unavailable.</div>";
		for (const BuiNodeId child : root->children)
			render_node(render_node, child, 0);
		return html;
	}

	std::string EditorApplication::RenderInspectorPanel() const
	{
		const UiDocumentTab *ui_tab = ui_docs_.GetActive();
		if (!state_.HasDocument())
		{
			if (!ui_tab)
				return "<div class=\"empty\">No workspace or UI document is open.</div>";
		}

		const auto row = [](std::string_view label, std::string_view value)
		{
			return "<div class=\"inspector-row\"><span class=\"inspector-label\">" +
				   EditorUi::Escape(label) + "</span><span class=\"inspector-value\">" +
				   EditorUi::Escape(value) + "</span></div>";
		};

		std::string html;
		if (ui_tab)
		{
			html += "<div class=\"inspector-section\">UI Document</div>";
			html += row("Name", ui_tab->title);
			html += row("Path", ui_tab->path.empty()
									? std::string("Not saved yet")
									: ui_tab->path.u8string());
			html += row("Modified", ui_tab->modified ? "Yes" : "No");
			const BuiNode *node = ui_docs_.GetSelectedNode(*ui_tab);
			if (node)
			{
				html += "<div class=\"inspector-section\">Node</div>";
				html += row("Type", node->type == BuiNodeType::Text
										? std::string("Text")
										: node->tag);
				html += row("Path", ui_tab->selection_path);
				if (node->type == BuiNodeType::Element)
				{
					const auto attribute_value = [node](std::string_view name)
					{
						const BuiAttribute *attribute = node->FindAttribute(name);
						return attribute ? attribute->value : std::string();
					};
					html += "<div class=\"ui-property\"><span class=\"inspector-label\">"
							"id</span><input id=\"ui_prop_id\" class=\"ui-input\" type=\"text\" value=\"" +
							EditorUi::Escape(attribute_value("id")) + "\"/></div>";
					html += "<div class=\"ui-property\"><span class=\"inspector-label\">"
							"class</span><input id=\"ui_prop_class\" class=\"ui-input\" type=\"text\" "
							"value=\"" +
							EditorUi::Escape(attribute_value("class")) + "\"/></div>";
				}
				else
				{
					html += "<div class=\"ui-property\"><span class=\"inspector-label\">"
							"text</span><input id=\"ui_prop_text\" class=\"ui-input\" type=\"text\" "
							"value=\"" +
							EditorUi::Escape(node->text) + "\"/></div>";
				}
			}
			else
			{
				html += "<div class=\"panel-hint\">Select a node in Hierarchy.</div>";
			}
		}

		if (!state_.HasDocument())
			return html;

		html += "<div class=\"inspector-section\">Workspace</div>";
		html += row("Name", state_.GetDocumentName());
		const std::string path = state_.GetDocumentPath().empty()
									 ? std::string("Not saved yet")
									 : state_.GetDocumentPath().u8string();
		html += row("Path", path);
		html += row("Modified", state_.IsModified() ? "Yes" : "No");
		html += row("Objects", std::to_string(state_.GetObjects().size()));

		const Selection &selection = state_.GetSelection();
		if (selection.kind == SelectionKind::Object)
		{
			const SceneObject *object = state_.FindObject(selection.id);
			if (!object)
				return html + "<div class=\"panel-hint\">The selected object no longer exists.</div>";
			html += "<div class=\"inspector-section\">Object</div>";
			html += row("Name", object->name);
			html += row("Identifier", object->id);
			html += row("Type", ToString(object->kind));
		}
		else if (selection.kind == SelectionKind::Asset)
		{
			const auto it = std::find_if(assets_.begin(), assets_.end(),
										 [&selection](const AssetEntry &asset)
										 { return asset.name == selection.id; });
			const std::string kind = it == assets_.end() ? std::string("Unknown") : it->kind;
			html += "<div class=\"inspector-section\">Asset</div>";
			html += row("Name", selection.id);
			html += row("Kind", kind);
		}
		else
		{
			html += "<div class=\"panel-hint\">Select an object or an asset to inspect it.</div>";
		}
		return html;
	}

	std::string EditorApplication::RenderOutputPanel() const
	{
		const std::vector<Engine::LogRecord> records = logs_.Snapshot();
		if (records.empty())
			return "<div class=\"empty\">No log messages yet.</div>";

		std::string html;
		for (auto it = records.rbegin(); it != records.rend(); ++it)
		{
			html += "<div class=\"log-line ";
			html += LogLevelClass(it->level);
			html += "\">" + EditorUi::Escape(Engine::FormatLogRecord(*it)) + "</div>";
		}
		return html;
	}

	std::string EditorApplication::RenderHistoryPanel() const
	{
		const std::vector<std::string> undo_entries = history_.GetUndoEntries();
		const std::vector<std::string> redo_entries = history_.GetRedoEntries();
		if (undo_entries.empty() && redo_entries.empty())
			return "<div class=\"empty\">Command history is empty.</div>";

		std::string html = "<div class=\"inspector-section\">Undo (" +
						   std::to_string(undo_entries.size()) + ")</div>";
		if (undo_entries.empty())
			html += "<div class=\"panel-hint\">Nothing to undo.</div>";
		else
		{
			for (auto it = undo_entries.rbegin(); it != undo_entries.rend(); ++it)
			{
				html += "<div class=\"history-row";
				if (it == undo_entries.rbegin())
					html += " history-current";
				html += "\">" + EditorUi::Escape(*it) + "</div>";
			}
		}

		html += "<div class=\"inspector-section\">Redo (" +
				std::to_string(redo_entries.size()) + ")</div>";
		if (redo_entries.empty())
			html += "<div class=\"panel-hint\">Nothing to redo.</div>";
		else
		{
			for (const auto &entry : redo_entries)
				html += "<div class=\"history-row\">" + EditorUi::Escape(entry) + "</div>";
		}
		return html;
	}

	std::string EditorApplication::RenderViewportPanel() const
	{
		if (ui_docs_.GetActive())
		{
			return "<div id=\"viewport_preview_frame\" class=\"viewport-preview-frame\"></div>";
		}
		std::string html = "<div class=\"viewport-hero\">";
		if (project_.HasProject())
		{
			const ProjectMetadata &metadata = project_.GetMetadata();
			html += "<div class=\"viewport-title\">" + EditorUi::Escape(metadata.name) + "</div>";
			html += "<div class=\"viewport-subtitle\">" + EditorUi::Escape(ToString(metadata.kind)) +
					" - " + EditorUi::Escape(ToString(metadata.graphics_api)) + "</div>";
			html += "<div class=\"viewport-meta\">" +
					EditorUi::Escape(project_.GetProjectRoot().u8string()) + "</div>";
			html += "<div class=\"viewport-meta\">Engine " +
					EditorUi::Escape(metadata.engine_version) + " | Template " +
					EditorUi::Escape(metadata.template_version) + "</div>";
			html += "<div class=\"viewport-actions\">";
			html += "<div class=\"accent-button\" data-action=\"command:project.validate\">"
					"Validate Project</div>";
			html += "<div class=\"ghost-button\" data-action=\"command:project.save\">"
					"Save Project</div>";
			html += "<div class=\"ghost-button\" data-action=\"command:project.update_template\">"
					"Update Scaffolding</div>";
			html += "</div>";
		}
		else
		{
			html += "<div class=\"viewport-title\">BlackLightEditor</div>";
			html += "<div class=\"viewport-subtitle\">Stage 2 - Project System</div>";
			html += "<div class=\"viewport-meta\">Create a project or open an existing "
					".blacklight file.</div>";
			html += "<div class=\"viewport-actions\">";
			html += "<div class=\"accent-button\" data-action=\"command:project.new\">"
					"New Project...</div>";
			html += "<div class=\"ghost-button\" data-action=\"command:project.open\">"
					"Open Project...</div>";
			html += "<div class=\"ghost-button\" data-action=\"command:file.new\">"
					"New Workspace</div>";
			html += "</div>";
		}

		html += "<div class=\"viewport-section-title\">Recent Projects</div>";
		const std::vector<RecentProject> &recent_projects = recent_projects_.GetEntries();
		if (recent_projects.empty())
		{
			html += "<div class=\"viewport-empty\">No recent projects.</div>";
		}
		else
		{
			html += "<div class=\"recent-list\">";
			const std::size_t count = (std::min)(recent_projects.size(), std::size_t(6));
			for (std::size_t index = 0; index < count; ++index)
			{
				const RecentProject &entry = recent_projects[index];
				html += "<div class=\"recent-row\" data-action=\"open-project:" +
						EditorUi::Escape(entry.path) + "\">" + EditorUi::Escape(entry.name) +
						" (" + EditorUi::Escape(entry.kind) + ")</div>";
			}
			html += "</div>";
		}

		if (state_.HasDocument())
		{
			html += "<div class=\"viewport-section-title\">Current Workspace</div>";
			html += "<div class=\"viewport-meta\">" +
					EditorUi::Escape(state_.GetDocumentName()) +
					(state_.IsModified() ? " (modified)" : "") + "</div>";
		}
		html += "</div>";
		return html;
	}

	void EditorApplication::ShowCreateProjectWizard()
	{
		wizard_.Show("MyProject", default_project_location_);
		ui_dirty_ = true;
	}

	std::filesystem::path EditorApplication::UiDirectory() const
	{
		if (!project_.HasProject())
			return {};
		return project_.GetProjectRoot() / "ui";
	}

	bool EditorApplication::NewUiDocument()
	{
		const std::filesystem::path directory = UiDirectory();
		std::error_code error;
		if (!directory.empty())
			std::filesystem::create_directories(directory, error);
		std::filesystem::path suggestion = directory.empty()
											   ? std::filesystem::path("Untitled.bui")
											   : directory / "Untitled.bui";
		std::string message;
		UiDocumentTab *tab = ui_docs_.CreateNew(suggestion, message);
		if (!tab)
		{
			state_.SetStatus("Failed to create UI document: " + message);
			return false;
		}
		state_.SetStatus("Created " + tab->title);
		events_.Emit({EventType::UiDocumentsChanged, tab->id, tab->title});
		ui_tree_expanded_.insert(tab->model.PathOf(tab->model.GetRoot()));
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::OpenUiDocumentDialog()
	{
		std::filesystem::path initial = UiDirectory();
		if (initial.empty())
			initial = GetExecutableDirectory();
		const std::filesystem::path path = ShowOpenBuiDialog(initial);
		if (path.empty())
			return false;
		return OpenUiDocument(path);
	}

	bool EditorApplication::OpenUiDocument(const std::filesystem::path &path)
	{
		std::string error;
		UiDocumentTab *tab = ui_docs_.Open(path, error);
		if (!tab)
		{
			state_.SetStatus("Failed to open UI document: " + error);
			BL_LOG(LogLevel::Error, "UI", "Failed to open " + path.u8string() + ": " + error);
			return false;
		}
		// Select the first real element so editing can start immediately.
		const BuiNode *root_node = tab->model.Find(tab->model.GetRoot());
		if (tab->selection_path.empty() && root_node && !root_node->children.empty())
			tab->selection_path = tab->model.PathOf(root_node->children.front());
		state_.SetStatus("Opened " + tab->title);
		if (!error.empty())
			BL_LOG(LogLevel::Warning, "UI", tab->title + ": " + error);
		events_.Emit({EventType::UiDocumentsChanged, tab->id, tab->title});
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::SaveUiDocument(bool save_as)
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
		{
			state_.SetStatus("There is no UI document to save");
			return false;
		}
		std::filesystem::path path = tab->path;
		if (save_as || path.empty())
		{
			path = ShowSaveBuiDialog(path.empty()
										 ? (UiDirectory().empty() ? GetExecutableDirectory() /
																		(tab->title.empty() ? "Untitled.bui" : tab->title)
																  : UiDirectory() / (tab->title.empty() ? "Untitled.bui" : tab->title))
										 : path);
			if (path.empty())
				return false;
		}
		std::string error;
		if (!ui_docs_.Save(*tab, path, error))
		{
			state_.SetStatus("Failed to save UI document: " + error);
			BL_LOG(LogLevel::Error, "UI", "Failed to save " + path.u8string() + ": " + error);
			return false;
		}
		state_.SetStatus("Saved " + tab->title);
		events_.Emit({EventType::UiDocumentModified, tab->id, tab->title});
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::CloseUiDocument()
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
			return false;
		const std::string id = tab->id;
		const std::string title = tab->title;
		if (tab->modified && !ConfirmDiscardChanges(tab->title))
			return false;
		std::string error;
		if (!ui_docs_.Close(id, true, error))
			return false;
		state_.SetStatus("Closed " + title);
		events_.Emit({EventType::UiDocumentsChanged, id, title});
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::HasActiveUiDocument() const
	{
		return ui_docs_.GetActive() != nullptr;
	}

	bool EditorApplication::HasUiSelection() const
	{
		return ui_docs_.GetSelectedNode() != nullptr;
	}

	void EditorApplication::SelectUiNode(std::string_view path)
	{
		if (!ui_docs_.SetSelection(path))
			return;
		const BuiNode *node = ui_docs_.GetSelectedNode();
		state_.SetStatus(node
							 ? "UI selection: " + (node->type == BuiNodeType::Text
													   ? std::string("text")
													   : "<" + node->tag + ">")
							 : std::string("UI selection cleared"));
		events_.Emit({EventType::UiSelectionChanged, std::string(path), {}});
		ui_dirty_ = true;
	}

	void EditorApplication::ToggleUiNode(std::string_view path)
	{
		if (ui_tree_expanded_.count(std::string(path)))
			ui_tree_expanded_.erase(std::string(path));
		else
			ui_tree_expanded_.insert(std::string(path));
		ui_dirty_ = true;
	}

	bool EditorApplication::AddUiNode(std::string_view kind)
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
			return false;
		const BuiNodeId selected_id = tab->model.NodeAtPath(tab->selection_path);
		const BuiNode *selected = selected_id == InvalidBuiNode
									  ? nullptr
									  : tab->model.Find(selected_id);
		BuiNodeId parent = tab->model.GetRoot();
		BuiNodeId insertion_sibling = InvalidBuiNode;
		if (selected && selected->type == BuiNodeType::Element &&
			!selected->self_closing)
		{
			parent = selected_id;
		}
		else if (selected && selected->parent != InvalidBuiNode)
		{
			parent = selected->parent;
			insertion_sibling = selected_id;
		}
		const BuiNode *parent_node = tab->model.Find(parent);
		std::size_t index = parent_node ? parent_node->children.size() : 0;
		if (insertion_sibling != InvalidBuiNode && parent_node)
		{
			const auto sibling = std::find(parent_node->children.begin(),
										   parent_node->children.end(), insertion_sibling);
			if (sibling != parent_node->children.end())
				index = static_cast<std::size_t>(
							std::distance(parent_node->children.begin(), sibling)) +
						1;
		}

		BuiNodeTemplate node;
		std::string description;
		if (kind == "text")
		{
			node.type = BuiNodeType::Text;
			node.text = "Text";
			description = "Add Text";
		}
		else
		{
			node.tag = std::string(kind);
			node.attributes = {{"id", ""}, {"class", ""}};
			if (kind == "button")
				node.text = "Button";
			description = kind == "button" ? "Add Button" : "Add Container";
		}

		const bool applied = ui_docs_.ApplyEdit(*tab, description,
												[parent, index, node](BuiDocumentModel &model, BuiEditResult &result)
												{
													return model.InsertNode(parent, index, node, &result);
												});
		if (applied)
		{
			state_.SetStatus(description);
			ui_tree_expanded_.insert(tab->model.PathOf(parent));
			events_.Emit({EventType::UiDocumentModified, tab->id, description});
			ui_dirty_ = true;
		}
		return applied;
	}

	bool EditorApplication::DeleteUiNode()
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab)
			return false;
		const std::string path = tab->selection_path;
		const BuiNodeId id = tab->model.NodeAtPath(path);
		if (id == InvalidBuiNode || id == tab->model.GetRoot())
			return false;
		const BuiNode *node = tab->model.Find(id);
		const std::string label = node->type == BuiNodeType::Text
									  ? std::string("text")
									  : "<" + node->tag + ">";
		const bool applied = ui_docs_.ApplyEdit(*tab, "Delete " + label,
												[id](BuiDocumentModel &model, BuiEditResult &result)
												{
													return model.RemoveNode(id, &result);
												});
		if (applied)
		{
			state_.SetStatus("Deleted UI node " + label);
			events_.Emit({EventType::UiDocumentModified, tab->id, "Delete"});
			ui_dirty_ = true;
		}
		return applied;
	}

	bool EditorApplication::MoveUiNode(int direction)
	{
		UiDocumentTab *tab = ui_docs_.GetActive();
		if (!tab || direction == 0)
			return false;
		const BuiNodeId id = tab->model.NodeAtPath(tab->selection_path);
		const BuiNode *node = tab->model.Find(id);
		if (!node || node->parent == InvalidBuiNode)
			return false;
		const BuiNode *parent = tab->model.Find(node->parent);
		if (!parent)
			return false;
		const auto it = std::find(parent->children.begin(), parent->children.end(), id);
		if (it == parent->children.end())
			return false;
		const int current = static_cast<int>(std::distance(parent->children.begin(), it));
		const int target = current + direction;
		if (target < 0 || target >= static_cast<int>(parent->children.size()))
			return false;
		const BuiNodeId new_parent = node->parent;
		const bool applied = ui_docs_.ApplyEdit(*tab,
												direction < 0 ? "Move UI Node Up" : "Move UI Node Down",
												[id, new_parent, target](BuiDocumentModel &model, BuiEditResult &result)
												{
													return model.MoveNode(id, new_parent, static_cast<std::size_t>(target),
																		  &result);
												});
		if (applied)
		{
			state_.SetStatus("Moved UI node");
			events_.Emit({EventType::UiDocumentModified, tab->id, "Move"});
			ui_dirty_ = true;
		}
		return applied;
	}

	bool EditorApplication::UndoUiDocument()
	{
		std::string description;
		if (!ui_docs_.Undo(description))
			return false;
		state_.SetStatus("Undo: " + description);
		events_.Emit({EventType::UiDocumentModified,
					  ui_docs_.GetActive() ? ui_docs_.GetActive()->id : std::string(), "Undo"});
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::RedoUiDocument()
	{
		std::string description;
		if (!ui_docs_.Redo(description))
			return false;
		state_.SetStatus("Redo: " + description);
		events_.Emit({EventType::UiDocumentModified,
					  ui_docs_.GetActive() ? ui_docs_.GetActive()->id : std::string(), "Redo"});
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::CreateProject(const ProjectOptions &options, std::string &error)
	{
		if (!project_.Create(options, error))
		{
			BL_LOG(LogLevel::Error, "Project", "Failed to create project: " + error);
			return false;
		}

		const ProjectMetadata &metadata = project_.GetMetadata();
		default_project_location_ = options.directory;
		config_.Set("project", "default_location", default_project_location_.u8string());
		recent_projects_.Add({metadata.name, project_.GetProjectFile().u8string(),
							  ToString(metadata.kind), ProjectGenerator::GetCurrentDate()});
		config_.Save(config_path_);
		BL_LOG(LogLevel::Info, "Project",
			   "Created " + metadata.name + " with engine SDK " + metadata.engine_version +
				   " at " + metadata.engine_root.u8string());
		state_.SetStatus("Created project " + metadata.name);
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::OpenProjectDialog()
	{
		const std::filesystem::path initial = project_.HasProject()
												  ? project_.GetProjectRoot()
												  : default_project_location_;
		const std::filesystem::path path = ShowOpenProjectDialog(initial);
		if (path.empty())
			return false;
		return OpenProject(path);
	}

	bool EditorApplication::OpenProject(const std::filesystem::path &project_file)
	{
		std::string error;
		if (!project_.Open(project_file, error))
		{
			BL_LOG(LogLevel::Error, "Project", "Failed to open project: " + error);
			state_.SetStatus("Failed to open project: " + error);
			return false;
		}

		const ProjectMetadata &metadata = project_.GetMetadata();
		recent_projects_.Add({metadata.name, project_.GetProjectFile().u8string(),
							  ToString(metadata.kind), ProjectGenerator::GetCurrentDate()});
		config_.Save(config_path_);
		state_.SetStatus("Opened project " + metadata.name);
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::CloseProject()
	{
		if (!project_.HasProject())
			return false;
		const std::string name = project_.GetMetadata().name;
		project_.Close();
		state_.SetStatus("Closed project " + name);
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::SaveProject()
	{
		if (!project_.HasProject())
		{
			state_.SetStatus("There is no project to save");
			return false;
		}

		std::string error;
		if (!project_.Save(error))
		{
			BL_LOG(LogLevel::Error, "Project", "Failed to save project: " + error);
			state_.SetStatus("Failed to save project: " + error);
			return false;
		}
		state_.SetStatus("Saved project " + project_.GetMetadata().name);
		ui_dirty_ = true;
		return true;
	}

	bool EditorApplication::ValidateProject()
	{
		if (!project_.HasProject())
		{
			state_.SetStatus("There is no project to validate");
			return false;
		}

		const std::vector<ProjectIssue> issues = project_.Validate();
		for (const ProjectIssue &issue : issues)
		{
			const LogLevel level = issue.severity == ProjectIssueSeverity::Error
									   ? LogLevel::Error
								   : issue.severity == ProjectIssueSeverity::Warning
									   ? LogLevel::Warning
									   : LogLevel::Info;
			BL_LOG(level, "Project",
				   std::string("[") + ToString(issue.severity) + "] " + issue.message);
		}

		const bool has_errors = ProjectValidator::HasErrors(issues);
		if (issues.empty())
			state_.SetStatus("Project validation passed");
		else
			state_.SetStatus("Project validation: " + std::to_string(issues.size()) +
							 (has_errors ? " issue(s) - errors found" : " warning(s)"));
		ui_dirty_ = true;
		return !has_errors;
	}

	bool EditorApplication::UpdateProjectScaffolding()
	{
		std::string error;
		if (!project_.UpdateScaffolding(error))
		{
			BL_LOG(LogLevel::Error, "Project", "Failed to update scaffolding: " + error);
			state_.SetStatus("Failed to update scaffolding: " + error);
			return false;
		}
		state_.SetStatus("Updated project scaffolding to template " +
						 project_.GetMetadata().template_version);
		ui_dirty_ = true;
		return true;
	}

	void EditorApplication::SaveEditorConfig()
	{
		config_.Set("editor", "theme", theme_id_);
		config_.Set("editor", "version", EditorVersion);
		config_.SetInt("editor", "history_limit", static_cast<int>(history_.GetLimit()));
		config_.Set("project", "default_location", default_project_location_.u8string());
		config_.SetInt("project", "recent_limit",
					   static_cast<int>(recent_projects_.GetLimit()));
		config_.Set("project", "last",
					project_.HasProject() ? project_.GetProjectFile().u8string() : std::string());
		recent_projects_.Save(config_);
		panels_.SaveConfig(config_);

		if (Engine::Rendering::RenderingDevice *device = GetRenderingDevice())
		{
			const UINT dpi = GetDpiForSystem();
			const Engine::Rendering::SurfaceState surface = device->GetSurfaceState();
			if (dpi > 0 && surface.width > 0 && surface.height > 0)
			{
				config_.SetInt("window", "width",
							   MulDiv(static_cast<int>(surface.width), USER_DEFAULT_SCREEN_DPI,
									  static_cast<int>(dpi)));
				config_.SetInt("window", "height",
							   MulDiv(static_cast<int>(surface.height), USER_DEFAULT_SCREEN_DPI,
									  static_cast<int>(dpi)));
			}
		}

		config_.Save(config_path_);
	}

} // namespace BlackLight::Editor
