#include "rmlui/RmlUiTree.h"

#include <string>
#include <utility>

#include <RmlUi/Core.h>

namespace BlackLight::UI {
namespace {

/// Reports a native element creation failure with the corresponding UI identifier.
void SetBuildError(UIBuildError& error, std::string message, const UIElement* element = nullptr) {
	if (element != nullptr && !element->GetId().empty()) {
		message += " (c:id=" + element->GetId() + ")";
	}
	error.message = std::move(message);
}

/// Applies the common native identifier to an RmlUi element.
void ApplyIdentifier(Rml::Element& native_element, const UIElement& element) {
	if (!element.GetId().empty()) {
		native_element.SetId(element.GetId());
	}
}

/// Configures RmlUi flex sizing from the container's per-axis layout rules.
void ApplyLayout(Rml::Element& native_element, const UIElementLayout& layout) {
	native_element.SetProperty("display", "flex");
	native_element.SetProperty("flex-direction", "column");
	native_element.SetProperty("flex-shrink", "0");
	native_element.SetProperty("align-self", layout.auto_size ? "flex-start" : "stretch");
	const bool content_sized_width = layout.auto_size && (!layout.width.has_value() || *layout.width == "auto");
	native_element.SetProperty("align-items", content_sized_width ? "flex-start" : "stretch");
	native_element.SetProperty("box-sizing", "border-box");
	native_element.SetProperty("width", layout.width.value_or(layout.auto_size ? "auto" : "100%"));
	native_element.SetProperty("height", layout.height.value_or(layout.auto_size ? "auto" : "100%"));
}

/// Creates one panel or text element, appends it, indexes its ID, and builds its children.
bool AppendNode(const UIElement& source, Rml::ElementDocument& document, Rml::Element& parent,
	std::unordered_map<std::string, Rml::Element*>& elements_by_id, UIBuildError& error) {
	Rml::ElementPtr native_owner;
	if (source.GetKind() == UIElementKind::Panel) {
		native_owner = document.CreateElement("div");
	} else if (source.GetKind() == UIElementKind::Text) {
		const auto* text = dynamic_cast<const UIText*>(&source);
		if (text == nullptr) {
			SetBuildError(error, "Text node type did not match its kind.", &source);
			return false;
		}
		native_owner = document.CreateElement("div");
		if (native_owner) {
			auto text_node = document.CreateTextNode(text->GetText());
			if (!text_node) {
				SetBuildError(error, "RmlUi could not create a text node.", &source);
				return false;
			}
			if (native_owner->AppendChild(std::move(text_node)) == nullptr) {
				SetBuildError(error, "RmlUi could not append a text node.", &source);
				return false;
			}
			native_owner->SetProperty("display", "block");
			native_owner->SetProperty("width", "auto");
			native_owner->SetProperty("height", "auto");
			native_owner->SetProperty("flex-shrink", "0");
		}
	} else {
		SetBuildError(error, "Only Panel and Text nodes can be nested below a Screen.", &source);
		return false;
	}

	if (!native_owner) {
		SetBuildError(error, "RmlUi could not create an element.", &source);
		return false;
	}

	Rml::Element* native_element = parent.AppendChild(std::move(native_owner));
	if (native_element == nullptr) {
		SetBuildError(error, "RmlUi could not append an element.", &source);
		return false;
	}
	ApplyIdentifier(*native_element, source);

	if (!source.GetId().empty()) {
		const auto inserted = elements_by_id.emplace(source.GetId(), native_element);
		if (!inserted.second) {
			SetBuildError(error, "Control identifiers must be unique.", &source);
			return false;
		}
	}

	if (source.GetKind() == UIElementKind::Panel) {
		const auto* panel = dynamic_cast<const UIPanel*>(&source);
		if (panel == nullptr) {
			SetBuildError(error, "Panel node type did not match its kind.", &source);
			return false;
		}
		ApplyLayout(*native_element, panel->GetLayout());
		for (const std::unique_ptr<UIElement>& child : panel->GetChildren()) {
			if (!AppendNode(*child, document, *native_element, elements_by_id, error)) {
				return false;
			}
		}
	}

	return true;
}

} // namespace

/// Creates an empty RmlUi wrapper before a document is attached.
RmlUiTree::RmlUiTree() = default;

/// Closes the document before releasing the native lookup table.
RmlUiTree::~RmlUiTree() {
	Close();
}

/// Creates an RmlUi document for the full Screen tree.
bool RmlUiTree::Build(Rml::Context& context, const UIScreen& screen,
	std::unique_ptr<RmlUiTree>& tree, UIBuildError& error) {
	error = {};
	auto built_tree = std::unique_ptr<RmlUiTree>(new RmlUiTree());
	built_tree->document_ = context.CreateDocument("body");
	if (built_tree->document_ == nullptr) {
		SetBuildError(error, "RmlUi could not create a document.", &screen);
		return false;
	}

	ApplyIdentifier(*built_tree->document_, screen);
	ApplyLayout(*built_tree->document_, screen.GetLayout());
	built_tree->document_->SetProperty("font-family", "Arial");
	built_tree->document_->SetProperty("color", "white");
	if (!screen.GetId().empty()) {
		built_tree->elements_by_id_.emplace(screen.GetId(), built_tree->document_);
	}

	for (const std::unique_ptr<UIElement>& child : screen.GetChildren()) {
		if (!AppendNode(*child, *built_tree->document_, *built_tree->document_,
			built_tree->elements_by_id_, error)) {
			built_tree->Close();
			return false;
		}
	}

	tree = std::move(built_tree);
	return true;
}

/// Returns the root document while it remains open.
Rml::ElementDocument* RmlUiTree::GetDocument() const noexcept {
	return document_;
}

/// Looks up a native element by its source control identifier.
Rml::Element* RmlUiTree::FindById(std::string_view id) const {
	const auto found = elements_by_id_.find(std::string(id));
	return found == elements_by_id_.end() ? nullptr : found->second;
}

/// Makes the built document visible in its owning RmlUi context.
void RmlUiTree::Show() {
	if (document_ != nullptr) {
		document_->Show();
	}
}

/// Closes the document and clears all non-owning native element pointers.
void RmlUiTree::Close() {
	if (document_ != nullptr) {
		document_->Close();
		document_ = nullptr;
	}
	elements_by_id_.clear();
}

} // namespace BlackLight::UI
