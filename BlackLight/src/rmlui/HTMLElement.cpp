#include "rmlui/HTMLElement.h"

#include <algorithm>
#include <cassert>

namespace BlackLight::UI {

HTMLElement::HTMLElement(const std::string& tag, HTMLElementKind kind)
	: tag_name_(tag), kind_(kind), parent_(nullptr) {}

HTMLElement::~HTMLElement() = default;

const std::string& HTMLElement::GetTagName() const {
	return tag_name_;
}

HTMLElementKind HTMLElement::GetKind() const {
	return kind_;
}

const std::string& HTMLElement::GetId() const {
	return id_;
}

void HTMLElement::SetId(const std::string& id) {
	id_ = id;
}

const std::vector<std::string>& HTMLElement::GetClasses() const {
	return classes_;
}

void HTMLElement::AddClass(const std::string& css_class) {
	if (!css_class.empty()) {
		classes_.push_back(css_class);
	}
}

bool HTMLElement::HasClass(const std::string& css_class) const {
	return std::find(classes_.begin(), classes_.end(), css_class) != classes_.end();
}

const std::string& HTMLElement::GetStyle() const {
	return style_;
}

void HTMLElement::SetStyle(const std::string& style) {
	style_ = style;
}

HTMLElement* HTMLElement::GetParent() const {
	return parent_;
}

void HTMLElement::SetParent(HTMLElement* parent) {
	parent_ = parent;
}

HTMLText::HTMLText(const std::string& text)
	: HTMLElement("#text", HTMLElementKind::Text), text_(text) {}

HTMLText::~HTMLText() = default;

const std::string& HTMLText::GetText() const {
	return text_;
}

void HTMLText::SetText(const std::string& text) {
	text_ = text;
}

HTMLContainer::HTMLContainer(const std::string& tag, HTMLElementKind kind)
	: HTMLElement(tag, kind) {}

HTMLContainer::~HTMLContainer() = default;

const std::vector<std::unique_ptr<HTMLElement>>& HTMLContainer::GetChildren() const {
	return children_;
}

void HTMLContainer::AddChild(std::unique_ptr<HTMLElement> child) {
	if (child) {
		child->SetParent(this);
		children_.push_back(std::move(child));
	}
}

bool HTMLContainer::RemoveChild(HTMLElement* child) {
	auto it = std::find_if(children_.begin(), children_.end(),
		[child](const auto& ptr) { return ptr.get() == child; });
	
	if (it != children_.end()) {
		child->SetParent(nullptr);
		children_.erase(it);
		return true;
	}
	return false;
}

HTMLElement* HTMLContainer::FindChildById(const std::string& id) const {
	for (const auto& child : children_) {
		if (child->GetId() == id) {
			return child.get();
		}
		if (auto* container = dynamic_cast<HTMLContainer*>(child.get())) {
			if (auto* found = container->FindChildById(id)) {
				return found;
			}
		}
	}
	return nullptr;
}

std::vector<HTMLElement*> HTMLContainer::FindChildrenByClass(const std::string& css_class) const {
	std::vector<HTMLElement*> results;
	FindByClassRecursive(css_class, results);
	return results;
}

void HTMLContainer::FindByClassRecursive(
	const std::string& css_class,
	std::vector<HTMLElement*>& results) const
{
	for (const auto& child : children_) {
		if (child->HasClass(css_class)) {
			results.push_back(child.get());
		}
		if (auto* container = dynamic_cast<HTMLContainer*>(child.get())) {
			container->FindByClassRecursive(css_class, results);
		}
	}
}

std::size_t HTMLContainer::GetChildCount() const {
	return children_.size();
}

HTMLElement* HTMLContainer::GetChildAt(std::size_t index) const {
	if (index < children_.size()) {
		return children_[index].get();
	}
	return nullptr;
}

void HTMLContainer::Clear() {
	for (auto& child : children_) {
		child->SetParent(nullptr);
	}
	children_.clear();
}

HTMLDocument::HTMLDocument()
	: HTMLContainer("", HTMLElementKind::Document) {}

HTMLDocument::~HTMLDocument() = default;

HTMLElement* HTMLDocument::FindById(const std::string& id) const {
	return FindChildById(id);
}

std::vector<HTMLElement*> HTMLDocument::FindByClass(const std::string& css_class) const {
	return FindChildrenByClass(css_class);
}

const std::vector<std::string>& HTMLDocument::GetStyleSheets() const {
	return stylesheets_;
}

void HTMLDocument::AddStyleSheet(const std::string& css) {
	if (!css.empty()) {
		stylesheets_.push_back(css);
	}
}

void HTMLDocument::ClearStyleSheets() {
	stylesheets_.clear();
}

std::size_t HTMLDocument::GetStyleSheetCount() const {
	return stylesheets_.size();
}

}
