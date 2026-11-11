#include "editor/core/EditorConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
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

		/// Lowercases an ASCII token for case-insensitive keyword comparison.
		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			return text;
		}

		/// Parses the accepted boolean spellings.
		bool ParseBool(std::string_view text, bool fallback)
		{
			const std::string lowered = ToLower(Trim(text));
			if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on")
				return true;
			if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off")
				return false;
			return fallback;
		}

	} // namespace

	EditorConfig::EditorConfig(std::filesystem::path path) : path_(std::move(path)) {}

	bool EditorConfig::Load() { return Load(path_); }

	bool EditorConfig::Load(const std::filesystem::path &path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
		{
			path_ = path;
			return false;
		}

		Sections parsed;
		std::string current_section;
		std::string line;
		bool first_line = true;
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
			if (trimmed.empty() || trimmed.front() == ';' || trimmed.front() == '#')
				continue;

			if (trimmed.front() == '[' && trimmed.back() == ']')
			{
				current_section = ToLower(Trim(std::string_view(trimmed).substr(1, trimmed.size() - 2)));
				parsed[current_section];
				continue;
			}

			const std::size_t separator = trimmed.find('=');
			if (separator == std::string::npos)
				continue;

			const std::string key = ToLower(Trim(std::string_view(trimmed).substr(0, separator)));
			if (key.empty())
				continue;
			const std::string value = Trim(std::string_view(trimmed).substr(separator + 1));
			parsed[current_section][key] = value;
		}

		sections_ = std::move(parsed);
		path_ = path;
		return true;
	}

	bool EditorConfig::Save() const { return Save(path_); }

	bool EditorConfig::Save(const std::filesystem::path &path) const
	{
		if (path.empty())
			return false;

		std::error_code error;
		const auto parent = path.parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent, error);
		if (error)
			return false;

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output)
			return false;

		for (const auto &[section, keys] : sections_)
		{
			output << '[' << section << "]\n";
			for (const auto &[key, value] : keys)
				output << key << '=' << value << '\n';
			output << '\n';
		}
		output.flush();
		return output.good();
	}

	void EditorConfig::Clear()
	{
		sections_.clear();
	}

	const std::filesystem::path &EditorConfig::GetPath() const noexcept { return path_; }

	void EditorConfig::SetPath(std::filesystem::path path) { path_ = std::move(path); }

	EditorConfig::Keys *EditorConfig::FindSection(std::string_view section) noexcept
	{
		const auto it = sections_.find(ToLower(std::string(section)));
		return it == sections_.end() ? nullptr : &it->second;
	}

	const EditorConfig::Keys *EditorConfig::FindSection(std::string_view section) const noexcept
	{
		const auto it = sections_.find(ToLower(std::string(section)));
		return it == sections_.end() ? nullptr : &it->second;
	}

	std::string EditorConfig::Get(std::string_view section, std::string_view key,
		std::string_view fallback) const
	{
		const Keys *keys = FindSection(section);
		if (!keys)
			return std::string(fallback);
		const auto it = keys->find(ToLower(std::string(key)));
		return it == keys->end() ? std::string(fallback) : it->second;
	}

	int EditorConfig::GetInt(std::string_view section, std::string_view key, int fallback) const
	{
		const std::string value = Get(section, key);
		if (value.empty())
			return fallback;
		try
		{
			std::size_t consumed = 0;
			const int parsed = std::stoi(value, &consumed);
			return consumed == value.size() ? parsed : fallback;
		}
		catch (...)
		{
			return fallback;
		}
	}

	bool EditorConfig::GetBool(std::string_view section, std::string_view key, bool fallback) const
	{
		const std::string value = Get(section, key);
		if (value.empty())
			return fallback;
		return ParseBool(value, fallback);
	}

	void EditorConfig::Set(std::string_view section, std::string_view key, std::string value)
	{
		sections_[ToLower(std::string(section))][ToLower(std::string(key))] = std::move(value);
	}

	void EditorConfig::SetInt(std::string_view section, std::string_view key, int value)
	{
		Set(section, key, std::to_string(value));
	}

	void EditorConfig::SetBool(std::string_view section, std::string_view key, bool value)
	{
		Set(section, key, value ? "1" : "0");
	}

	bool EditorConfig::Remove(std::string_view section, std::string_view key)
	{
		Keys *keys = FindSection(section);
		if (!keys)
			return false;
		return keys->erase(ToLower(std::string(key))) != 0;
	}

	std::vector<std::string> EditorConfig::GetRecentFiles() const
	{
		std::vector<std::string> result;
		const int count = GetInt("recent", "count", 0);
		if (count <= 0)
			return result;
		result.reserve(static_cast<std::size_t>(count));
		for (int index = 0; index < count; ++index)
		{
			const std::string value = Get("recent", "file" + std::to_string(index));
			if (!value.empty())
				result.push_back(value);
		}
		return result;
	}

	void EditorConfig::AddRecentFile(std::string path, std::size_t limit)
	{
		if (path.empty())
			return;

		std::vector<std::string> files = GetRecentFiles();
		files.erase(std::remove(files.begin(), files.end(), path), files.end());
		files.insert(files.begin(), std::move(path));
		if (limit > 0 && files.size() > limit)
			files.resize(limit);

		SetInt("recent", "count", static_cast<int>(files.size()));
		for (std::size_t index = 0; index < files.size(); ++index)
			Set("recent", "file" + std::to_string(index), files[index]);
		for (std::size_t index = files.size(); index < 64; ++index)
		{
			if (!Remove("recent", "file" + std::to_string(index)))
				break;
		}
	}

	void EditorConfig::ClearRecentFiles()
	{
		for (std::size_t index = 0; index < 64; ++index)
		{
			if (!Remove("recent", "file" + std::to_string(index)))
				break;
		}
		SetInt("recent", "count", 0);
	}

} // namespace BlackLight::Editor
