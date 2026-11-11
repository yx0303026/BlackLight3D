#include "editor/project/ProjectManager.h"

#include <system_error>
#include <utility>

namespace BlackLight::Editor
{

	ProjectManager::ProjectManager(EventBus &events) : events_(&events) {}

	bool ProjectManager::HasProject() const noexcept
	{
		return !metadata_.name.empty() && !file_.empty();
	}

	const ProjectMetadata &ProjectManager::GetMetadata() const noexcept { return metadata_; }

	const std::filesystem::path &ProjectManager::GetProjectRoot() const noexcept { return root_; }

	const std::filesystem::path &ProjectManager::GetProjectFile() const noexcept { return file_; }

	bool ProjectManager::Create(const ProjectOptions &options, std::string &error)
	{
		ProjectGenerationResult result = generator_.Generate(options);
		if (!result.success)
		{
			error = result.error;
			return false;
		}

		metadata_ = std::move(result.metadata);
		root_ = std::move(result.project_root);
		file_ = root_ / std::string(ProjectFileName);
		error.clear();

		Emit(EventType::ProjectChanged, metadata_.name, file_.u8string());
		Emit(EventType::ProjectOpened, metadata_.name, file_.u8string());
		Emit(EventType::StatusMessage, "Created project " + metadata_.name, {});
		return true;
	}

	bool ProjectManager::Open(const std::filesystem::path &project_file, std::string &error)
	{
		if (project_file.empty())
		{
			error = "No project file was supplied";
			return false;
		}

		std::error_code filesystem_error;
		std::filesystem::path path = project_file;
		if (std::filesystem::is_directory(path, filesystem_error) && !filesystem_error)
			path /= std::string(ProjectFileName);
		path = std::filesystem::weakly_canonical(path, filesystem_error);
		if (filesystem_error)
			path = std::filesystem::absolute(project_file, filesystem_error);
		if (filesystem_error)
		{
			error = "Invalid project path: " + filesystem_error.message();
			return false;
		}

		ProjectMetadata metadata;
		if (!LoadProjectFile(path, metadata, error))
			return false;

		const std::vector<ProjectIssue> issues = ProjectValidator::ValidateMetadata(metadata);
		if (ProjectValidator::HasErrors(issues))
		{
			error = ProjectValidator::FormatIssues(issues);
			return false;
		}

		metadata_ = std::move(metadata);
		file_ = path;
		root_ = path.parent_path();
		error.clear();

		Emit(EventType::ProjectChanged, metadata_.name, file_.u8string());
		Emit(EventType::ProjectOpened, metadata_.name, file_.u8string());
		Emit(EventType::StatusMessage, "Opened project " + metadata_.name, {});
		return true;
	}

	bool ProjectManager::Save(std::string &error)
	{
		if (!HasProject())
		{
			error = "No project is open";
			return false;
		}
		if (!SaveProjectFile(file_, metadata_, error))
			return false;
		Emit(EventType::ProjectChanged, metadata_.name, file_.u8string());
		Emit(EventType::StatusMessage, "Saved project " + metadata_.name, {});
		return true;
	}

	bool ProjectManager::UpdateScaffolding(std::string &error)
	{
		if (!HasProject())
		{
			error = "No project is open";
			return false;
		}

		ProjectGenerationResult result = generator_.UpdateScaffolding(metadata_, root_);
		if (!result.success)
		{
			error = result.error;
			return false;
		}

		metadata_ = std::move(result.metadata);
		error.clear();
		Emit(EventType::ProjectChanged, metadata_.name, file_.u8string());
		Emit(EventType::StatusMessage,
			"Updated project scaffolding to template " + metadata_.template_version, {});
		return true;
	}

	bool ProjectManager::IsTemplateUpdateAvailable() const
	{
		return HasProject() && generator_.GetTemplates().IsUpdateAvailable(metadata_);
	}

	void ProjectManager::Close()
	{
		if (!HasProject())
			return;
		const std::string name = metadata_.name;
		const std::string path = file_.u8string();
		metadata_ = {};
		root_.clear();
		file_.clear();
		Emit(EventType::ProjectClosed, name, path);
		Emit(EventType::ProjectChanged, {}, {});
		Emit(EventType::StatusMessage, "Closed project " + name, {});
	}

	std::vector<ProjectIssue> ProjectManager::Validate(
		std::string_view expected_template_version) const
	{
		if (!HasProject())
			return {{ProjectIssueSeverity::Error, "No project is open", {}}};

		const std::string expected = expected_template_version.empty()
			? generator_.GetTemplates().GetVersion(metadata_.kind)
			: std::string(expected_template_version);
		return ProjectValidator::ValidateDirectory(root_, metadata_, expected);
	}

	void ProjectManager::Emit(EventType type, std::string name, std::string detail) const
	{
		if (events_)
			events_->Emit({type, std::move(name), std::move(detail)});
	}

} // namespace BlackLight::Editor
