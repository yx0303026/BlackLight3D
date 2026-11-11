#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Stores editor settings in a small, human-readable INI document.
	///
	/// The format accepts `[section]` headers, `key = value` pairs, blank lines,
	/// and lines beginning with `;` or `#` as comments. Values are UTF-8 text and
	/// may contain `=`; only the first separator is significant.
	class EditorConfig final
	{
	public:
		/// Creates an empty configuration bound to the supplied file path.
		explicit EditorConfig(std::filesystem::path path = {});

		/// Loads the configured path, replacing all current values.
		/// Returns false when the file is missing or cannot be parsed.
		bool Load();
		/// Loads a specific file and remembers it as the active path.
		bool Load(const std::filesystem::path &path);
		/// Saves to the configured path.
		bool Save() const;
		/// Saves to a specific file without changing the active path.
		bool Save(const std::filesystem::path &path) const;
		/// Removes every stored value without touching the file system.
		void Clear();

		/// Returns the file used by path-less Load and Save calls.
		const std::filesystem::path &GetPath() const noexcept;
		/// Sets the file used by path-less Load and Save calls.
		void SetPath(std::filesystem::path path);

		/// Returns a stored string or the fallback when the key is absent.
		std::string Get(std::string_view section, std::string_view key,
			std::string_view fallback = {}) const;
		/// Returns a stored integer or the fallback when the value is not numeric.
		int GetInt(std::string_view section, std::string_view key, int fallback) const;
		/// Returns a stored boolean or the fallback when the value is not recognized.
		bool GetBool(std::string_view section, std::string_view key, bool fallback) const;

		/// Stores a string value.
		void Set(std::string_view section, std::string_view key, std::string value);
		/// Stores an integer value.
		void SetInt(std::string_view section, std::string_view key, int value);
		/// Stores a boolean as `1` or `0`.
		void SetBool(std::string_view section, std::string_view key, bool value);
		/// Removes one key and returns whether it existed.
		bool Remove(std::string_view section, std::string_view key);

		/// Returns the most recently used workspace paths, newest first.
		std::vector<std::string> GetRecentFiles() const;
		/// Inserts a workspace path at the front, removing duplicates and trimming to the limit.
		void AddRecentFile(std::string path, std::size_t limit = 10);
		/// Removes the whole recent-workspace list.
		void ClearRecentFiles();

	private:
		using Keys = std::map<std::string, std::string, std::less<>>;
		using Sections = std::map<std::string, Keys, std::less<>>;

		Keys *FindSection(std::string_view section) noexcept;
		const Keys *FindSection(std::string_view section) const noexcept;

		Sections sections_;
		std::filesystem::path path_;
	};

} // namespace BlackLight::Editor
