#pragma once

#include "editor/project/EngineSdk.h"
#include "editor/project/ProjectTypes.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace BlackLight::Editor
{

	class EditorUi;

	/// Drives the project creation dialog embedded in `Editor.bui`.
	///
	/// The wizard owns only transient form state; the engine SDK is prepared in the
	/// background and project generation is delegated to the create handler supplied
	/// by the application.
	class ProjectWizard final
	{
	public:
		/// Receives the requested options and returns false with an error message.
		using CreateHandler = std::function<bool(const ProjectOptions &options,
			std::string &error)>;
		/// Opens a folder picker for the supplied initial directory.
		using BrowseHandler = std::function<std::filesystem::path(
			const std::filesystem::path &initial_directory)>;

		/// Binds the wizard to the editor UI and installs its listeners.
		void Initialize(EditorUi &ui);
		/// Detaches listeners; call before the document is closed.
		void Shutdown(EditorUi &ui);

		/// Supplies the project creation callback.
		void SetCreateHandler(CreateHandler handler);
		/// Supplies the folder picker callback.
		void SetBrowseHandler(BrowseHandler handler);

		/// Shows the dialog with sensible defaults.
		void Show(std::string default_name, const std::filesystem::path &default_location);
		/// Hides the dialog and cancels a pending engine download.
		void Hide();
		/// Returns whether the dialog is visible.
		bool IsVisible() const noexcept;

		/// Advances the background engine download; call once per frame.
		void Update();

		/// Selects a project type programmatically.
		void SelectKind(ProjectKind kind);
		/// Selects a graphics API programmatically.
		void SelectGraphicsApi(GraphicsApi api);

	private:
		void HandleAction(std::string_view action);
		void BeginCreate();
		void CreateProject();
		void BrowseLocation();
		void RefreshSelection();
		void RefreshKindHint();
		void RefreshEngineInfo();
		void SetBusy(bool busy);
		void ShowError(std::string_view message);
		void ClearError();
		void HandleNameChanged();
		std::string GetInput(std::string_view id) const;
		void SetInput(std::string_view id, std::string_view value);

		EditorUi *ui_ = nullptr;
		CreateHandler create_handler_;
		BrowseHandler browse_handler_;
		EngineSdkInstaller sdk_installer_;
		ProjectOptions pending_options_;
		ProjectKind kind_ = ProjectKind::WindowsApp;
		GraphicsApi graphics_api_ = GraphicsApi::OpenGL;
		std::string last_derived_namespace_;
		std::string last_sdk_status_;
		bool busy_ = false;
		bool visible_ = false;
	};

} // namespace BlackLight::Editor
