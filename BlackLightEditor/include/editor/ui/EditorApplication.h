#pragma once

#include "editor/core/CommandHistory.h"
#include "editor/core/CommandRegistry.h"
#include "editor/core/EditorConfig.h"
#include "editor/core/EditorEvent.h"
#include "editor/core/EditorState.h"
#include "editor/core/EditorTheme.h"
#include "editor/project/ProjectManager.h"
#include "editor/project/RecentProjects.h"
#include "editor/ui/EditorMenu.h"
#include "editor/ui/EditorUi.h"
#include "editor/ui/LogBuffer.h"
#include "editor/ui/PanelManager.h"
#include "editor/ui/ProjectWizard.h"
#include "engine/Window.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Rml
{
	class Event;
}

namespace BlackLight::Editor
{

	/// Describes one entry shown by the stage-one asset browser placeholder.
	struct AssetEntry
	{
		std::string name;
		std::string kind;
	};

	/// Hosts the editor window and wires the stage-one core framework together.
	class EditorApplication final : public Engine::Window
	{
	public:
		/// Creates the editor host using a pre-loaded window configuration.
		EditorApplication(Engine::WindowConfig window_config,
			std::filesystem::path config_path);
		~EditorApplication() override;

		EditorApplication(const EditorApplication &) = delete;
		EditorApplication &operator=(const EditorApplication &) = delete;

	protected:
		bool OnInitialize() override;
		void OnUpdate() override;
		void OnShutdown() override;

	private:
		void RegisterCommands();
		void RegisterPanels();
		void RegisterMenus();

		void OnEditorEvent(const EditorEvent &event);
		void ProcessShortcuts();
		void UpdateUi();
		void UpdateStatusBar();
		void UpdateToolbarState();

		void ApplyTheme(std::string_view theme_id, bool announce);
		const EditorTheme &CurrentTheme() const;

		bool NewWorkspace();
		bool OpenWorkspaceDialog();
		bool OpenWorkspace(const std::filesystem::path &path);
		bool SaveWorkspace(bool save_as);
		bool CloseWorkspace();
		bool AddObject();
		bool DeleteSelected();
		bool ConfirmDiscardIfNeeded();
		bool HasSelectedObject() const;

		void ShowCreateProjectWizard();
		bool CreateProject(const ProjectOptions &options, std::string &error);
		bool OpenProjectDialog();
		bool OpenProject(const std::filesystem::path &project_file);
		bool CloseProject();
		bool SaveProject();
		bool ValidateProject();
		bool UpdateProjectScaffolding();

		void HandlePanelAction(std::string_view action);
		void HandleToolbarClick(Rml::Event &event);

		std::string RenderScenePanel() const;
		std::string RenderAssetsPanel() const;
		std::string RenderInspectorPanel() const;
		std::string RenderOutputPanel() const;
		std::string RenderHistoryPanel() const;
		std::string RenderViewportPanel() const;

		void SaveEditorConfig();

		std::filesystem::path config_path_;
		EditorConfig config_;
		EventBus events_;
		EditorState state_;
		CommandRegistry commands_;
		CommandHistory history_;
		ThemeRegistry themes_;
		EditorUi ui_;
		PanelManager panels_;
		EditorMenu menus_;
		ProjectManager project_;
		RecentProjects recent_projects_;
		ProjectWizard wizard_;
		LogBuffer logs_;
		std::vector<AssetEntry> assets_;
		std::filesystem::path default_project_location_;
		std::string theme_id_ = "dark";
		std::size_t last_log_revision_ = 0;
		bool icon_font_loaded_ = false;
		bool ui_dirty_ = true;
	};

} // namespace BlackLight::Editor
