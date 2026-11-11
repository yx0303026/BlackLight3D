#pragma once

#include "editor/project/ProjectFile.h"
#include "editor/project/ProjectTemplates.h"

#include <filesystem>
#include <string>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes one file written by the project generator.
	struct GeneratedFile
	{
		std::filesystem::path path;
		/// True when an existing editor-managed file was rewritten.
		bool rewritten = false;
	};

	/// Reports the outcome of project creation or scaffolding updates.
	struct ProjectGenerationResult
	{
		bool success = false;
		std::string error;
		std::filesystem::path project_root;
		ProjectMetadata metadata;
		std::vector<GeneratedFile> files;
		std::vector<std::string> warnings;
	};

	/// Generates project directories and files from the built-in templates.
	class ProjectGenerator final
	{
	public:
		ProjectGenerator();
		explicit ProjectGenerator(TemplateLibrary templates);

		/// Creates a new project. The target directory must not exist or must be empty.
		ProjectGenerationResult Generate(const ProjectOptions &options) const;

		/// Rewrites editor-managed files (CMake and presets) and bumps the template version.
		/// User-owned source files are never touched.
		ProjectGenerationResult UpdateScaffolding(const ProjectMetadata &metadata,
			const std::filesystem::path &project_root) const;

		/// Returns the embedded template library.
		const TemplateLibrary &GetTemplates() const noexcept;

		/// Returns the current local date as `YYYY-MM-DD`.
		static std::string GetCurrentDate();

	private:
		TemplateContext BuildContext(const ProjectMetadata &metadata,
			const ProjectTemplate &project_template) const;
		bool WriteTemplateFiles(const ProjectTemplate &project_template,
			const TemplateContext &context, const std::filesystem::path &root,
			bool regenerable_only, std::vector<GeneratedFile> &files,
			std::string &error) const;
		bool WriteProjectMetadata(const ProjectMetadata &metadata,
			const std::filesystem::path &root, std::string &error) const;

		TemplateLibrary templates_;
	};

} // namespace BlackLight::Editor
