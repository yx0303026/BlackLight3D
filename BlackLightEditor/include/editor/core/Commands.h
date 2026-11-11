#pragma once

#include "editor/core/EditorState.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace BlackLight::Editor
{

	/// Supplies the editor services available while a command executes.
	struct CommandContext
	{
		EditorState &state;
		EventBus &events;
	};

	/// Implements a reversible editor operation.
	class Command
	{
	public:
		virtual ~Command() = default;

		/// Returns the command name shown in menus and history panels.
		virtual const char *GetName() const noexcept = 0;
		/// Returns whether the command can run in the current editor state.
		virtual bool CanExecute(const CommandContext &context) const;
		/// Applies the command; returning false leaves the command unrecorded.
		virtual bool Execute(CommandContext &context) = 0;
		/// Reverts the command; returning false restores it to the undo stack.
		virtual bool Undo(CommandContext &context);
		/// Returns whether the command can be reverted.
		virtual bool IsUndoable() const noexcept;
		/// Returns whether executing the command invalidates the undo and redo stacks.
		virtual bool ClearsHistory() const noexcept;
		/// Returns whether executing the command establishes a clean save point.
		virtual bool SetsSavePoint() const noexcept;
	};

	/// Replaces the current document with an untitled workspace.
	class NewWorkspaceCommand final : public Command
	{
	public:
		explicit NewWorkspaceCommand(std::string name = "Untitled Workspace");
		const char *GetName() const noexcept override;
		bool Execute(CommandContext &context) override;
		bool ClearsHistory() const noexcept override;

	private:
		std::string name_;
	};

	/// Loads a workspace file and replaces the current document.
	class OpenWorkspaceCommand final : public Command
	{
	public:
		explicit OpenWorkspaceCommand(std::filesystem::path path);
		const char *GetName() const noexcept override;
		bool CanExecute(const CommandContext &context) const override;
		bool Execute(CommandContext &context) override;
		bool ClearsHistory() const noexcept override;

	private:
		std::filesystem::path path_;
	};

	/// Writes the current document to a workspace file.
	class SaveWorkspaceCommand final : public Command
	{
	public:
		explicit SaveWorkspaceCommand(std::filesystem::path path);
		const char *GetName() const noexcept override;
		bool CanExecute(const CommandContext &context) const override;
		bool Execute(CommandContext &context) override;
		bool IsUndoable() const noexcept override;
		bool SetsSavePoint() const noexcept override;

	private:
		std::filesystem::path path_;
	};

	/// Closes the current document without saving.
	class CloseWorkspaceCommand final : public Command
	{
	public:
		const char *GetName() const noexcept override;
		bool CanExecute(const CommandContext &context) const override;
		bool Execute(CommandContext &context) override;
		bool ClearsHistory() const noexcept override;
	};

	/// Adds a new object to the current document.
	class AddObjectCommand final : public Command
	{
	public:
		explicit AddObjectCommand(ObjectKind kind = ObjectKind::Group);
		const char *GetName() const noexcept override;
		bool CanExecute(const CommandContext &context) const override;
		bool Execute(CommandContext &context) override;
		bool Undo(CommandContext &context) override;

	private:
		ObjectKind kind_;
		SceneObject object_;
		bool object_created_ = false;
	};

	/// Removes one object and restores its position on undo.
	class DeleteObjectCommand final : public Command
	{
	public:
		explicit DeleteObjectCommand(std::string id);
		const char *GetName() const noexcept override;
		bool CanExecute(const CommandContext &context) const override;
		bool Execute(CommandContext &context) override;
		bool Undo(CommandContext &context) override;

	private:
		std::string id_;
		SceneObject removed_;
		std::size_t removed_index_ = 0;
		bool was_selected_ = false;
	};

	/// Renames one object and restores the previous name on undo.
	class RenameObjectCommand final : public Command
	{
	public:
		RenameObjectCommand(std::string id, std::string name);
		const char *GetName() const noexcept override;
		bool CanExecute(const CommandContext &context) const override;
		bool Execute(CommandContext &context) override;
		bool Undo(CommandContext &context) override;

	private:
		std::string id_;
		std::string name_;
		std::string previous_name_;
		bool name_captured_ = false;
	};

} // namespace BlackLight::Editor
