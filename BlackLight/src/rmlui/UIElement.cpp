#include "rmlui/UIElement.h"

#include <utility>

namespace BlackLight::UI {

/// Initializes the common identity and kind fields for a UI node.
UIElement::UIElement(UIElementKind kind, std::string id)
	: kind_(kind), id_(std::move(id)) {}

/// Releases the node's polymorphic base state.
UIElement::~UIElement() = default;

/// Returns the kind recorded by the concrete node constructor.
UIElementKind UIElement::GetKind() const noexcept {
	return kind_;
}

/// Returns the identifier assigned to this node.
const std::string& UIElement::GetId() const noexcept {
	return id_;
}

/// Returns the mutable parent pointer for non-const tree edits.
UIContainer* UIElement::GetParent() noexcept {
	return parent_;
}

/// Returns the parent pointer for read-only tree inspection.
const UIContainer* UIElement::GetParent() const noexcept {
	return parent_;
}

/// Initializes a container node and its sizing configuration.
UIContainer::UIContainer(UIElementKind kind, std::string id, UIElementLayout layout)
	: UIElement(kind, std::move(id)), layout_(std::move(layout)) {}

/// Releases the container and its owned children.
UIContainer::~UIContainer() = default;

/// Returns this container's sizing configuration.
const UIElementLayout& UIContainer::GetLayout() const noexcept {
	return layout_;
}

/// Returns the owned child nodes in insertion order.
const std::vector<std::unique_ptr<UIElement>>& UIContainer::GetChildren() const noexcept {
	return children_;
}

/// Adds a child while preserving the tree's single-parent and acyclic invariants.
bool UIContainer::AddChild(std::unique_ptr<UIElement> child) {
	if (!child || child->GetParent() != nullptr) {
		return false;
	}

	for (UIElement* ancestor = this; ancestor != nullptr; ancestor = ancestor->GetParent()) {
		if (ancestor == child.get()) {
			return false;
		}
	}

	children_.push_back(std::move(child));
	children_.back()->parent_ = this;
	return true;
}

/// Constructs the root screen node.
UIScreen::UIScreen(std::string id, UIElementLayout layout)
	: UIContainer(UIElementKind::Screen, std::move(id), std::move(layout)) {}

/// Constructs a panel node.
UIPanel::UIPanel(std::string id, UIElementLayout layout)
	: UIContainer(UIElementKind::Panel, std::move(id), std::move(layout)) {}

/// Constructs a leaf text node.
UIText::UIText(std::string text, std::string id)
	: UIElement(UIElementKind::Text, std::move(id)), text_(std::move(text)) {}

/// Returns the source text held by this leaf node.
const std::string& UIText::GetText() const noexcept {
	return text_;
}

} // namespace BlackLight::UI
