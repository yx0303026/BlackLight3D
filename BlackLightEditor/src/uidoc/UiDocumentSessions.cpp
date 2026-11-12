#include "editor/uidoc/UiDocumentSessions.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		/// Reads a complete UTF-8 file without newline translation.
		bool ReadTextFile(const std::filesystem::path &path, std::string &text,
			std::string &error)
		{
			std::ifstream input(path, std::ios::binary);
			if (!input)
			{
				error = "Failed to open " + path.u8string();
				return false;
			}
			std::ostringstream buffer;
			buffer << input.rdbuf();
			if (!input && !input.eof())
			{
				error = "Failed to read " + path.u8string();
				return false;
			}
			text = buffer.str();
			return true;
		}

		/// Writes UTF-8 bytes and creates the parent directory when needed.
		bool WriteTextFile(const std::filesystem::path &path, std::string_view text,
			std::string &error)
		{
			std::error_code filesystem_error;
			if (!path.parent_path().empty())
			{
				std::filesystem::create_directories(path.parent_path(), filesystem_error);
				if (filesystem_error)
				{
					error = "Failed to create " + path.parent_path().u8string() + ": " +
						filesystem_error.message();
					return false;
				}
			}

			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			if (!output)
			{
				error = "Failed to open " + path.u8string() + " for writing";
				return false;
			}
			output.write(text.data(), static_cast<std::streamsize>(text.size()));
			if (!output)
			{
				error = "Failed to write " + path.u8string();
				return false;
			}
			return true;
		}

		/// Returns a canonical path when possible and an absolute path otherwise.
		std::filesystem::path NormalizePath(const std::filesystem::path &path)
		{
			std::error_code error;
			std::filesystem::path result = std::filesystem::weakly_canonical(path, error);
			if (error)
				result = std::filesystem::absolute(path, error);
			return result;
		}

		/// Generates the file contents used by a newly created UI document.
		const char *NewDocumentText()
		{
			return R"(<?xml version="1.0" encoding="UTF-8"?>
<bui>
  <styles>
    body {
      display: block;
      width: 100%;
      height: 100%;
      margin: 0;
      padding: 0;
      font-family: Arial;
      font-size: 16dp;
      color: #ffffff;
      background-color: #202124;
    }
  </styles>
  <layout>
    <div class="main-screen">
    </div>
  </layout>
</bui>
)";
		}

	} // namespace

	bool UiDocumentSessions::CreateEmpty(std::filesystem::path &suggested,
		std::string &error)
	{
		if (suggested.empty())
			suggested = "Untitled.bui";
		if (suggested.extension() != ".bui")
			suggested.replace_extension(".bui");
		BuiDocumentModel model;
		BuiParseError parse_error;
		if (!BuiDocumentModel::Parse(NewDocumentText(), model, parse_error))
		{
			error = parse_error.message;
			return false;
		}
		error.clear();
		return true;
	}

	UiDocumentTab *UiDocumentSessions::Open(const std::filesystem::path &path,
		std::string &error)
	{
		if (path.empty())
		{
			error = "No UI document path was supplied";
			return nullptr;
		}
		const std::filesystem::path normalized = NormalizePath(path);
		for (UiDocumentTab &tab : tabs_)
		{
			if (!tab.path.empty() && NormalizePath(tab.path) == normalized)
			{
				active_id_ = tab.id;
				error.clear();
				return &tab;
			}
		}

		std::string text;
		if (!ReadTextFile(normalized, text, error))
			return nullptr;
		BuiDocumentModel model;
		BuiParseError parse_error;
		if (!BuiDocumentModel::Parse(std::move(text), model, parse_error))
		{
			error = "Failed to parse " + normalized.filename().u8string() + ": " +
				parse_error.message;
			if (parse_error.line > 0)
				error += " (line " + std::to_string(parse_error.line) + ")";
			return nullptr;
		}

		UiDocumentTab tab;
		tab.id = "ui-doc-" + std::to_string(next_id_++);
		tab.path = normalized;
		tab.title = normalized.filename().u8string();
		// Existing files start clean; only a fresh document counts as modified
		// until it has been written once.
		tab.saved_history_depth = 0;
		tab.undo_position = 0;
		tab.modified = false;
		tabs_.push_back(std::move(tab));
		active_id_ = tabs_.back().id;
		tabs_.back().model = std::move(model);
		error = parse_error.message;
		return &tabs_.back();
	}

	UiDocumentTab *UiDocumentSessions::CreateNew(
		const std::filesystem::path &suggested, std::string &error)
	{
		BuiDocumentModel model;
		BuiParseError parse_error;
		if (!BuiDocumentModel::Parse(NewDocumentText(), model, parse_error))
		{
			error = parse_error.message;
			return nullptr;
		}

		std::filesystem::path target = suggested;
		if (target.empty())
			target = "Untitled.bui";
		if (target.extension() != ".bui")
			target.replace_extension(".bui");

		std::filesystem::path unique = target;
		int suffix = 2;
		while (std::filesystem::exists(unique))
		{
			unique = target.parent_path() /
				(target.stem().u8string() + std::to_string(suffix++) + ".bui");
		}

		UiDocumentTab tab;
		PrepareTab(tab, unique);
		tab.model = std::move(model);
		tab.saved_history_depth = 0;
		tab.undo_position = 0;
		tab.modified = true;
		tabs_.push_back(std::move(tab));
		active_id_ = tabs_.back().id;
		error.clear();
		return &tabs_.back();
	}

	void UiDocumentSessions::PrepareTab(UiDocumentTab &tab,
		const std::filesystem::path &suggested)
	{
		tab.id = "ui-doc-" + std::to_string(next_id_++);
		tab.path = NormalizePath(suggested);
		tab.title = tab.path.filename().u8string();
		if (tab.title.empty())
			tab.title = "Untitled.bui";
		tab.modified = true;
		tab.undo_position = 0;
		tab.saved_history_depth = 0;
	}

	bool UiDocumentSessions::Close(std::string_view tab_id, bool discard,
		std::string &error)
	{
		const auto it = std::find_if(tabs_.begin(), tabs_.end(),
			[tab_id](const UiDocumentTab &tab) { return tab.id == tab_id; });
		if (it == tabs_.end())
		{
			error = "Unknown UI document";
			return false;
		}
		if (it->modified && !discard)
		{
			error = it->title + " has unsaved changes";
			return false;
		}
		Erase(tab_id);
		error.clear();
		return true;
	}

	void UiDocumentSessions::Erase(std::string_view tab_id)
	{
		const bool was_active = active_id_ == tab_id;
		tabs_.erase(std::remove_if(tabs_.begin(), tabs_.end(),
			[tab_id](const UiDocumentTab &tab) { return tab.id == tab_id; }),
			tabs_.end());
		if (was_active)
		{
			active_id_ = tabs_.empty() ? std::string() : tabs_.front().id;
		}
	}

	UiDocumentTab *UiDocumentSessions::GetActive() noexcept
	{
		return const_cast<UiDocumentTab *>(
			static_cast<const UiDocumentSessions *>(this)->GetActive());
	}

	const UiDocumentTab *UiDocumentSessions::GetActive() const noexcept
	{
		if (tabs_.empty())
			return nullptr;
		for (const UiDocumentTab &tab : tabs_)
		{
			if (tab.id == active_id_)
				return &tab;
		}
		return &tabs_.front();
	}

	const std::deque<UiDocumentTab> &UiDocumentSessions::GetTabs() const noexcept
	{
		return tabs_;
	}

	bool UiDocumentSessions::Activate(std::string_view tab_id)
	{
		for (const UiDocumentTab &tab : tabs_)
		{
			if (tab.id == tab_id)
			{
				active_id_ = tab.id;
				return true;
			}
		}
		return false;
	}

	UiDocumentTab *UiDocumentSessions::Find(std::string_view tab_id) noexcept
	{
		for (UiDocumentTab &tab : tabs_)
		{
			if (tab.id == tab_id)
				return &tab;
		}
		return nullptr;
	}

	const UiDocumentTab *UiDocumentSessions::Find(std::string_view tab_id) const noexcept
	{
		for (const UiDocumentTab &tab : tabs_)
		{
			if (tab.id == tab_id)
				return &tab;
		}
		return nullptr;
	}

	bool UiDocumentSessions::Save(UiDocumentTab &tab,
		const std::filesystem::path &path, std::string &error)
	{
		if (path.empty())
		{
			error = "No destination path was supplied";
			return false;
		}
		if (!WriteTextFile(path, tab.model.GetText(), error))
			return false;
		tab.path = NormalizePath(path);
		tab.title = tab.path.filename().u8string();
		tab.saved_history_depth = tab.undo_position;
		UpdateModified(tab);
		error.clear();
		return true;
	}

	bool UiDocumentSessions::SaveActive(bool save_as,
		const std::filesystem::path &chosen_path, std::string &error)
	{
		UiDocumentTab *tab = GetActive();
		if (!tab)
		{
			error = "There is no UI document to save";
			return false;
		}
		std::filesystem::path path = chosen_path;
		if (path.empty())
			path = tab->path;
		if (save_as || path.empty())
		{
			error = "A destination path is required";
			return false;
		}
		return Save(*tab, path, error);
	}

	bool UiDocumentSessions::ApplyEdit(UiDocumentTab &tab,
		std::string_view description, const EditFunction &edit)
	{
		if (!edit)
			return false;
		BuiEditResult result;
		if (!edit(tab.model, result))
			return false;
		if (result.new_text == result.old_text)
			return false;

		tab.history.erase(tab.history.begin() +
			static_cast<std::ptrdiff_t>(tab.undo_position), tab.history.end());
		tab.history.push_back({std::string(description), result.old_text,
			result.new_text, result.old_selection_path, result.new_selection_path});
		tab.undo_position = tab.history.size();
		if (!result.new_selection_path.empty())
			tab.selection_path = result.new_selection_path;
		UpdateModified(tab);
		return true;
	}

	bool UiDocumentSessions::ApplyTransientEdit(UiDocumentTab &tab,
		const EditFunction &edit)
	{
		if (!edit)
			return false;
		BuiEditResult result;
		if (!edit(tab.model, result))
			return false;
		if (result.new_text == result.old_text)
			return false;
		if (!result.new_selection_path.empty())
			tab.selection_path = result.new_selection_path;
		return true;
	}

	bool UiDocumentSessions::RestoreSnapshot(UiDocumentTab &tab,
		const std::string &text, std::string &selection, std::string &error)
	{
		BuiDocumentModel model;
		BuiParseError parse_error;
		if (!BuiDocumentModel::Parse(text, model, parse_error))
		{
			error = parse_error.message;
			return false;
		}
		tab.model = std::move(model);
		tab.selection_path = selection;
		if (tab.model.NodeAtPath(tab.selection_path) == InvalidBuiNode)
			tab.selection_path.clear();
		error.clear();
		return true;
	}

	bool UiDocumentSessions::Undo(std::string &description)
	{
		UiDocumentTab *tab = GetActive();
		if (!tab || !tab->CanUndo())
		{
			description.clear();
			return false;
		}
		const UiDocumentTab::UndoEntry &entry = tab->history[tab->undo_position - 1];
		std::string selection = entry.before_selection;
		std::string error;
		if (!RestoreSnapshot(*tab, entry.before, selection, error))
			return false;
		--tab->undo_position;
		tab->modified = tab->undo_position != tab->saved_history_depth;
		description = entry.description;
		return true;
	}

	bool UiDocumentSessions::Redo(std::string &description)
	{
		UiDocumentTab *tab = GetActive();
		if (!tab || !tab->CanRedo())
		{
			description.clear();
			return false;
		}
		const UiDocumentTab::UndoEntry &entry = tab->history[tab->undo_position];
		std::string selection = entry.after_selection;
		std::string error;
		if (!RestoreSnapshot(*tab, entry.after, selection, error))
			return false;
		++tab->undo_position;
		tab->modified = tab->undo_position != tab->saved_history_depth;
		description = entry.description;
		return true;
	}

	bool UiDocumentSessions::SetSelection(std::string_view path)
	{
		UiDocumentTab *tab = GetActive();
		if (!tab)
			return false;
		if (!path.empty() && tab->model.NodeAtPath(path) == InvalidBuiNode)
			return false;
		tab->selection_path = std::string(path);
		return true;
	}

	void UiDocumentSessions::ClearSelection()
	{
		if (UiDocumentTab *tab = GetActive())
			tab->selection_path.clear();
	}

	const BuiNode *UiDocumentSessions::GetSelectedNode() const
	{
		const UiDocumentTab *tab = GetActive();
		return tab ? GetSelectedNode(*tab) : nullptr;
	}

	const BuiNode *UiDocumentSessions::GetSelectedNode(const UiDocumentTab &tab) const
	{
		if (tab.selection_path.empty())
			return nullptr;
		const BuiNodeId id = tab.model.NodeAtPath(tab.selection_path);
		if (id == InvalidBuiNode || id == tab.model.GetRoot())
			return nullptr;
		return tab.model.Find(id);
	}

	bool UiDocumentSessions::HasModifiedTabs() const noexcept
	{
		return std::any_of(tabs_.begin(), tabs_.end(),
			[](const UiDocumentTab &tab) { return tab.modified; });
	}

	bool UiDocumentSessions::HasDocuments() const noexcept { return !tabs_.empty(); }

	bool UiDocumentSessions::CloseAll(bool discard, std::string &error)
	{
		if (!discard)
		{
			for (const UiDocumentTab &tab : tabs_)
			{
				if (tab.modified)
				{
					error = tab.title + " has unsaved changes";
					return false;
				}
			}
		}
		tabs_.clear();
		active_id_.clear();
		error.clear();
		return true;
	}

	void UiDocumentSessions::UpdateModified(UiDocumentTab &tab) noexcept
	{
		tab.modified = tab.undo_position != tab.saved_history_depth;
	}

} // namespace BlackLight::Editor
