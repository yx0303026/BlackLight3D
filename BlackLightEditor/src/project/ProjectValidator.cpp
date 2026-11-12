#include "editor/project/ProjectValidator.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Appends one issue.
		void AddIssue(std::vector<ProjectIssue> &issues, ProjectIssueSeverity severity,
			std::string message, std::filesystem::path path = {})
		{
			issues.push_back({severity, std::move(message), std::move(path)});
		}

		/// Returns whether a directory exists and is a directory.
		bool IsDirectory(const std::filesystem::path &path)
		{
			std::error_code error;
			return std::filesystem::is_directory(path, error) && !error;
		}

		/// Returns whether a file exists.
		bool IsFile(const std::filesystem::path &path)
		{
			std::error_code error;
			return std::filesystem::is_regular_file(path, error) && !error;
		}

		/// Returns whether a directory contains at least one regular file.
		bool ContainsFile(const std::filesystem::path &directory)
		{
			std::error_code error;
			for (std::filesystem::recursive_directory_iterator it(directory, error), end;
				it != end && !error; it.increment(error))
			{
				if (it->is_regular_file(error) && !error)
					return true;
			}
			return false;
		}

	} // namespace

	const char *ToString(ProjectIssueSeverity severity) noexcept
	{
		switch (severity)
		{
		case ProjectIssueSeverity::Info: return "Info";
		case ProjectIssueSeverity::Warning: return "Warning";
		case ProjectIssueSeverity::Error: return "Error";
		}
		return "Unknown";
	}

	std::vector<ProjectIssue> ProjectValidator::ValidateMetadata(const ProjectMetadata &metadata)
	{
		std::vector<ProjectIssue> issues;

		if (metadata.format_version != ProjectFormatVersion)
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"Unsupported project format version " +
					std::to_string(metadata.format_version) + " (expected " +
					std::to_string(ProjectFormatVersion) + ")");
		}
		if (metadata.name.empty())
			AddIssue(issues, ProjectIssueSeverity::Error, "The project has no name");
		else if (!IsValidProjectName(metadata.name))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"The project name contains characters that are not allowed in a directory name: " +
					metadata.name);
		}
		if (metadata.id.empty())
			AddIssue(issues, ProjectIssueSeverity::Error, "The project has no identifier");
		if (metadata.name_space.empty())
			AddIssue(issues, ProjectIssueSeverity::Error, "The project has no C++ namespace");
		else if (!IsValidNamespace(metadata.name_space))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"The project namespace is not a valid C++ identifier: " + metadata.name_space);
		}
		if (metadata.engine_root.empty())
			AddIssue(issues, ProjectIssueSeverity::Error, "The project has no BlackLight engine SDK");
		if (metadata.engine_version.empty())
			AddIssue(issues, ProjectIssueSeverity::Warning, "The project does not record an engine version");
		if (metadata.template_version.empty())
			AddIssue(issues, ProjectIssueSeverity::Warning, "The project does not record a template version");

		for (const ProjectDependency &dependency : metadata.dependencies)
		{
			if (dependency.name.empty())
				AddIssue(issues, ProjectIssueSeverity::Warning, "A project dependency has no name");
			else if (dependency.version.empty())
			{
				AddIssue(issues, ProjectIssueSeverity::Warning,
					"Dependency '" + dependency.name + "' has no pinned version");
			}
		}

		const bool has_blacklight = std::any_of(metadata.dependencies.begin(),
			metadata.dependencies.end(),
			[](const ProjectDependency &dependency) { return dependency.name == "BlackLight"; });
		if (!has_blacklight)
			AddIssue(issues, ProjectIssueSeverity::Warning,
				"The project does not list BlackLight as a dependency");

		return issues;
	}

	std::vector<ProjectIssue> ProjectValidator::ValidateDirectory(
		const std::filesystem::path &project_directory,
		const ProjectMetadata &metadata,
		std::string_view expected_template_version)
	{
		std::vector<ProjectIssue> issues = ValidateMetadata(metadata);

		if (project_directory.empty())
		{
			AddIssue(issues, ProjectIssueSeverity::Error, "No project directory was supplied");
			return issues;
		}
		if (!IsDirectory(project_directory))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"The project directory does not exist: " + project_directory.u8string(),
				project_directory);
			return issues;
		}

		const std::filesystem::path project_file =
			project_directory / std::string(ProjectFileName);
		if (!IsFile(project_file))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"Missing project metadata file: " + project_file.u8string(), project_file);
		}

		const std::filesystem::path cmake_file = project_directory / "CMakeLists.txt";
		if (!IsFile(cmake_file))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"Missing CMakeLists.txt; the project cannot be configured", cmake_file);
		}

		const std::filesystem::path source_directory = project_directory / "src";
		if (!IsDirectory(source_directory))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"Missing src directory: " + source_directory.u8string(), source_directory);
		}
		else if (!ContainsFile(source_directory))
		{
			AddIssue(issues, ProjectIssueSeverity::Warning,
				"The src directory contains no source files", source_directory);
		}

		const std::filesystem::path assets_directory = project_directory / "assets";
		if (!IsDirectory(assets_directory))
		{
			AddIssue(issues, ProjectIssueSeverity::Warning,
				"Missing assets directory: " + assets_directory.u8string(), assets_directory);
		}

		const std::filesystem::path engine_api_header =
			metadata.engine_root / "include" / "BlackLightApi.h";
		if (metadata.engine_root.empty())
		{
			// Already reported by ValidateMetadata.
		}
		else if (!IsDirectory(metadata.engine_root))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"The BlackLight engine SDK does not exist: " +
					metadata.engine_root.u8string(), metadata.engine_root);
		}
		else if (!IsFile(engine_api_header))
		{
			AddIssue(issues, ProjectIssueSeverity::Error,
				"The BlackLight engine SDK does not contain include/BlackLightApi.h: " +
					metadata.engine_root.u8string(), engine_api_header);
		}

		if (!expected_template_version.empty() &&
			metadata.template_version != expected_template_version)
		{
			AddIssue(issues, ProjectIssueSeverity::Warning,
				"The project was generated by template version '" +
					metadata.template_version + "'; version '" +
					std::string(expected_template_version) + "' is available");
		}

		const std::vector<ProjectIssue> ui_issues = ValidateUiDirectory(project_directory);
		issues.insert(issues.end(), ui_issues.begin(), ui_issues.end());

		return issues;
	}

	std::vector<ProjectIssue> ProjectValidator::ValidateUiDirectory(
		const std::filesystem::path &project_directory)
	{
		std::vector<ProjectIssue> issues;
		const std::filesystem::path ui_directory = project_directory / "ui";
		if (!IsDirectory(ui_directory))
		{
			AddIssue(issues, ProjectIssueSeverity::Warning,
				"Missing ui directory; create ui/Main.bui for the default interface",
				ui_directory);
			return issues;
		}

		bool has_document = false;
		std::error_code error;
		for (std::filesystem::directory_iterator it(ui_directory, error), end;
			it != end && !error; it.increment(error))
		{
			if (!it->is_regular_file(error))
				continue;
			const std::string extension = it->path().extension().u8string();
			if (extension == ".bui")
			{
				has_document = true;
				break;
			}
		}
		if (!has_document)
		{
			AddIssue(issues, ProjectIssueSeverity::Warning,
				"The ui directory contains no .bui documents", ui_directory);
		}
		return issues;
	}

	bool ProjectValidator::HasErrors(const std::vector<ProjectIssue> &issues) noexcept
	{
		return std::any_of(issues.begin(), issues.end(), [](const ProjectIssue &issue)
		{
			return issue.severity == ProjectIssueSeverity::Error;
		});
	}

	bool ProjectValidator::HasWarnings(const std::vector<ProjectIssue> &issues) noexcept
	{
		return std::any_of(issues.begin(), issues.end(), [](const ProjectIssue &issue)
		{
			return issue.severity == ProjectIssueSeverity::Warning;
		});
	}

	std::string ProjectValidator::FormatIssues(const std::vector<ProjectIssue> &issues)
	{
		std::string text;
		for (const ProjectIssue &issue : issues)
		{
			if (!text.empty())
				text += '\n';
			text += '[';
			text += ToString(issue.severity);
			text += "] ";
			text += issue.message;
			if (!issue.path.empty())
			{
				text += " (";
				text += issue.path.u8string();
				text += ')';
			}
		}
		return text;
	}

} // namespace BlackLight::Editor
