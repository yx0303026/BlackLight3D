#include "editor/project/ProjectGenerator.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <system_error>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Removes leading and trailing ASCII whitespace.
		std::string Trim(std::string_view text)
		{
			std::size_t begin = 0;
			std::size_t end = text.size();
			while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
				++begin;
			while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
				--end;
			return std::string(text.substr(begin, end - begin));
		}

		/// Uppercases an ASCII token.
		std::string ToUpper(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::toupper(character));
			});
			return text;
		}

		/// Converts a path to a forward-slash UTF-8 string for generated build files.
		std::string ToGenericPath(const std::filesystem::path &path)
		{
			return path.generic_u8string();
		}

		/// Writes text with LF line endings and creates the parent directory.
		bool WriteTextFile(const std::filesystem::path &path, std::string_view text,
			std::string &error)
		{
			std::error_code filesystem_error;
			const std::filesystem::path parent = path.parent_path();
			if (!parent.empty())
				std::filesystem::create_directories(parent, filesystem_error);
			if (filesystem_error)
			{
				error = "Failed to create " + parent.u8string() + ": " +
					filesystem_error.message();
				return false;
			}

			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			if (!output)
			{
				error = "Failed to open " + path.u8string() + " for writing";
				return false;
			}
			output.write(text.data(), static_cast<std::streamsize>(text.size()));
			output.flush();
			if (!output.good())
			{
				error = "Failed to write " + path.u8string();
				return false;
			}
			return true;
		}

		/// Returns whether the path exists as a directory.
		bool IsDirectory(const std::filesystem::path &path)
		{
			std::error_code error;
			return std::filesystem::is_directory(path, error) && !error;
		}

		/// Removes a partially generated project directory.
		void RemoveGeneratedDirectory(const std::filesystem::path &root)
		{
			std::error_code error;
			std::filesystem::remove_all(root, error);
		}

	} // namespace

	ProjectGenerator::ProjectGenerator() = default;

	ProjectGenerator::ProjectGenerator(TemplateLibrary templates)
		: templates_(std::move(templates))
	{
	}

	std::string ProjectGenerator::GetCurrentDate()
	{
		const std::time_t now =
			std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local{};
#if defined(_MSC_VER)
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		char buffer[16]{};
		if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local) == 0)
			return "1970-01-01";
		return buffer;
	}

	ProjectGenerationResult ProjectGenerator::Generate(const ProjectOptions &options) const
	{
		ProjectGenerationResult result;

		ProjectOptions normalized = options;
		normalized.name = Trim(options.name);
		normalized.name_space = Trim(options.name_space);
		if (normalized.name.empty())
		{
			result.error = "Enter a project name";
			return result;
		}
		if (!IsValidProjectName(normalized.name))
		{
			result.error = "'" + normalized.name +
				"' is not a valid project name. Use letters, digits, spaces, '-' and '_', "
				"and start with a letter.";
			return result;
		}
		if (normalized.directory.empty())
		{
			result.error = "Choose a location for the project";
			return result;
		}
		if (normalized.name_space.empty())
			normalized.name_space = DeriveNamespace(normalized.name);
		if (!IsValidNamespace(normalized.name_space))
		{
			result.error = "'" + normalized.name_space +
				"' is not a valid C++ namespace";
			return result;
		}
		if (normalized.engine_root.empty())
		{
			result.error = "The BlackLight engine SDK is required";
			return result;
		}

		const ProjectTemplate *project_template = templates_.Find(normalized.kind);
		if (!project_template)
		{
			result.error = "No template is registered for this project type";
			return result;
		}

		std::error_code filesystem_error;
		std::filesystem::path parent =
			std::filesystem::weakly_canonical(normalized.directory, filesystem_error);
		if (filesystem_error)
			parent = std::filesystem::absolute(normalized.directory, filesystem_error);
		if (filesystem_error)
		{
			result.error = "Invalid project location: " + filesystem_error.message();
			return result;
		}

		std::filesystem::path engine_root =
			std::filesystem::weakly_canonical(normalized.engine_root, filesystem_error);
		if (filesystem_error)
			engine_root = std::filesystem::absolute(normalized.engine_root, filesystem_error);
		if (filesystem_error)
			engine_root = normalized.engine_root;

		const std::filesystem::path root = parent / normalized.name;
		result.project_root = root;

		const bool exists = std::filesystem::exists(root, filesystem_error);
		if (filesystem_error)
		{
			result.error = "Failed to inspect " + root.u8string() + ": " +
				filesystem_error.message();
			return result;
		}
		if (exists)
		{
			if (!IsDirectory(root))
			{
				result.error = "A file already exists at " + root.u8string();
				return result;
			}
			const bool empty = std::filesystem::is_empty(root, filesystem_error);
			if (filesystem_error || !empty)
			{
				result.error = "The project directory is not empty: " + root.u8string();
				return result;
			}
		}

		if (!IsDirectory(engine_root))
		{
			result.warnings.push_back(
				"The BlackLight engine SDK does not exist yet: " + engine_root.u8string());
		}
		else if (!std::filesystem::is_regular_file(
			engine_root / "include" / "BlackLightApi.h", filesystem_error))
		{
			result.warnings.push_back(
				"The BlackLight engine SDK does not contain include/BlackLightApi.h: " +
				engine_root.u8string());
		}

		ProjectMetadata metadata;
		metadata.format_version = ProjectFormatVersion;
		metadata.name = normalized.name;
		metadata.id = normalized.id.empty() ? GenerateProjectId() : normalized.id;
		metadata.name_space = normalized.name_space;
		metadata.kind = normalized.kind;
		metadata.graphics_api = normalized.graphics_api;
		metadata.engine_root = engine_root;
		metadata.engine_version = normalized.engine_version.empty()
			? TemplateLibrary::GetEngineVersion()
			: normalized.engine_version;
		metadata.template_version = project_template->version;
		metadata.created = GetCurrentDate();
		metadata.dependencies.push_back(
			{"BlackLight", metadata.engine_version, ToGenericPath(engine_root)});

		std::filesystem::create_directories(root, filesystem_error);
		if (filesystem_error)
		{
			result.error = "Failed to create " + root.u8string() + ": " +
				filesystem_error.message();
			return result;
		}
		for (const std::string &directory : project_template->directories)
		{
			std::filesystem::create_directories(root / std::filesystem::u8path(directory),
				filesystem_error);
			if (filesystem_error)
			{
				result.error = "Failed to create " + directory + ": " +
					filesystem_error.message();
				RemoveGeneratedDirectory(root);
				return result;
			}
		}

		const TemplateContext context = BuildContext(metadata, *project_template);
		if (!WriteTemplateFiles(*project_template, context, root, false, result.files,
			result.error) ||
			!WriteProjectMetadata(metadata, root, result.error))
		{
			RemoveGeneratedDirectory(root);
			result.files.clear();
			return result;
		}

		result.metadata = std::move(metadata);
		result.success = true;
		return result;
	}

	ProjectGenerationResult ProjectGenerator::UpdateScaffolding(
		const ProjectMetadata &metadata, const std::filesystem::path &project_root) const
	{
		ProjectGenerationResult result;
		result.project_root = project_root;

		const ProjectTemplate *project_template = templates_.Find(metadata.kind);
		if (!project_template)
		{
			result.error = "No template is registered for this project type";
			return result;
		}
		if (project_root.empty() || !IsDirectory(project_root))
		{
			result.error = "The project directory does not exist: " + project_root.u8string();
			return result;
		}

		ProjectMetadata updated = metadata;
		updated.template_version = project_template->version;
		if (updated.engine_version.empty())
			updated.engine_version = TemplateLibrary::GetEngineVersion();
		if (updated.created.empty())
			updated.created = GetCurrentDate();
		bool has_blacklight = false;
		for (ProjectDependency &dependency : updated.dependencies)
		{
			if (dependency.name != "BlackLight")
				continue;
			has_blacklight = true;
			dependency.version = updated.engine_version;
			dependency.path = ToGenericPath(updated.engine_root);
		}
		if (!has_blacklight)
		{
			updated.dependencies.push_back(
				{"BlackLight", updated.engine_version, ToGenericPath(updated.engine_root)});
		}

		const TemplateContext context = BuildContext(updated, *project_template);
		if (!WriteTemplateFiles(*project_template, context, project_root, true, result.files,
			result.error) ||
			!WriteProjectMetadata(updated, project_root, result.error))
		{
			return result;
		}

		result.metadata = std::move(updated);
		result.success = true;
		return result;
	}

	const TemplateLibrary &ProjectGenerator::GetTemplates() const noexcept
	{
		return templates_;
	}

	TemplateContext ProjectGenerator::BuildContext(const ProjectMetadata &metadata,
		const ProjectTemplate &project_template) const
	{
		TemplateContext context;
		context.Set("PROJECT_NAME", metadata.name);
		context.Set("PROJECT_NAMESPACE", metadata.name_space);
		context.Set("PROJECT_TARGET", SanitizeTargetName(metadata.name));
		context.Set("PROJECT_ID", metadata.id);
		context.Set("PROJECT_KIND", ToString(metadata.kind));
		context.Set("PROJECT_KIND_DESCRIPTION", project_template.description);
		context.Set("GRAPHICS_API", ToString(metadata.graphics_api));
		context.Set("GRAPHICS_API_UPPER", ToUpper(ToString(metadata.graphics_api)));
		context.Set("GRAPHICS_BACKEND", GetEngineBackend(metadata.graphics_api));
		context.Set("GRAPHICS_API_NOTE", GetGraphicsApiNote(metadata.graphics_api));
		context.Set("ENGINE_SDK_ROOT", ToGenericPath(metadata.engine_root));
		context.Set("ENGINE_VERSION", metadata.engine_version.empty()
			? TemplateLibrary::GetEngineVersion()
			: metadata.engine_version);
		context.Set("TEMPLATE_VERSION", project_template.version);
		context.Set("CREATED_DATE", metadata.created.empty()
			? GetCurrentDate()
			: metadata.created);
		const std::string year = metadata.created.size() >= 4
			? metadata.created.substr(0, 4)
			: GetCurrentDate().substr(0, 4);
		context.Set("YEAR", year);
		context.Set("INCLUDE_GUARD", ToUpper(metadata.name_space) + "_PLUGIN_API_H");
		return context;
	}

	bool ProjectGenerator::WriteTemplateFiles(const ProjectTemplate &project_template,
		const TemplateContext &context, const std::filesystem::path &root,
		bool regenerable_only, std::vector<GeneratedFile> &files, std::string &error) const
	{
		for (const TemplateFile &file : project_template.files)
		{
			if (regenerable_only && file.policy != TemplateFilePolicy::Regenerate)
				continue;

			std::string render_error;
			const std::string relative_path = context.Render(file.path, &render_error);
			if (!render_error.empty())
			{
				error = "Template path '" + file.path + "': " + render_error;
				return false;
			}
			const std::string content = context.Render(file.content, &render_error);
			if (!render_error.empty())
			{
				error = "Template file '" + relative_path + "': " + render_error;
				return false;
			}

			const std::filesystem::path target = root / std::filesystem::u8path(relative_path);
			std::error_code filesystem_error;
			const std::filesystem::path relative =
				std::filesystem::relative(target, root, filesystem_error);
			if (filesystem_error || relative.empty() ||
				*relative.begin() == std::filesystem::path(".."))
			{
				error = "Template file escapes the project directory: " + relative_path;
				return false;
			}

			const bool existed = std::filesystem::exists(target, filesystem_error) &&
				!filesystem_error;
			if (!WriteTextFile(target, content, error))
				return false;
			files.push_back({target, regenerable_only && existed});
		}
		return true;
	}

	bool ProjectGenerator::WriteProjectMetadata(const ProjectMetadata &metadata,
		const std::filesystem::path &root, std::string &error) const
	{
		const std::filesystem::path path = root / std::string(ProjectFileName);
		if (!SaveProjectFile(path, metadata, error))
		{
			if (!error.empty())
				error = "Failed to write " + path.u8string() + ": " + error;
			return false;
		}
		return true;
	}

} // namespace BlackLight::Editor
