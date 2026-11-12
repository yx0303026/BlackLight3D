#include "editor/ui/BuiPreview.h"

#include "editor/ui/EditorUi.h"
#include "rmlui/BuiDocument.h"
#include "rmlui/BuiTree.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <string>
#include <utility>

namespace BlackLight::Editor
{

	BuiPreview::BuiPreview() = default;

	BuiPreview::~BuiPreview() { Close(); }

	bool BuiPreview::Rebuild(Rml::Context &context, const BuiDocumentModel &model,
		std::string &error)
	{
		Close();
		BlackLight::UI::BuiDocument document;
		document.SetStyles(model.GetStyles());
		document.SetLayout(model.BuildPreviewLayout());
		BlackLight::UI::BuiBuildError build_error;
		if (!BlackLight::UI::BuiTree::Build(context, document, tree_, build_error))
		{
			error = build_error.message;
			return false;
		}
		tree_->Show();
		error.clear();
		return true;
	}

	void BuiPreview::Close()
	{
		tree_.reset();
		anchor_ = nullptr;
	}

	void BuiPreview::Update()
	{
		AnchorTo(anchor_);
	}

	bool BuiPreview::IsOpen() const noexcept { return tree_ && tree_->GetDocument(); }

	Rml::ElementDocument *BuiPreview::GetDocument() const noexcept
	{
		return tree_ ? tree_->GetDocument() : nullptr;
	}

	void BuiPreview::AnchorTo(Rml::Element *viewport)
	{
		anchor_ = viewport;
		Rml::ElementDocument *document = GetDocument();
		if (!anchor_ || !document)
			return;

		anchor_->GetBox();
		const Rml::Vector2f offset = anchor_->GetAbsoluteOffset(Rml::BoxArea::Border);
		const Rml::Vector2f size = anchor_->GetBox().GetSize(Rml::BoxArea::Border);
		document->SetProperty("position", "absolute");
		document->SetProperty("left", std::to_string(offset.x + margin_) + "px");
		document->SetProperty("top", std::to_string(offset.y + margin_) + "px");
		document->SetProperty("width",
			std::to_string((std::max)(1.0f, size.x - margin_ * 2.0f)) + "px");
		document->SetProperty("height",
			std::to_string((std::max)(1.0f, size.y - margin_ * 2.0f)) + "px");
		document->SetProperty("z-index", "50");
		document->SetProperty("background-color", "transparent");
		document->SetProperty("overflow", "hidden");
	}

	void BuiPreview::SetMargin(float margin) noexcept
	{
		margin_ = margin;
	}

	void BuiPreview::SetSelectionHandler(SelectionHandler handler)
	{
		selection_handler_ = std::move(handler);
	}

	void BuiPreview::HandleClick(Rml::Event &event)
	{
		Rml::ElementDocument *document = GetDocument();
		if (!document)
			return;
		const std::string path = EditorUi::FindAttribute(event.GetTargetElement(),
			document, "data-bl-node");
		if (!path.empty() && selection_handler_)
			selection_handler_(path);
	}

} // namespace BlackLight::Editor
