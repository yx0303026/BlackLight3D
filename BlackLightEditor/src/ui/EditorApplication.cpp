#include "editor/ui/EditorApplication.h"

#include "blacklight_editor_bui.h"
#include "editor/core/Commands.h"
#include "editor/ui/EditorIcons.h"
#include "editor/ui/Win32Dialogs.h"
#include "engine/Input.h"
#include "engine/Logger.h"
#include <RmlUi/Core.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <exception>
#include <initializer_list>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

using BlackLight::Engine::LogLevel;
using BlackLight::Engine::Logger;

namespace BlackLight::Editor
{

	namespace
	{

		constexpr const char *EditorVersion = "0.1.0";

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
			case LogLevel::Warning: return "log-warning";
			case LogLevel::Error:
			case LogLevel::Critical: return "log-error";
			case LogLevel::Trace:
			case LogLevel::Debug: return "log-trace";
			default: return "log-info";
			}
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
		Logger::Get().AddSink([this](const Engine::LogRecord &record) { logs_.Add(record); });
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

		events_.SubscribeAll([this](const EditorEvent &event) { OnEditorEvent(event); });
		RegisterCommands();
		RegisterPanels();
		RegisterMenus();

		wizard_.Initialize(ui_);
		wizard_.SetCreateHandler([this](const ProjectOptions &options, std::string &error)
		{
			return CreateProject(options, error);
		});
		wizard_.SetBrowseHandler([](const std::filesystem::path &initial)
		{
			return ShowBrowseFolderDialog(initial, L"Select the project location");
		});

		panels_.SetActionHandler([this](std::string_view action) { HandlePanelAction(action); });
		panels_.ApplyConfig(config_);
		menus_.Initialize(ui_);
		panels_.Initialize(ui_);
		ui_.AddListener("toolbar", "toolbar", "click",
			[this](Rml::Event &event) { HandleToolbarClick(event); });

		last_log_revision_ = logs_.GetRevision();
		state_.SetStatus("Ready - Stage 2 project system");
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

		if (!ui_dirty_)
			return;
		ui_dirty_ = false;
		UpdateUi();
	}

	void EditorApplication::OnShutdown()
	{
		Logger::Get().ClearSinks();
		SaveEditorConfig();
		wizard_.Shutdown(ui_);
		menus_.Shutdown(ui_);
		panels_.Shutdown(ui_);
		ui_.Shutdown();
		BL_LOG(LogLevel::Info, "Editor", "BlackLightEditor shutdown complete");
		Logger::Get().Flush();
	}

	void EditorApplication::RegisterCommands()
	{
		const auto add = [this](CommandInfo info) { commands_.Register(std::move(info)); };

		add(CommandInfo("file.new", "New Workspace", "File",
			[this] { return NewWorkspace(); }).Shortcut('N', true));
		add(CommandInfo("file.open", "Open Workspace...", "File",
			[this] { return OpenWorkspaceDialog(); }).Shortcut('O', true));
		add(CommandInfo("file.save", "Save Workspace", "File",
			[this] { return SaveWorkspace(false); }).Shortcut('S', true)
			.Enabled([this] { return state_.HasDocument(); }));
		add(CommandInfo("file.save_as", "Save Workspace As...", "File",
			[this] { return SaveWorkspace(true); }).Shortcut('S', true, true)
			.Enabled([this] { return state_.HasDocument(); }));
		add(CommandInfo("file.close", "Close Workspace", "File",
			[this] { return CloseWorkspace(); }).Shortcut('W', true)
			.Enabled([this] { return state_.HasDocument(); }));

		add(CommandInfo("project.new", "New Project...", "File", [this]
		{
			ShowCreateProjectWizard();
			return true;
		}).Shortcut('N', true, true));
		add(CommandInfo("project.open", "Open Project...", "File",
			[this] { return OpenProjectDialog(); }).Shortcut('O', true, true));
		add(CommandInfo("project.save", "Save Project", "File",
			[this] { return SaveProject(); })
			.Enabled([this] { return project_.HasProject(); }));
		add(CommandInfo("project.close", "Close Project", "File",
			[this] { return CloseProject(); })
			.Enabled([this] { return project_.HasProject(); }));

		add(CommandInfo("app.quit", "Exit", "File", [this]
		{
			if (!ConfirmDiscardIfNeeded())
				return false;
			events_.Emit({EventType::QuitRequested, {}, {}});
			return true;
		}).Shortcut('Q', true));

		add(CommandInfo("edit.undo", "Undo", "Edit",
			[this] { return history_.Undo(); }).Shortcut('Z', true)
			.Enabled([this] { return history_.CanUndo(); }));
		add(CommandInfo("edit.redo", "Redo", "Edit",
			[this] { return history_.Redo(); }).Shortcut('Y', true)
			.Enabled([this] { return history_.CanRedo(); }));
		add(CommandInfo("edit.redo_alternate", "Redo", "Edit",
			[this] { return history_.Redo(); }).Shortcut('Z', true, true)
			.Enabled([this] { return history_.CanRedo(); }));
		add(CommandInfo("edit.clear_history", "Clear Command History", "Tools", [this]
		{
			history_.Clear();
			state_.SetStatus("Command history cleared");
			return true;
		}));

		add(CommandInfo("object.add", "Add Object", "Edit",
			[this] { return AddObject(); })
			.Enabled([this] { return state_.HasDocument(); }));
		add(CommandInfo("object.delete", "Delete Selected Object", "Edit",
			[this] { return DeleteSelected(); }).Shortcut(VK_DELETE)
			.Enabled([this] { return HasSelectedObject(); }));

		add(CommandInfo("output.clear", "Clear Output", "Tools", [this]
		{
			logs_.Clear();
			state_.SetStatus("Output cleared");
			return true;
		}));

		add(CommandInfo("view.panel.scene", "Scene Panel", "View",
			[this] { return panels_.TogglePanel("scene"); }).Shortcut('1', true)
			.Checked([this] { return panels_.IsVisible("scene"); }));
		add(CommandInfo("view.panel.assets", "Assets Panel", "View",
			[this] { return panels_.TogglePanel("assets"); }).Shortcut('2', true)
			.Checked([this] { return panels_.IsVisible("assets"); }));
		add(CommandInfo("view.panel.inspector", "Inspector Panel", "View",
			[this] { return panels_.TogglePanel("inspector"); }).Shortcut('3', true)
			.Checked([this] { return panels_.IsVisible("inspector"); }));
		add(CommandInfo("view.panel.output", "Output Panel", "View",
			[this] { return panels_.TogglePanel("output"); }).Shortcut('4', true)
			.Checked([this] { return panels_.IsVisible("output"); }));
		add(CommandInfo("view.panel.history", "History Panel", "View",
			[this] { return panels_.TogglePanel("history"); }).Shortcut('5', true)
			.Checked([this] { return panels_.IsVisible("history"); }));

		for (const auto &theme : themes_.GetThemes())
		{
			const std::string theme_id = theme.id;
			add(CommandInfo("theme." + theme_id, "Theme: " + theme.name, "View",
				[this, theme_id]
				{
					ApplyTheme(theme_id, true);
					return true;
				}).Checked([this, theme_id] { return theme_id_ == theme_id; }));
		}

		add(CommandInfo("layout.reset", "Reset Layout", "View", [this]
		{
			panels_.ResetLayout();
			state_.SetStatus("Layout reset to defaults");
			return true;
		}));

		add(CommandInfo("mode.edit", "Edit Mode", "Tools", [this]
		{
			state_.SetMode(EditorMode::Edit);
			state_.SetStatus("Mode: Edit");
			return true;
		}).Checked([this] { return state_.GetMode() == EditorMode::Edit; }));
		add(CommandInfo("mode.play", "Play Mode", "Tools", [this]
		{
			state_.SetMode(EditorMode::Play);
			state_.SetStatus("Mode: Play");
			return true;
		}).Checked([this] { return state_.GetMode() == EditorMode::Play; }));
		add(CommandInfo("mode.pause", "Pause Mode", "Tools", [this]
		{
			state_.SetMode(EditorMode::Paused);
			state_.SetStatus("Mode: Paused");
			return true;
		}).Checked([this] { return state_.GetMode() == EditorMode::Paused; }));

		add(CommandInfo("project.validate", "Validate Project", "Build",
			[this] { return ValidateProject(); })
			.Enabled([this] { return project_.HasProject(); }));
		add(CommandInfo("project.update_template", "Update Project Scaffolding", "Tools",
			[this] { return UpdateProjectScaffolding(); })
			.Enabled([this] { return project_.HasProject(); }));

		add(CommandInfo("build.configure", "Configure Build", "Build", [this]
		{
			BL_LOG(LogLevel::Info, "Build", "Build configuration arrives in stage 4");
			state_.SetStatus("Build configuration arrives in stage 4");
			return true;
		}));
		add(CommandInfo("build.build", "Build Project", "Build", [this]
		{
			BL_LOG(LogLevel::Info, "Build", "CMake and Ninja integration arrives in stage 4");
			state_.SetStatus("CMake and Ninja integration arrives in stage 4");
			return true;
		}));
		add(CommandInfo("build.run", "Run Project", "Build", [this]
		{
			BL_LOG(LogLevel::Info, "Build", "Runtime preview arrives in stage 3");
			state_.SetStatus("Runtime preview arrives in stage 3");
			return true;
		}));

		add(CommandInfo("help.about", "About BlackLightEditor", "Help", [this]
		{
			BL_LOG(LogLevel::Info, "Editor",
				std::string("BlackLightEditor ") + EditorVersion + " - Stage 1 editor core framework");
			BL_LOG(LogLevel::Info, "Editor",
				"Application shell, event bus, configuration, editor state, commands, "
				"command history, menus, toolbar, shortcuts, dockable panels, and themes");
			state_.SetStatus(std::string("BlackLightEditor ") + EditorVersion +
				" - Stage 1 editor core framework");
			return true;
		}));
	}

	void EditorApplication::RegisterPanels()
	{
		panels_.RegisterPanel({"scene", "Scene", DockRegion::Left, true, true,
			[this] { return RenderScenePanel(); }});
		panels_.RegisterPanel({"assets", "Assets", DockRegion::Left, true, true,
			[this] { return RenderAssetsPanel(); }});
		panels_.RegisterPanel({"inspector", "Inspector", DockRegion::Right, true, true,
			[this] { return RenderInspectorPanel(); }});
		panels_.RegisterPanel({"output", "Output", DockRegion::Bottom, true, true,
			[this] { return RenderOutputPanel(); }});
		panels_.RegisterPanel({"history", "History", DockRegion::Bottom, true, true,
			[this] { return RenderHistoryPanel(); }});
		panels_.RegisterPanel({"viewport", "Viewport", DockRegion::Center, true, false,
			[this] { return RenderViewportPanel(); }});
	}

	void EditorApplication::RegisterMenus()
	{
		menus_.AddMenu("file", "File",
			{"project.new", "project.open", "", "file.new", "file.open", "",
				"file.save", "file.save_as", "", "file.close", "project.save", "project.close",
				"", "app.quit"});
		menus_.AddMenu("edit", "Edit",
			{"edit.undo", "edit.redo", "", "object.add", "object.delete"});
		menus_.AddMenu("view", "View",
			{"view.panel.scene", "view.panel.assets", "view.panel.inspector",
				"view.panel.output", "view.panel.history", "",
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
		UpdateStatusBar();
		UpdateToolbarState();
	}

	void EditorApplication::UpdateStatusBar()
	{
		ui_.SetText("status_message", state_.GetStatus());
		ui_.SetText("status_undo",
			history_.CanUndo() ? "Undo: " + history_.GetUndoName() : std::string("Undo: -"));

		std::string document = "No workspace";
		if (state_.HasDocument())
			document = state_.GetDocumentName() + (state_.IsModified() ? " *" : "");
		ui_.SetText("status_document", document);
		ui_.SetText("status_project",
			project_.HasProject() ? project_.GetMetadata().name : std::string("No project"));

		const std::size_t object_count = state_.GetObjects().size();
		ui_.SetText("status_objects", std::to_string(object_count) +
			(object_count == 1 ? " object" : " objects"));
		ui_.SetText("status_mode", ToString(state_.GetMode()));
		ui_.SetText("status_theme", CurrentTheme().name);

		const EditorMode mode = state_.GetMode();
		ui_.SetClass("status_mode", "play", mode == EditorMode::Play);
		ui_.SetClass("status_mode", "pause", mode == EditorMode::Paused);
	}

	void EditorApplication::UpdateToolbarState()
	{
		ui_.SetClass("tool_save", "disabled", !state_.HasDocument());
		ui_.SetClass("tool_add", "disabled", !state_.HasDocument());
		ui_.SetClass("tool_delete", "disabled", !HasSelectedObject());
		ui_.SetClass("tool_undo", "disabled", !history_.CanUndo());
		ui_.SetClass("tool_redo", "disabled", !history_.CanRedo());

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

	bool EditorApplication::HasSelectedObject() const
	{
		const Selection &selection = state_.GetSelection();
		return selection.kind == SelectionKind::Object &&
			state_.FindObject(selection.id) != nullptr;
	}

	void EditorApplication::HandlePanelAction(std::string_view action)
	{
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
		}
	}

	void EditorApplication::HandleToolbarClick(Rml::Event &event)
	{
		Rml::Element *toolbar = ui_.Find("toolbar");
		const std::string command = EditorUi::FindAttribute(event.GetTargetElement(), toolbar,
			"data-command");
		if (!command.empty())
			commands_.Invoke(command);
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

	std::string EditorApplication::RenderInspectorPanel() const
	{
		if (!state_.HasDocument())
			return "<div class=\"empty\">No workspace is open.</div>";

		const auto row = [](std::string_view label, std::string_view value)
		{
			return "<div class=\"inspector-row\"><span class=\"inspector-label\">" +
				EditorUi::Escape(label) + "</span><span class=\"inspector-value\">" +
				EditorUi::Escape(value) + "</span></div>";
		};

		std::string html = "<div class=\"inspector-section\">Workspace</div>";
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
				[&selection](const AssetEntry &asset) { return asset.name == selection.id; });
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
