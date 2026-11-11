#pragma once

#include "editor/core/EditorEvent.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Selects the editor interaction mode.
	enum class EditorMode : std::uint8_t
	{
		Edit,
		Play,
		Paused
	};

	/// Describes the kind of object stored in a workspace document.
	enum class ObjectKind : std::uint8_t
	{
		Group,
		Camera,
		Light,
		Sprite,
		Audio,
		Script
	};

	/// Describes what the editor currently has selected.
	enum class SelectionKind : std::uint8_t
	{
		None,
		Object,
		Asset
	};

	const char *ToString(EditorMode mode) noexcept;
	const char *ToString(ObjectKind kind) noexcept;
	const char *ToString(SelectionKind kind) noexcept;

	/// Parses an object kind written by the workspace file format.
	bool ObjectKindFromString(std::string_view text, ObjectKind &kind) noexcept;

	/// Represents one editable object in a workspace document.
	struct SceneObject
	{
		std::string id;
		std::string name;
		ObjectKind kind = ObjectKind::Group;
	};

	/// Identifies the current selection without owning the selected data.
	struct Selection
	{
		SelectionKind kind = SelectionKind::None;
		std::string id;
	};

	/// Owns the current workspace document, its selection, and the editor mode.
	///
	/// Every mutation that other systems can observe emits an event through the
	/// bus supplied at construction. This class performs no UI work, so it can be
	/// tested and reused without an RmlUi context.
	class EditorState final
	{
	public:
		/// Binds the state to the event bus that receives its notifications.
		explicit EditorState(EventBus &events);

		/// Returns whether a workspace document is open.
		bool HasDocument() const noexcept;
		/// Returns the display name of the open document.
		const std::string &GetDocumentName() const noexcept;
		/// Returns the workspace file path, or an empty path for an unsaved document.
		const std::filesystem::path &GetDocumentPath() const noexcept;
		/// Returns whether the document changed since the last save.
		bool IsModified() const noexcept;

		/// Discards the current document and starts an untitled workspace.
		void NewDocument(std::string name = "Untitled Workspace");
		/// Closes the current document and clears the selection.
		void CloseDocument();
		/// Loads a workspace file, replacing the current document on success.
		bool OpenDocument(const std::filesystem::path &path);
		/// Writes the current document to the supplied path.
		bool SaveDocument(const std::filesystem::path &path);
		/// Updates the modified flag and reports a change when it differs.
		void MarkModified(bool modified);

		/// Returns every object in document order.
		const std::vector<SceneObject> &GetObjects() const noexcept;
		/// Returns an object by identifier, or null when it does not exist.
		const SceneObject *FindObject(std::string_view id) const noexcept;
		/// Adds an object, assigning a missing identifier and name.
		/// Returns false when the document is closed or the identifier is taken.
		bool AddObject(SceneObject object);
		/// Inserts an object at the supplied document index for undo support.
		bool InsertObject(std::size_t index, SceneObject object);
		/// Removes an object and clears a matching selection.
		bool RemoveObject(std::string_view id);
		/// Changes an object's display name.
		bool RenameObject(std::string_view id, std::string name);
		/// Returns an identifier that no current object uses.
		std::string GenerateObjectId() const;
		/// Builds a unique display name using the supplied base text.
		std::string MakeUniqueObjectName(std::string_view base) const;

		/// Returns the current selection.
		const Selection &GetSelection() const noexcept;
		/// Selects an object or asset, or clears the selection when the kind is None.
		void SetSelection(SelectionKind kind, std::string id);
		/// Clears the selection.
		void ClearSelection();

		/// Returns the current editor mode.
		EditorMode GetMode() const noexcept;
		/// Changes the editor mode.
		void SetMode(EditorMode mode);

		/// Returns the status-bar text most recently set by a subsystem.
		const std::string &GetStatus() const noexcept;
		/// Sets the status-bar text and emits a status event when it changes.
		void SetStatus(std::string text);

	private:
		void Emit(EventType type, std::string name = {}, std::string detail = {}) const;

		EventBus *events_ = nullptr;
		std::string document_name_;
		std::filesystem::path document_path_;
		bool modified_ = false;
		std::vector<SceneObject> objects_;
		Selection selection_;
		EditorMode mode_ = EditorMode::Edit;
		std::string status_ = "Ready";
	};

} // namespace BlackLight::Editor
