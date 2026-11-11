#pragma once

#include "editor/core/EditorEvent.h"
#include "editor/project/ProjectFile.h"
#include "editor/project/ProjectGenerator.h"
#include "editor/project/ProjectValidator.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Owns the open project, its metadata, and the generator that creates projects.
	///
	/// The manager performs no UI work and emits project events through the bus
	/// supplied at construction, so it can be tested without RmlUi.
	class ProjectManager final
	{
	public:
		/// Binds the manager to the event bus that receives its notifications.
		explicit ProjectManager(EventBus &events);

		/// Returns whether a project is open.
		bool HasProject() const noexcept;
		/// Returns the open project metadata, or default metadata when none is open.
		const ProjectMetadata &GetMetadata() const noexcept;
		/// Returns the project directory, or an empty path when none is open.
		const std::filesystem::path &GetProjectRoot() const noexcept;
		/// Returns the `.blacklight` path, or an empty path when none is open.
		const std::filesystem::path &GetProjectFile() const noexcept;

		/// Generates a new project and opens it. Returns false with `error` on failure.
		bool Create(const ProjectOptions &options, std::string &error);

		/// Opens an existing project file or project directory.
		bool Open(const std::filesystem::path &project_file, std::string &error);

		/// Writes the open project metadata.
		bool Save(std::string &error);

		/// Rewrites editor-managed scaffolding files and bumps the template version.
		bool UpdateScaffolding(std::string &error);

		/// Returns whether a newer template is available for the open project.
		bool IsTemplateUpdateAvailable() const;

		/// Closes the open project.
		void Close();

		/// Validates the open project. Pass an empty version to use the built-in template.
		std::vector<ProjectIssue> Validate(std::string_view expected_template_version = {}) const;

	private:
		void Emit(EventType type, std::string name = {}, std::string detail = {}) const;

		EventBus *events_ = nullptr;
		ProjectGenerator generator_;
		ProjectMetadata metadata_;
		std::filesystem::path root_;
		std::filesystem::path file_;
	};

} // namespace BlackLight::Editor
