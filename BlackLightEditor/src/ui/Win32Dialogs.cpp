#include "editor/ui/Win32Dialogs.h"

#include <algorithm>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>

namespace BlackLight::Editor
{

	namespace
	{

		/// Converts UTF-8 text to UTF-16 for the wide Win32 dialogs.
		std::wstring ToWide(std::string_view text)
		{
			if (text.empty())
				return {};
			const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
				static_cast<int>(text.size()), nullptr, 0);
			if (length <= 0)
				return {};
			std::wstring result(static_cast<std::size_t>(length), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
				result.data(), length);
			return result;
		}

		/// Copies a text value into a fixed Win32 dialog buffer.
		void CopyToBuffer(const std::wstring &text, wchar_t *buffer, std::size_t capacity)
		{
			const std::size_t count = (std::min)(text.size(), capacity - 1);
			std::copy_n(text.data(), count, buffer);
			buffer[count] = L'\0';
		}

		/// Shows an open-file dialog with the supplied filter.
		std::filesystem::path ShowOpenFileDialog(const std::filesystem::path &initial_directory,
			const wchar_t *filter, const wchar_t *default_extension)
		{
			std::vector<wchar_t> file_name(32768, L'\0');
			const std::wstring initial = initial_directory.wstring();

			OPENFILENAMEW dialog{};
			dialog.lStructSize = sizeof(dialog);
			dialog.hwndOwner = GetActiveWindow();
			dialog.lpstrFilter = filter;
			dialog.lpstrFile = file_name.data();
			dialog.nMaxFile = static_cast<DWORD>(file_name.size());
			dialog.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
			dialog.lpstrDefExt = default_extension;
			dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			if (!GetOpenFileNameW(&dialog))
				return {};
			return std::filesystem::path(file_name.data());
		}

		/// Selects the initial folder when the shell browse dialog opens.
		int CALLBACK BrowseCallback(HWND window, UINT message, LPARAM /*parameter*/,
			LPARAM data)
		{
			if (message == BFFM_INITIALIZED && data != 0)
				SendMessageW(window, BFFM_SETSELECTIONW, TRUE, data);
			return 0;
		}

	} // namespace

	std::filesystem::path ShowOpenWorkspaceDialog(
		const std::filesystem::path &initial_directory)
	{
		return ShowOpenFileDialog(initial_directory,
			L"BlackLight Workspace (*.blw)\0*.blw\0All Files (*.*)\0*.*\0", L"blw");
	}

	std::filesystem::path ShowSaveWorkspaceDialog(
		const std::filesystem::path &suggested_path)
	{
		std::vector<wchar_t> file_name(32768, L'\0');
		std::wstring suggestion = suggested_path.filename().wstring();
		if (suggestion.empty())
			suggestion = L"Untitled.blw";
		if (suggested_path.extension().empty())
			suggestion += L".blw";
		CopyToBuffer(suggestion, file_name.data(), file_name.size());
		const std::wstring initial = suggested_path.parent_path().wstring();

		OPENFILENAMEW dialog{};
		dialog.lStructSize = sizeof(dialog);
		dialog.hwndOwner = GetActiveWindow();
		dialog.lpstrFilter = L"BlackLight Workspace (*.blw)\0*.blw\0All Files (*.*)\0*.*\0";
		dialog.lpstrFile = file_name.data();
		dialog.nMaxFile = static_cast<DWORD>(file_name.size());
		dialog.lpstrInitialDir = initial.empty() ? nullptr : initial.c_str();
		dialog.lpstrDefExt = L"blw";
		dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetSaveFileNameW(&dialog))
			return {};
		return std::filesystem::path(file_name.data());
	}

	std::filesystem::path ShowOpenProjectDialog(
		const std::filesystem::path &initial_directory)
	{
		return ShowOpenFileDialog(initial_directory,
			L"BlackLight Project (*.blacklight)\0*.blacklight\0All Files (*.*)\0*.*\0",
			L"blacklight");
	}

	std::filesystem::path ShowBrowseFolderDialog(
		const std::filesystem::path &initial_directory, std::wstring_view title)
	{
		const HRESULT com_result =
			CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		const bool uninitialize_com = SUCCEEDED(com_result);

		std::wstring initial = initial_directory.wstring();
		std::wstring title_text(title);

		BROWSEINFOW info{};
		info.hwndOwner = GetActiveWindow();
		info.lpszTitle = title_text.c_str();
		info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
		info.lpfn = BrowseCallback;
		info.lParam = initial.empty()
			? 0
			: reinterpret_cast<LPARAM>(initial.c_str());

		PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&info);
		std::filesystem::path result;
		if (item)
		{
			wchar_t path[MAX_PATH]{};
			if (SHGetPathFromIDListW(item, path))
				result = std::filesystem::path(path);
			CoTaskMemFree(item);
		}

		if (uninitialize_com)
			CoUninitialize();
		return result;
	}

	bool ConfirmDiscardChanges(std::string_view document_name)
	{
		const std::wstring message = L"'" + ToWide(document_name) +
			L"' has unsaved changes.\n\nDiscard the changes?";
		return MessageBoxW(nullptr, message.c_str(), L"BlackLightEditor",
			MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES;
	}

} // namespace BlackLight::Editor
