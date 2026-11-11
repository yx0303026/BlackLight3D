#pragma once

#include "editor/project/ProjectTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Records one library that a generated project depends on.
	struct ProjectDependency
	{
		std::string name;
		std::string version;
		std::string path;
	};

	/// Describes the metadata persisted in a `.blacklight` project file.
	struct ProjectMetadata
	{
		int format_version = ProjectFormatVersion;
		std::string name;
		std::string id;
		std::string name_space;
		ProjectKind kind = ProjectKind::WindowsApp;
		GraphicsApi graphics_api = GraphicsApi::OpenGL;
		/// Directory of the prebuilt engine SDK recorded at generation time.
		std::filesystem::path engine_root;
		std::string engine_version;
		std::string template_version;
		std::string created;
		std::vector<ProjectDependency> dependencies;
	};

	/// Loads project metadata from a `.blacklight` file.
	/// Returns false and fills `error` when the file is missing or malformed.
	bool LoadProjectFile(const std::filesystem::path &path, ProjectMetadata &metadata,
		std::string &error);

	/// Writes project metadata using the line-based `.blacklight` format.
	bool SaveProjectFile(const std::filesystem::path &path, const ProjectMetadata &metadata,
		std::string &error);

} // namespace BlackLight::Editor
