#pragma once

#include "editor/uidoc/BuiDocumentModel.h"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes one open `.bui` document and its independent undo state.
	struct UiDocumentTab
	{
		std::string id;
		std::filesystem::path path;
		std::string title;
		BuiDocumentModel model;
		bool modified = false;
		std::string selection_path;
		std::size_t saved_history_depth = 0;
		std::size_t undo_position = 0;

		struct UndoEntry
		{
			std::string description;
			std::string before;
			std::string after;
			std::string before_selection;
			std::string after_selection;
		};
		std::vector<UndoEntry> history;

		/// Returns whether an undo entry is available.
		bool CanUndo() const noexcept { return undo_position > 0; }
		/// Returns whether a redo entry is available.
		bool CanRedo() const noexcept { return undo_position < history.size(); }
	};

	/// Owns the open UI documents, their selections, and per-tab undo stacks.
	///
	/// This layer is deliberately independent of RmlUi so opening, editing,
	/// saving, and undo behavior can be tested by the editor core suite.
	class UiDocumentSessions final
	{
	public:
		/// Receives a model edit after it has been validated and applied.
		using EditFunction = std::function<bool(BuiDocumentModel &, BuiEditResult &)>;

		UiDocumentSessions() = default;

		/// Opens an existing `.bui` file, activating its tab when already open.
		UiDocumentTab *Open(const std::filesystem::path &path, std::string &error);
		/// Creates an unsaved document with a suggested destination path.
		UiDocumentTab *CreateNew(const std::filesystem::path &suggested,
			std::string &error);
		/// Creates a valid empty document at the suggested path.
		static bool CreateEmpty(std::filesystem::path &suggested, std::string &error);

		/// Closes a tab when it is unused or `discard` is true.
		bool Close(std::string_view tab_id, bool discard, std::string &error);
		/// Returns the active tab, or null when no document is open.
		UiDocumentTab *GetActive() noexcept;
		/// Returns the active tab, or null when no document is open.
		const UiDocumentTab *GetActive() const noexcept;
		/// Returns every open tab in display order.
		const std::deque<UiDocumentTab> &GetTabs() const noexcept;
		/// Activates a tab by identifier.
		bool Activate(std::string_view tab_id);
		/// Returns a tab by identifier, or null.
		UiDocumentTab *Find(std::string_view tab_id) noexcept;
		/// Returns a tab by identifier, or null.
		const UiDocumentTab *Find(std::string_view tab_id) const noexcept;

		/// Writes a tab to a path and updates its title and save point.
		bool Save(UiDocumentTab &tab, const std::filesystem::path &path,
			std::string &error);
		/// Saves the active tab, optionally asking callers for a new path.
		bool SaveActive(bool save_as, const std::filesystem::path &chosen_path,
			std::string &error);

		/// Applies one edit through a callback and records a complete undo entry.
		bool ApplyEdit(UiDocumentTab &tab, std::string_view description,
			const EditFunction &edit);
		/// Applies an edit without starting a new history entry.
		bool ApplyTransientEdit(UiDocumentTab &tab, const EditFunction &edit);
		/// Undoes one edit in the active tab.
		bool Undo(std::string &description);
		/// Redoes one edit in the active tab.
		bool Redo(std::string &description);

		/// Sets the active tab's selection path.
		bool SetSelection(std::string_view path);
		/// Clears the active tab's selection.
		void ClearSelection();
		/// Updates the displayed selection and returns its node.
		const BuiNode *GetSelectedNode() const;
		/// Returns the selected node in a specific tab.
		const BuiNode *GetSelectedNode(const UiDocumentTab &tab) const;

		/// Returns whether any open tab has unsaved changes.
		bool HasModifiedTabs() const noexcept;
		/// Returns whether any UI document is open.
		bool HasDocuments() const noexcept;
		/// Closes every tab, returning false when one has unsaved changes.
		bool CloseAll(bool discard, std::string &error);

		/// Marks one tab modified or clean using the current undo position.
		void UpdateModified(UiDocumentTab &tab) noexcept;

	private:
		/// Assigns a stable session identifier and title to a new tab.
		void PrepareTab(UiDocumentTab &tab, const std::filesystem::path &suggested);
		/// Restores a tab to one snapshot without adding history.
		bool RestoreSnapshot(UiDocumentTab &tab, const std::string &text,
			std::string &selection, std::string &error);
		/// Removes a tab from the collection by identifier.
		void Erase(std::string_view tab_id);

		std::deque<UiDocumentTab> tabs_;
		std::string active_id_;
		std::size_t next_id_ = 1;
	};

} // namespace BlackLight::Editor
