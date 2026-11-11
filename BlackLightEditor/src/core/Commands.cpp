#include "editor/core/Commands.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace BlackLight::Editor
{

	bool Command::CanExecute(const CommandContext & /*context*/) const { return true; }

	bool Command::Undo(CommandContext & /*context*/) { return false; }

	bool Command::IsUndoable() const noexcept { return true; }

	bool Command::ClearsHistory() const noexcept { return false; }

	bool Command::SetsSavePoint() const noexcept { return false; }

	NewWorkspaceCommand::NewWorkspaceCommand(std::string name) : name_(std::move(name)) {}

	const char *NewWorkspaceCommand::GetName() const noexcept { return "New Workspace"; }

	bool NewWorkspaceCommand::Execute(CommandContext &context)
	{
		context.state.NewDocument(name_);
		return context.state.HasDocument();
	}

	bool NewWorkspaceCommand::ClearsHistory() const noexcept { return true; }

	OpenWorkspaceCommand::OpenWorkspaceCommand(std::filesystem::path path) : path_(std::move(path)) {}

	const char *OpenWorkspaceCommand::GetName() const noexcept { return "Open Workspace"; }

	bool OpenWorkspaceCommand::CanExecute(const CommandContext & /*context*/) const
	{
		return !path_.empty();
	}

	bool OpenWorkspaceCommand::Execute(CommandContext &context)
	{
		if (!context.state.OpenDocument(path_))
		{
			context.state.SetStatus("Failed to open " + path_.filename().u8string());
			return false;
		}
		return true;
	}

	bool OpenWorkspaceCommand::ClearsHistory() const noexcept { return true; }

	SaveWorkspaceCommand::SaveWorkspaceCommand(std::filesystem::path path) : path_(std::move(path)) {}

	const char *SaveWorkspaceCommand::GetName() const noexcept { return "Save Workspace"; }

	bool SaveWorkspaceCommand::CanExecute(const CommandContext &context) const
	{
		return context.state.HasDocument() && !path_.empty();
	}

	bool SaveWorkspaceCommand::Execute(CommandContext &context)
	{
		if (!context.state.SaveDocument(path_))
		{
			context.state.SetStatus("Failed to save " + path_.filename().u8string());
			return false;
		}
		return true;
	}

	bool SaveWorkspaceCommand::IsUndoable() const noexcept { return false; }

	bool SaveWorkspaceCommand::SetsSavePoint() const noexcept { return true; }

	const char *CloseWorkspaceCommand::GetName() const noexcept { return "Close Workspace"; }

	bool CloseWorkspaceCommand::CanExecute(const CommandContext &context) const
	{
		return context.state.HasDocument();
	}

	bool CloseWorkspaceCommand::Execute(CommandContext &context)
	{
		context.state.CloseDocument();
		return true;
	}

	bool CloseWorkspaceCommand::ClearsHistory() const noexcept { return true; }

	AddObjectCommand::AddObjectCommand(ObjectKind kind) : kind_(kind) {}

	const char *AddObjectCommand::GetName() const noexcept { return "Add Object"; }

	bool AddObjectCommand::CanExecute(const CommandContext &context) const
	{
		return context.state.HasDocument();
	}

	bool AddObjectCommand::Execute(CommandContext &context)
	{
		if (!context.state.HasDocument())
			return false;
		if (!object_created_)
		{
			object_.id = context.state.GenerateObjectId();
			object_.kind = kind_;
			object_.name = context.state.MakeUniqueObjectName(ToString(kind_));
			object_created_ = true;
		}
		if (!context.state.AddObject(object_))
			return false;
		context.state.SetSelection(SelectionKind::Object, object_.id);
		return true;
	}

	bool AddObjectCommand::Undo(CommandContext &context)
	{
		return context.state.RemoveObject(object_.id);
	}

	DeleteObjectCommand::DeleteObjectCommand(std::string id) : id_(std::move(id)) {}

	const char *DeleteObjectCommand::GetName() const noexcept { return "Delete Object"; }

	bool DeleteObjectCommand::CanExecute(const CommandContext &context) const
	{
		return context.state.FindObject(id_) != nullptr;
	}

	bool DeleteObjectCommand::Execute(CommandContext &context)
	{
		const auto &objects = context.state.GetObjects();
		const auto it = std::find_if(objects.begin(), objects.end(),
			[this](const SceneObject &object) { return object.id == id_; });
		if (it == objects.end())
			return false;

		removed_ = *it;
		removed_index_ = static_cast<std::size_t>(std::distance(objects.begin(), it));
		was_selected_ = context.state.GetSelection().kind == SelectionKind::Object &&
			context.state.GetSelection().id == id_;
		return context.state.RemoveObject(id_);
	}

	bool DeleteObjectCommand::Undo(CommandContext &context)
	{
		if (!context.state.InsertObject(removed_index_, removed_))
			return false;
		if (was_selected_)
			context.state.SetSelection(SelectionKind::Object, removed_.id);
		return true;
	}

	RenameObjectCommand::RenameObjectCommand(std::string id, std::string name)
		: id_(std::move(id)), name_(std::move(name))
	{
	}

	const char *RenameObjectCommand::GetName() const noexcept { return "Rename Object"; }

	bool RenameObjectCommand::CanExecute(const CommandContext &context) const
	{
		return context.state.FindObject(id_) != nullptr && !name_.empty();
	}

	bool RenameObjectCommand::Execute(CommandContext &context)
	{
		const SceneObject *object = context.state.FindObject(id_);
		if (!object)
			return false;
		if (!name_captured_)
		{
			previous_name_ = object->name;
			name_captured_ = true;
		}
		return context.state.RenameObject(id_, name_);
	}

	bool RenameObjectCommand::Undo(CommandContext &context)
	{
		return name_captured_ && context.state.RenameObject(id_, previous_name_);
	}

} // namespace BlackLight::Editor
