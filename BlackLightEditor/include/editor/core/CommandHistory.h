#pragma once

#include "editor/core/Commands.h"
#include "editor/core/EditorEvent.h"
#include "editor/core/EditorState.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace BlackLight::Editor
{
	class UiDocumentSessions;

	/// Executes commands and maintains the undo and redo stacks.
	class CommandHistory final
	{
	public:
		/// Binds the history to the state it mutates and the bus receives notifications.
		CommandHistory(EditorState &state, EventBus &events, std::size_t limit = 128);

		/// Supplies the UI document sessions available to commands.
		void SetUiDocuments(UiDocumentSessions *ui_docs) noexcept;

		/// Executes a command and records it when undoable.
		/// Returns false when the command is missing, cannot run, or fails.
		bool Execute(std::unique_ptr<Command> command);
		/// Reverts the most recent undoable command.
		bool Undo();
		/// Re-applies the most recently undone command.
		bool Redo();

		/// Returns whether an undo step is available.
		bool CanUndo() const noexcept;
		/// Returns whether a redo step is available.
		bool CanRedo() const noexcept;
		/// Returns the name of the next undo command, or an empty string.
		std::string GetUndoName() const;
		/// Returns the name of the next redo command, or an empty string.
		std::string GetRedoName() const;
		/// Returns undo command names from oldest to newest.
		std::vector<std::string> GetUndoEntries() const;
		/// Returns redo command names from most recently undone to earliest.
		std::vector<std::string> GetRedoEntries() const;

		/// Returns the undo stack size.
		std::size_t GetUndoCount() const noexcept;
		/// Returns the redo stack size.
		std::size_t GetRedoCount() const noexcept;

		/// Clears both stacks and marks the document as unmodified.
		void Clear();

		/// Marks the current undo position as the saved state.
		void MarkSaved();
		/// Returns whether the current undo position differs from the saved state.
		bool IsModified() const noexcept;

		/// Sets the maximum undo depth; zero keeps an unlimited stack.
		void SetLimit(std::size_t limit);
		/// Returns the maximum undo depth.
		std::size_t GetLimit() const noexcept;

	private:
		CommandContext Context() noexcept;
		void SyncModifiedState();
		void EmitHistoryChanged();

		EditorState &state_;
		EventBus &events_;
		UiDocumentSessions *ui_docs_ = nullptr;
		std::vector<std::unique_ptr<Command>> undo_;
		std::vector<std::unique_ptr<Command>> redo_;
		std::size_t saved_undo_count_ = 0;
		std::size_t limit_ = 128;
	};

} // namespace BlackLight::Editor
