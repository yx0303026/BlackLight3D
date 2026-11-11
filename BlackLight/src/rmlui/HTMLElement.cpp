#include "rmlui/HTMLElement.h"

#include <algorithm>
#include <utility>

namespace BlackLight::UI {

namespace {

HTMLElementAttributes MakeTextAttributes()
{
	HTMLElementAttributes attributes;
	attributes.tag_name = "#text";
	return attributes;
}

HTMLElementAttributes MakeDocumentAttributes(const HTMLElementAttributes& attributes)
{
	HTMLElementAttributes document_attributes = attributes;
	if (document_attributes.tag_name.empty()) {
		document_attributes.tag_name = "html";
	}
	return document_attributes;
}

bool HasClass(const HTMLElement& element, const std::string& class_name)
{
	const auto& classes = element.GetClasses();
	return std::find(classes.begin(), classes.end(), class_name) != classes.end();
}

void CollectByClass(HTMLContainer& container, const std::string& class_name, std::vector<HTMLElement*>& results)
{
	for (const auto& child : container.GetChildren()) {
		if (HasClass(*child, class_name)) {
			results.push_back(child.get());
		}
		if (auto* nested = dynamic_cast<HTMLContainer*>(child.get())) {
			CollectByClass(*nested, class_name, results);
		}
	}
}

} // namespace

HTMLElement::HTMLElement(HTMLElementKind kind, const HTMLElementAttributes& attributes)
	: kind_(kind), attributes_(attributes)
{
}

HTMLElement::~HTMLElement() = default;

HTMLElementKind HTMLElement::GetKind() const noexcept
{
	return kind_;
}

const std::string& HTMLElement::GetId() const noexcept
{
	return attributes_.id;
}

const std::vector<std::string>& HTMLElement::GetClasses() const noexcept
{
	return attributes_.classes;
}

const std::string& HTMLElement::GetStyle() const noexcept
{
	return attributes_.style;
}

const std::string& HTMLElement::GetTagName() const noexcept
{
	return attributes_.tag_name;
}

HTMLContainer* HTMLElement::GetParent() noexcept
{
	return parent_;
}

const HTMLContainer* HTMLElement::GetParent() const noexcept
{
	return parent_;
}

const HTMLElementAttributes& HTMLElement::GetAttributes() const noexcept
{
	return attributes_;
}

std::string HTMLElement::GetAttribute(const std::string& name) const
{
	const auto attribute = attributes_.custom_attributes.find(name);
	if (attribute == attributes_.custom_attributes.end()) {
		return {};
	}
	return attribute->second;
}

HTMLContainer::HTMLContainer(HTMLElementKind kind, const HTMLElementAttributes& attributes)
	: HTMLElement(kind, attributes)
{
}

HTMLContainer::~HTMLContainer() = default;

const std::vector<std::unique_ptr<HTMLElement>>& HTMLContainer::GetChildren() const noexcept
{
	return children_;
}

bool HTMLContainer::AddChild(std::unique_ptr<HTMLElement> child)
{
	if (!child || child->parent_ != nullptr) {
		return false;
	}
	child->parent_ = this;
	children_.push_back(std::move(child));
	return true;
}

HTMLElement* HTMLContainer::FindById(const std::string& id)
{
	for (const auto& child : children_) {
		if (child->GetId() == id) {
			return child.get();
		}
		if (auto* container = dynamic_cast<HTMLContainer*>(child.get())) {
			if (HTMLElement* found = container->FindById(id)) {
				return found;
			}
		}
	}
	return nullptr;
}

std::vector<HTMLElement*> HTMLContainer::FindByClass(const std::string& class_name)
{
	std::vector<HTMLElement*> results;
	CollectByClass(*this, class_name, results);
	return results;
}

HTMLDocument::HTMLDocument(const HTMLElementAttributes& attributes)
	: HTMLContainer(HTMLElementKind::Document, MakeDocumentAttributes(attributes))
{
}

void HTMLDocument::AddStyleSheet(std::string css)
{
	if (!css.empty()) {
		style_sheets_.push_back(std::move(css));
	}
}

const std::vector<std::string>& HTMLDocument::GetStyleSheets() const noexcept
{
	return style_sheets_;
}

void HTMLDocument::SetCharset(std::string charset)
{
	charset_ = std::move(charset);
}

const std::string& HTMLDocument::GetCharset() const noexcept
{
	return charset_;
}

void HTMLDocument::SetLanguage(std::string language)
{
	language_ = std::move(language);
}

const std::string& HTMLDocument::GetLanguage() const noexcept
{
	return language_;
}

HTMLText::HTMLText(std::string text)
	: HTMLElement(HTMLElementKind::Text, MakeTextAttributes()), text_(std::move(text))
{
}

const std::string& HTMLText::GetText() const noexcept
{
	return text_;
}

void HTMLText::SetText(std::string text)
{
	text_ = std::move(text);
}

} // namespace BlackLight::UI