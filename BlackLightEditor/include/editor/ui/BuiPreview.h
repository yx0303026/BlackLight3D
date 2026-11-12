#pragma once

#include "editor/uidoc/BuiDocumentModel.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace Rml
{
	class Context;
	class Element;
	class ElementDocument;
	class Event;
}

namespace BlackLight::UI
{
	class BuiTree;
}

namespace BlackLight::Editor
{

	/// Builds and positions the live `.bui` preview document.
	///
	/// Preview markup is generated from the document model with synthetic
	/// `data-bl-node` attributes. Those attributes never enter the editable model
	/// or the saved file.
	class BuiPreview final
	{
	public:
		/// Receives the structural path selected by a click in the preview.
		using SelectionHandler = std::function<void(std::string_view)>;

		BuiPreview();
		~BuiPreview();

		BuiPreview(const BuiPreview &) = delete;
		BuiPreview &operator=(const BuiPreview &) = delete;

		/// Rebuilds the preview document from the model.
		bool Rebuild(Rml::Context &context, const BuiDocumentModel &model,
			std::string &error);
		/// Removes the preview document.
		void Close();
		/// Returns whether a preview document is loaded.
		bool IsOpen() const noexcept;
		/// Returns the loaded preview document.
		Rml::ElementDocument *GetDocument() const noexcept;

		/// Anchors the preview over the supplied editor element.
		void AnchorTo(Rml::Element *viewport);
		/// Reapplies the last anchor, for per-frame layout synchronization.
		void Update();
		/// Leaves a margin around the preview inside the viewport.
		void SetMargin(float margin) noexcept;

		/// Binds click handling so preview clicks update the model selection.
		void SetSelectionHandler(SelectionHandler handler);
		/// Routes one preview click through the selection handler.
		void HandleClick(Rml::Event &event);

	private:
		std::unique_ptr<BlackLight::UI::BuiTree> tree_;
		Rml::Element *anchor_ = nullptr;
		SelectionHandler selection_handler_;
		float margin_ = 8.0f;
	};

} // namespace BlackLight::Editor
