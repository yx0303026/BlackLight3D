#pragma once

#include <filesystem>
#include <string_view>

namespace BlackLight::Editor
{

	/// Shows the native open dialog for `.blw` workspace files.
	/// Returns an empty path when the user cancels.
	std::filesystem::path ShowOpenWorkspaceDialog(
		const std::filesystem::path &initial_directory);

	/// Shows the native save dialog for `.blw` workspace files.
	/// Returns an empty path when the user cancels.
	std::filesystem::path ShowSaveWorkspaceDialog(
		const std::filesystem::path &suggested_path);

	/// Shows the native open dialog for `.blacklight` project files.
	/// Returns an empty path when the user cancels.
	std::filesystem::path ShowOpenProjectDialog(
		const std::filesystem::path &initial_directory);

	/// Shows the native folder picker used by the project wizard.
	/// Returns an empty path when the user cancels.
	std::filesystem::path ShowBrowseFolderDialog(
		const std::filesystem::path &initial_directory,
		std::wstring_view title = L"Select a folder");

	/// Asks whether unsaved changes should be discarded.
	bool ConfirmDiscardChanges(std::string_view document_name);

} // namespace BlackLight::Editor
