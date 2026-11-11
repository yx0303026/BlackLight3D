#pragma once

#include "editor/project/ProjectTypes.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Maps `${VARIABLE}` placeholders to project-specific text.
	///
	/// `$${` produces a literal `${` so templates can document the syntax.
	class TemplateContext final
	{
	public:
		/// Stores or replaces one variable.
		void Set(std::string key, std::string value);
		/// Returns whether a variable is defined.
		bool Contains(std::string_view key) const;
		/// Returns a variable value, or null when it is undefined.
		const std::string *Find(std::string_view key) const;
		/// Returns every variable in stable order.
		const std::map<std::string, std::string, std::less<>> &GetVariables() const noexcept;

		/// Replaces every `${VARIABLE}` placeholder.
		/// Returns an empty string and fills `error` for an unknown or unterminated variable.
		std::string Render(std::string_view text, std::string *error = nullptr) const;

	private:
		std::map<std::string, std::string, std::less<>> variables_;
	};

	/// Controls whether a generated file is refreshed when a project is updated.
	enum class TemplateFilePolicy : std::uint8_t
	{
		/// Written only when the project is created; user edits are preserved.
		CreateOnly,
		/// Rewritten by project updates because the editor owns the file.
		Regenerate
	};

	/// Describes one file emitted by a project template.
	struct TemplateFile
	{
		/// Path relative to the project root; may contain variables.
		std::string path;
		/// File content; may contain variables.
		std::string content;
		TemplateFilePolicy policy = TemplateFilePolicy::CreateOnly;
	};

	/// Describes one versioned project scaffold.
	struct ProjectTemplate
	{
		ProjectKind kind = ProjectKind::WindowsApp;
		std::string version;
		std::string description;
		std::vector<std::string> directories;
		std::vector<TemplateFile> files;
	};

} // namespace BlackLight::Editor
