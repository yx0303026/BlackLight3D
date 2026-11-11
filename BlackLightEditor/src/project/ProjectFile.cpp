#include "editor/project/ProjectFile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace BlackLight::Editor
{

	namespace
	{

		constexpr const char *FileHeaderPrefix = "blacklight-project";

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

		/// Replaces line breaks so values stay on one line in the line-based format.
		std::string SanitizeValue(std::string_view text)
		{
			std::string result = Trim(text);
			for (char &character : result)
			{
				if (character == '\r' || character == '\n' || character == '\t')
					character = ' ';
			}
			return result;
		}

		/// Splits a dependency value into name, version, and path.
		ProjectDependency ParseDependency(std::string_view text)
		{
			ProjectDependency dependency;
			std::vector<std::string> parts;
			std::size_t start = 0;
			while (start <= text.size())
			{
				const std::size_t separator = text.find('|', start);
				const std::size_t end = separator == std::string_view::npos ? text.size() : separator;
				parts.push_back(Trim(text.substr(start, end - start)));
				if (separator == std::string_view::npos)
					break;
				start = separator + 1;
			}

			if (!parts.empty())
				dependency.name = parts[0];
			if (parts.size() > 1)
				dependency.version = parts[1];
			if (parts.size() > 2)
				dependency.path = parts[2];
			return dependency;
		}

	} // namespace

	bool LoadProjectFile(const std::filesystem::path &path, ProjectMetadata &metadata,
		std::string &error)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
		{
			error = "The project file does not exist: " + path.u8string();
			return false;
		}

		ProjectMetadata parsed;
		bool header_seen = false;
		bool first_line = true;
		std::string line;
		while (std::getline(input, line))
		{
			if (first_line)
			{
				first_line = false;
				if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
					static_cast<unsigned char>(line[1]) == 0xBB &&
					static_cast<unsigned char>(line[2]) == 0xBF)
				{
					line.erase(0, 3);
				}
			}
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			const std::string trimmed = Trim(line);
			if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';')
				continue;

			if (!header_seen)
			{
				std::istringstream header(trimmed);
				std::string token;
				int version = 0;
				if (!(header >> token >> version) || token != FileHeaderPrefix)
				{
					error = "Missing blacklight-project header in " + path.u8string();
					return false;
				}
				if (version != ProjectFormatVersion)
				{
					error = "Unsupported project format version " + std::to_string(version) +
						" in " + path.u8string();
					return false;
				}
				parsed.format_version = version;
				header_seen = true;
				continue;
			}

			const std::size_t separator = trimmed.find('=');
			if (separator == std::string::npos)
			{
				error = "Expected 'key = value' in " + path.u8string() + ": " + trimmed;
				return false;
			}

			const std::string key = Trim(std::string_view(trimmed).substr(0, separator));
			const std::string value = Trim(std::string_view(trimmed).substr(separator + 1));
			if (key.empty())
			{
				error = "Empty project key in " + path.u8string();
				return false;
			}

			if (key == "name")
				parsed.name = SanitizeValue(value);
			else if (key == "id")
				parsed.id = value;
			else if (key == "namespace")
				parsed.name_space = value;
			else if (key == "kind")
			{
				if (!ProjectKindFromString(value, parsed.kind))
				{
					error = "Unknown project kind '" + value + "' in " + path.u8string();
					return false;
				}
			}
			else if (key == "graphics")
			{
				if (!GraphicsApiFromString(value, parsed.graphics_api))
				{
					error = "Unknown graphics API '" + value + "' in " + path.u8string();
					return false;
				}
			}
			else if (key == "engine_root")
				parsed.engine_root = std::filesystem::u8path(value);
			else if (key == "engine_version")
				parsed.engine_version = value;
			else if (key == "template_version")
				parsed.template_version = value;
			else if (key == "created")
				parsed.created = value;
			else if (key == "dependency")
				parsed.dependencies.push_back(ParseDependency(value));
			// Unknown keys are ignored so newer editors can add metadata.
		}

		if (!header_seen)
		{
			error = "The project file is empty: " + path.u8string();
			return false;
		}
		if (parsed.name.empty())
		{
			error = "The project file has no name: " + path.u8string();
			return false;
		}
		if (parsed.id.empty())
		{
			error = "The project file has no identifier: " + path.u8string();
			return false;
		}
		if (parsed.name_space.empty())
			parsed.name_space = DeriveNamespace(parsed.name);

		metadata = std::move(parsed);
		error.clear();
		return true;
	}

	bool SaveProjectFile(const std::filesystem::path &path, const ProjectMetadata &metadata,
		std::string &error)
	{
		if (path.empty())
		{
			error = "No project file path was supplied";
			return false;
		}

		std::error_code filesystem_error;
		const std::filesystem::path parent = path.parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent, filesystem_error);
		if (filesystem_error)
		{
			error = "Failed to create the project directory: " + filesystem_error.message();
			return false;
		}

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			error = "Failed to open the project file for writing: " + path.u8string();
			return false;
		}

		output << "# BlackLight project metadata\n";
		output << FileHeaderPrefix << ' ' << ProjectFormatVersion << '\n';
		output << "name = " << SanitizeValue(metadata.name) << '\n';
		output << "id = " << SanitizeValue(metadata.id) << '\n';
		output << "kind = " << ToString(metadata.kind) << '\n';
		output << "namespace = " << SanitizeValue(metadata.name_space) << '\n';
		output << "graphics = " << ToString(metadata.graphics_api) << '\n';
		output << "engine_root = " << SanitizeValue(metadata.engine_root.u8string()) << '\n';
		output << "engine_version = " << SanitizeValue(metadata.engine_version) << '\n';
		output << "template_version = " << SanitizeValue(metadata.template_version) << '\n';
		output << "created = " << SanitizeValue(metadata.created) << '\n';
		for (const ProjectDependency &dependency : metadata.dependencies)
		{
			output << "dependency = " << SanitizeValue(dependency.name) << " | "
				<< SanitizeValue(dependency.version) << " | "
				<< SanitizeValue(dependency.path) << '\n';
		}

		output.flush();
		if (!output.good())
		{
			error = "Failed to write the project file: " + path.u8string();
			return false;
		}
		error.clear();
		return true;
	}

} // namespace BlackLight::Editor
