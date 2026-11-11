#include "editor/core/EditorState.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace BlackLight::Editor
{

	namespace
	{

		constexpr const char *WorkspaceHeader = "blacklight-workspace 1";

		/// Removes leading and trailing ASCII whitespace.
		std::string Trim(std::string_view text)
		{
			std::size_t begin = 0;
			std::size_t end = text.size();
			while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
				++begin;
			while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
				--end;
			return std::string(text.substr(begin, end - begin));
		}

		/// Lowercases an ASCII token.
		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			return text;
		}

		/// Keeps display names on a single line so the workspace format stays line-based.
		std::string SanitizeName(std::string_view name)
		{
			std::string result = Trim(name);
			for (char &character : result)
			{
				if (character == '\r' || character == '\n' || character == '\t')
					character = ' ';
			}
			return result;
		}

	} // namespace

	const char *ToString(EditorMode mode) noexcept
	{
		switch (mode)
		{
		case EditorMode::Edit: return "Edit";
		case EditorMode::Play: return "Play";
		case EditorMode::Paused: return "Paused";
		}
		return "Unknown";
	}

	const char *ToString(ObjectKind kind) noexcept
	{
		switch (kind)
		{
		case ObjectKind::Group: return "Group";
		case ObjectKind::Camera: return "Camera";
		case ObjectKind::Light: return "Light";
		case ObjectKind::Sprite: return "Sprite";
		case ObjectKind::Audio: return "Audio";
		case ObjectKind::Script: return "Script";
		}
		return "Unknown";
	}

	const char *ToString(SelectionKind kind) noexcept
	{
		switch (kind)
		{
		case SelectionKind::None: return "None";
		case SelectionKind::Object: return "Object";
		case SelectionKind::Asset: return "Asset";
		}
		return "Unknown";
	}

	bool ObjectKindFromString(std::string_view text, ObjectKind &kind) noexcept
	{
		const std::string lowered = ToLower(std::string(text));
		if (lowered == "group") { kind = ObjectKind::Group; return true; }
		if (lowered == "camera") { kind = ObjectKind::Camera; return true; }
		if (lowered == "light") { kind = ObjectKind::Light; return true; }
		if (lowered == "sprite") { kind = ObjectKind::Sprite; return true; }
		if (lowered == "audio") { kind = ObjectKind::Audio; return true; }
		if (lowered == "script") { kind = ObjectKind::Script; return true; }
		return false;
	}

	EditorState::EditorState(EventBus &events) : events_(&events) {}

	bool EditorState::HasDocument() const noexcept { return !document_name_.empty(); }

	const std::string &EditorState::GetDocumentName() const noexcept { return document_name_; }

	const std::filesystem::path &EditorState::GetDocumentPath() const noexcept { return document_path_; }

	bool EditorState::IsModified() const noexcept { return modified_; }

	void EditorState::NewDocument(std::string name)
	{
		const std::string sanitized = SanitizeName(name);
		document_name_ = sanitized.empty() ? "Untitled Workspace" : sanitized;
		document_path_.clear();
		objects_.clear();
		selection_ = {};
		modified_ = false;
		Emit(EventType::DocumentChanged, document_name_, "new");
		Emit(EventType::SelectionChanged, {}, ToString(SelectionKind::None));
	}

	void EditorState::CloseDocument()
	{
		if (!HasDocument())
			return;
		document_name_.clear();
		document_path_.clear();
		objects_.clear();
		selection_ = {};
		modified_ = false;
		Emit(EventType::DocumentChanged, {}, "close");
		Emit(EventType::SelectionChanged, {}, ToString(SelectionKind::None));
	}

	bool EditorState::OpenDocument(const std::filesystem::path &path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
			return false;

		std::vector<SceneObject> loaded_objects;
		std::string loaded_name;
		std::string line;
		bool header_seen = false;
		bool first_line = true;
		while (std::getline(input, line))
		{
			if (first_line)
			{
				first_line = false;
				if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
					static_cast<unsigned char>(line[1]) == 0xBB &&
					static_cast<unsigned char>(line[2]) == 0xBF)
				{
					line.erase(0, 3);
				}
			}
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (Trim(line).empty())
				continue;

			if (!header_seen)
			{
				if (Trim(line) != WorkspaceHeader)
					return false;
				header_seen = true;
				continue;
			}

			std::istringstream stream(line);
			std::string token;
			stream >> token;
			if (token == "name")
			{
				std::string value;
				std::getline(stream, value);
				loaded_name = SanitizeName(value);
				continue;
			}
			if (token == "object")
			{
				SceneObject object;
				std::string kind_text;
				if (!(stream >> object.id >> kind_text))
					return false;
				if (object.id.empty() || !ObjectKindFromString(kind_text, object.kind))
					return false;
				std::string value;
				std::getline(stream, value);
				object.name = SanitizeName(value);
				if (object.name.empty())
					object.name = object.id;

				const bool duplicate = std::any_of(loaded_objects.begin(), loaded_objects.end(),
					[&object](const SceneObject &existing) { return existing.id == object.id; });
				if (duplicate)
					return false;
				loaded_objects.push_back(std::move(object));
				continue;
			}
			// Unknown directives are ignored so newer files remain readable.
		}

		if (!header_seen)
			return false;
		if (loaded_name.empty())
			loaded_name = path.stem().u8string();
		if (loaded_name.empty())
			loaded_name = "Untitled Workspace";

		document_name_ = std::move(loaded_name);
		document_path_ = path;
		objects_ = std::move(loaded_objects);
		selection_ = {};
		modified_ = false;
		Emit(EventType::DocumentChanged, document_name_, "open");
		Emit(EventType::SelectionChanged, {}, ToString(SelectionKind::None));
		return true;
	}

	bool EditorState::SaveDocument(const std::filesystem::path &path)
	{
		if (!HasDocument() || path.empty())
			return false;

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output)
			return false;

		output << WorkspaceHeader << '\n';
		output << "name " << document_name_ << '\n';
		for (const auto &object : objects_)
			output << "object " << object.id << ' ' << ToString(object.kind) << ' ' << object.name << '\n';
		output.flush();
		if (!output.good())
			return false;

		document_path_ = path;
		Emit(EventType::DocumentChanged, document_name_, "save");
		MarkModified(false);
		return true;
	}

	void EditorState::MarkModified(bool modified)
	{
		if (modified_ == modified)
			return;
		modified_ = modified;
		Emit(EventType::DocumentModified, document_name_, modified ? "1" : "0");
	}

	const std::vector<SceneObject> &EditorState::GetObjects() const noexcept { return objects_; }

	const SceneObject *EditorState::FindObject(std::string_view id) const noexcept
	{
		for (const auto &object : objects_)
		{
			if (object.id == id)
				return &object;
		}
		return nullptr;
	}

	bool EditorState::AddObject(SceneObject object)
	{
		if (!HasDocument())
			return false;
		if (object.id.empty())
			object.id = GenerateObjectId();
		if (FindObject(object.id))
			return false;
		object.name = SanitizeName(object.name);
		if (object.name.empty())
			object.name = MakeUniqueObjectName(ToString(object.kind));
		objects_.push_back(std::move(object));
		Emit(EventType::ObjectsChanged, objects_.back().id, "add");
		MarkModified(true);
		return true;
	}

	bool EditorState::InsertObject(std::size_t index, SceneObject object)
	{
		if (!HasDocument() || object.id.empty() || FindObject(object.id))
			return false;
		object.name = SanitizeName(object.name);
		if (object.name.empty())
			object.name = object.id;
		index = std::min(index, objects_.size());
		objects_.insert(objects_.begin() + static_cast<std::ptrdiff_t>(index), std::move(object));
		const std::string inserted_id = objects_[index].id;
		Emit(EventType::ObjectsChanged, inserted_id, "insert");
		MarkModified(true);
		return true;
	}

	bool EditorState::RemoveObject(std::string_view id)
	{
		const auto it = std::find_if(objects_.begin(), objects_.end(),
			[id](const SceneObject &object) { return object.id == id; });
		if (it == objects_.end())
			return false;

		const std::string removed_id = it->id;
		const bool was_selected = selection_.kind == SelectionKind::Object && selection_.id == removed_id;
		objects_.erase(it);
		if (was_selected)
			selection_ = {};
		Emit(EventType::ObjectsChanged, removed_id, "remove");
		if (was_selected)
			Emit(EventType::SelectionChanged, {}, ToString(SelectionKind::None));
		MarkModified(true);
		return true;
	}

	bool EditorState::RenameObject(std::string_view id, std::string name)
	{
		const std::string sanitized = SanitizeName(name);
		if (sanitized.empty())
			return false;
		for (auto &object : objects_)
		{
			if (object.id != id)
				continue;
			if (object.name == sanitized)
				return true;
			object.name = sanitized;
			Emit(EventType::ObjectsChanged, object.id, "rename");
			MarkModified(true);
			return true;
		}
		return false;
	}

	std::string EditorState::GenerateObjectId() const
	{
		for (std::size_t number = 1;; ++number)
		{
			const std::string candidate = "obj-" + std::to_string(number);
			if (!FindObject(candidate))
				return candidate;
		}
	}

	std::string EditorState::MakeUniqueObjectName(std::string_view base) const
	{
		const std::string prefix = Trim(base).empty() ? "Object" : Trim(base);
		for (std::size_t number = 1;; ++number)
		{
			const std::string candidate = prefix + " " + std::to_string(number);
			const bool taken = std::any_of(objects_.begin(), objects_.end(),
				[&candidate](const SceneObject &object) { return object.name == candidate; });
			if (!taken)
				return candidate;
		}
	}

	const Selection &EditorState::GetSelection() const noexcept { return selection_; }

	void EditorState::SetSelection(SelectionKind kind, std::string id)
	{
		if (kind == SelectionKind::None || id.empty())
		{
			ClearSelection();
			return;
		}
		if (kind == SelectionKind::Object && !FindObject(id))
			return;
		if (selection_.kind == kind && selection_.id == id)
			return;
		selection_.kind = kind;
		selection_.id = std::move(id);
		Emit(EventType::SelectionChanged, selection_.id, ToString(selection_.kind));
	}

	void EditorState::ClearSelection()
	{
		if (selection_.kind == SelectionKind::None && selection_.id.empty())
			return;
		selection_ = {};
		Emit(EventType::SelectionChanged, {}, ToString(SelectionKind::None));
	}

	EditorMode EditorState::GetMode() const noexcept { return mode_; }

	void EditorState::SetMode(EditorMode mode)
	{
		if (mode_ == mode)
			return;
		mode_ = mode;
		Emit(EventType::ModeChanged, ToString(mode_), {});
	}

	const std::string &EditorState::GetStatus() const noexcept { return status_; }

	void EditorState::SetStatus(std::string text)
	{
		if (status_ == text)
			return;
		status_ = std::move(text);
		Emit(EventType::StatusMessage, status_, {});
	}

	void EditorState::Emit(EventType type, std::string name, std::string detail) const
	{
		if (events_)
			events_->Emit({type, std::move(name), std::move(detail)});
	}

} // namespace BlackLight::Editor
