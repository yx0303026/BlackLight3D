#include "editor/core/CommandHistory.h"

#include <utility>

namespace BlackLight::Editor
{

	CommandHistory::CommandHistory(EditorState &state, EventBus &events, std::size_t limit)
		: state_(state), events_(events), limit_(limit)
	{
	}

	void CommandHistory::SetUiDocuments(UiDocumentSessions *ui_docs) noexcept
	{
		ui_docs_ = ui_docs;
	}

	CommandContext CommandHistory::Context() noexcept
	{
		return {state_, events_, ui_docs_};
	}

	bool CommandHistory::Execute(std::unique_ptr<Command> command)
	{
		if (!command)
			return false;

		CommandContext context = Context();
		if (!command->CanExecute(context))
			return false;

		const bool undoable = command->IsUndoable();
		const bool clears_history = command->ClearsHistory();
		const bool sets_save_point = command->SetsSavePoint();
		const std::string name = command->GetName();

		if (!command->Execute(context))
			return false;

		if (clears_history)
		{
			undo_.clear();
			redo_.clear();
			saved_undo_count_ = 0;
		}
		else if (undoable)
		{
			undo_.push_back(std::move(command));
			if (limit_ > 0 && undo_.size() > limit_)
			{
				const std::size_t dropped = undo_.size() - limit_;
				undo_.erase(undo_.begin(), undo_.begin() + static_cast<std::ptrdiff_t>(dropped));
				saved_undo_count_ = saved_undo_count_ > dropped ? saved_undo_count_ - dropped : 0;
			}
			redo_.clear();
		}

		if (sets_save_point)
			saved_undo_count_ = undo_.size();

		SyncModifiedState();
		events_.Emit({EventType::CommandExecuted, name, name});
		EmitHistoryChanged();
		return true;
	}

	bool CommandHistory::Undo()
	{
		if (undo_.empty())
			return false;

		std::unique_ptr<Command> command = std::move(undo_.back());
		undo_.pop_back();
		const std::string name = command->GetName();
		CommandContext context = Context();
		if (!command->Undo(context))
		{
			undo_.push_back(std::move(command));
			return false;
		}

		redo_.push_back(std::move(command));
		SyncModifiedState();
		state_.SetStatus("Undo: " + name);
		events_.Emit({EventType::CommandUndone, name, {}});
		EmitHistoryChanged();
		return true;
	}

	bool CommandHistory::Redo()
	{
		if (redo_.empty())
			return false;

		std::unique_ptr<Command> command = std::move(redo_.back());
		redo_.pop_back();
		const std::string name = command->GetName();
		CommandContext context = Context();
		if (!command->Execute(context))
		{
			redo_.push_back(std::move(command));
			return false;
		}

		undo_.push_back(std::move(command));
		SyncModifiedState();
		state_.SetStatus("Redo: " + name);
		events_.Emit({EventType::CommandRedone, name, {}});
		EmitHistoryChanged();
		return true;
	}

	bool CommandHistory::CanUndo() const noexcept { return !undo_.empty(); }

	bool CommandHistory::CanRedo() const noexcept { return !redo_.empty(); }

	std::string CommandHistory::GetUndoName() const
	{
		return undo_.empty() ? std::string() : std::string(undo_.back()->GetName());
	}

	std::string CommandHistory::GetRedoName() const
	{
		return redo_.empty() ? std::string() : std::string(redo_.back()->GetName());
	}

	std::vector<std::string> CommandHistory::GetUndoEntries() const
	{
		std::vector<std::string> entries;
		entries.reserve(undo_.size());
		for (const auto &command : undo_)
			entries.emplace_back(command->GetName());
		return entries;
	}

	std::vector<std::string> CommandHistory::GetRedoEntries() const
	{
		std::vector<std::string> entries;
		entries.reserve(redo_.size());
		for (auto it = redo_.rbegin(); it != redo_.rend(); ++it)
			entries.emplace_back((*it)->GetName());
		return entries;
	}

	std::size_t CommandHistory::GetUndoCount() const noexcept { return undo_.size(); }

	std::size_t CommandHistory::GetRedoCount() const noexcept { return redo_.size(); }

	void CommandHistory::Clear()
	{
		undo_.clear();
		redo_.clear();
		saved_undo_count_ = 0;
		SyncModifiedState();
		EmitHistoryChanged();
	}

	void CommandHistory::MarkSaved()
	{
		saved_undo_count_ = undo_.size();
		SyncModifiedState();
		EmitHistoryChanged();
	}

	bool CommandHistory::IsModified() const noexcept { return undo_.size() != saved_undo_count_; }

	void CommandHistory::SetLimit(std::size_t limit)
	{
		limit_ = limit;
		if (limit_ > 0 && undo_.size() > limit_)
		{
			const std::size_t dropped = undo_.size() - limit_;
			undo_.erase(undo_.begin(), undo_.begin() + static_cast<std::ptrdiff_t>(dropped));
			saved_undo_count_ = saved_undo_count_ > dropped ? saved_undo_count_ - dropped : 0;
		}
		SyncModifiedState();
		EmitHistoryChanged();
	}

	std::size_t CommandHistory::GetLimit() const noexcept { return limit_; }

	void CommandHistory::SyncModifiedState()
	{
		state_.MarkModified(IsModified());
	}

	void CommandHistory::EmitHistoryChanged()
	{
		events_.Emit({EventType::CommandHistoryChanged, GetUndoName(), GetRedoName()});
	}

} // namespace BlackLight::Editor
