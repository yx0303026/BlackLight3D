#pragma once

#include "editor/ui/BuiGizmoMath.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>

namespace Rml
{
	class Element;
	class ElementDocument;
	class Event;
}

namespace BlackLight::Editor
{

	/// Provides a 2D move/resize gizmo for the live `.bui` preview.
	///
	/// The gizmo owns only runtime overlay elements. It never changes the source
	/// document directly; completed gestures are reported through the callback so
	/// the editor can apply one undoable model edit and let the preview rebuild.
	class BuiGizmo final
	{
	public:
		/// Receives the selected structural path and the final rectangle.
		using EditHandler = std::function<void(std::string_view,
			const BuiGizmoGeometry &)>;

		BuiGizmo();
		~BuiGizmo();

		BuiGizmo(const BuiGizmo &) = delete;
		BuiGizmo &operator=(const BuiGizmo &) = delete;

		/// Attaches a runtime overlay to the supplied preview document.
		void Attach(Rml::ElementDocument *document);
		/// Removes the runtime overlay and clears transient drag state.
		void Detach();

		/// Enables or disables hit testing and overlay rendering.
		void SetEnabled(bool enabled) noexcept;
		/// Returns whether the gizmo currently accepts pointer input.
		bool IsEnabled() const noexcept;

		/// Sets the structural path represented by the gizmo.
		void SetSelectionPath(std::string_view path);
		/// Returns the currently represented structural path.
		const std::string &GetSelectionPath() const noexcept;

		/// Snaps committed and nudged values to the supplied grid; zero disables it.
		void SetGridStep(float step) noexcept;
		/// Returns the current snap grid in density-independent pixels.
		float GetGridStep() const noexcept;

		/// Recomputes the overlay rectangle from the selected preview element.
		void Update();

		/// Routes a preview mousedown event into move/resize hit testing.
		void HandleMouseDown(Rml::Event &event);
		/// Routes a preview mousemove event while a drag is active.
		void HandleMouseMove(Rml::Event &event);
		/// Completes the active drag and reports the resulting rectangle.
		void HandleMouseUp(Rml::Event &event);

		/// Offsets the selection by one keyboard step and reports it immediately.
		bool Nudge(float delta_x, float delta_y);
		/// Abandons the active drag and restores the element's authored style.
		void Cancel();
		/// Returns whether a drag gesture is currently in progress.
		bool IsDragging() const noexcept;

		/// Returns whether a drag ended since the last call, so the editor can
		/// swallow the click event that the same gesture would otherwise produce.
		bool ConsumeClickSuppression() noexcept;

		/// Installs the callback invoked after a move or resize gesture completes.
		void SetEditHandler(EditHandler handler);

	private:
		/// Finds the runtime element carrying the selected structural path.
		Rml::Element *FindSelectedElement() const;
		/// Finds a path-bearing descendant recursively.
		Rml::Element *FindElementByPath(Rml::Element *element) const;
		/// Returns the rectangle relative to the selected element's offset parent.
		BuiGizmoGeometry ReadGeometry(Rml::Element *element) const;
		/// Applies a transient geometry to the selected runtime element while dragging.
		void ApplyTransientGeometry(Rml::Element *element,
			const BuiGizmoGeometry &geometry);
		/// Restores the element's authored inline geometry after a cancelled gesture.
		void RestoreAuthoredGeometry(Rml::Element *element);
		/// Updates the overlay rectangle and its eight handles.
		void ApplyOverlayGeometry(Rml::Element *element, const BuiGizmoGeometry &geometry);
		/// Shows or hides the overlay while preserving the current selection.
		void SetOverlayVisible(bool visible);
		/// Converts an RmlUi mouse event into offset-parent coordinates.
		bool MousePosition(Rml::Event &event, float &x, float &y) const;
		/// Reports the current rectangle through the host callback and resyncs.
		void Commit(const BuiGizmoGeometry &geometry);

		Rml::ElementDocument *document_ = nullptr;
		Rml::Element *overlay_ = nullptr;
		std::array<Rml::Element *, 8> handles_{};
		std::string selection_path_;
		EditHandler edit_handler_;

		BuiGizmoHandle drag_handle_ = BuiGizmoHandle::None;
		BuiGizmoGeometry drag_start_geometry_;
		float drag_start_x_ = 0.0f;
		float drag_start_y_ = 0.0f;
		BuiGizmoGeometry transient_geometry_;
		std::string drag_original_style_;
		bool drag_had_style_ = false;
		bool suppress_click_ = false;

		float grid_step_ = 0.0f;
		float density_ = 1.0f;
		bool enabled_ = true;
	};

} // namespace BlackLight::Editor
