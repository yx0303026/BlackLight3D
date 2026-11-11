#include "rmlui/RmlUiTree.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <RmlUi/Core.h>
#include <RmlUi/Core/StreamMemory.h>
#include <RmlUi/Core/StyleSheetContainer.h>

namespace BlackLight::UI {
namespace {

constexpr const char* DEFAULT_STYLE_SHEET = R"css(
html, body { 
    display: block; 
    width: 100%; 
    height: 100%; 
    font-family: Arial, sans-serif; 
    color: #f4f6f8;
    margin: 0;
    padding: 0;
}

div { display: block; box-sizing: border-box; }
button { 
    color: #ffffff; 
    background-color: #326b8c; 
    border: 1px solid #4d91b3; 
    padding: 7px 12px;
    cursor: pointer;
}
button:hover { background-color: #3f829f; }
button:active { background-color: #24536e; }

input, textarea, select {
    border: 1px solid #596775;
    padding: 4px 8px;
    font-family: Arial, sans-serif;
    color: #f4f6f8;
    background-color: #263746;
}

ul, ol { list-style-position: inside; }
li { margin-left: 20px; }

table { width: 100%; border-collapse: collapse; }
th, td { border: 1px solid #596775; padding: 8px; text-align: left; }
th { background-color: #3a4a52; }

.panel { display: flex; flex-direction: column; }
.button-group { display: flex; gap: 8px; }
.container { display: flex; flex-direction: column; }
)css";

void SetBuildError(UIBuildError& error, std::string message) {
	error.message = std::move(message);
}

void ApplyIdentifier(Rml::Element& native_element, const HTMLElement& element) {
	if (!element.GetId().empty()) {
		native_element.SetId(element.GetId());
	}
}

void ApplyClasses(Rml::Element& native_element, const HTMLElement& element) {
	for (const auto& css_class : element.GetClasses()) {
		native_element.SetClass(css_class, true);
	}
}

void ApplyInlineStyle(Rml::Element& native_element, const HTMLElement& element) {
	const auto& style = element.GetStyle();
	if (!style.empty()) {
		native_element.SetAttribute("style", style);
	}
}

bool BuildElement(const HTMLElement& html_element, Rml::Element& parent_native, 
	std::unordered_map<std::string, Rml::Element*>& elements_by_id, UIBuildError& error)
{
	if (html_element.GetKind() == HTMLElementKind::Text) {
		const auto& text = dynamic_cast<const HTMLText&>(html_element);
		auto text_node = parent_native.AppendChild(
			parent_native.GetOwnerDocument()->CreateTextNode(text.GetText())
		);
		if (!text_node) {
			SetBuildError(error, "Failed to create text node");
			return false;
		}
		return true;
	}

	const std::string& tag_name = html_element.GetTagName();
	Rml::Element* native_element = nullptr;

	if (tag_name == "button") {
		native_element = parent_native.AppendChild(
			parent_native.GetOwnerDocument()->CreateElement("button")
		);
	} else if (tag_name == "input") {
		native_element = parent_native.AppendChild(
			parent_native.GetOwnerDocument()->CreateElement("input")
		);
	} else if (tag_name == "select" || tag_name == "textarea") {
		native_element = parent_native.AppendChild(
			parent_native.GetOwnerDocument()->CreateElement(tag_name)
		);
	} else {
		native_element = parent_native.AppendChild(
			parent_native.GetOwnerDocument()->CreateElement("div")
		);
	}

	if (!native_element) {
		SetBuildError(error, "Failed to create element: " + tag_name);
		return false;
	}

	ApplyIdentifier(*native_element, html_element);
	ApplyClasses(*native_element, html_element);
	ApplyInlineStyle(*native_element, html_element);

	if (!html_element.GetId().empty()) {
		elements_by_id[html_element.GetId()] = native_element;
	}

	if (const auto* container = dynamic_cast<const HTMLContainer*>(&html_element)) {
		for (const auto& child : container->GetChildren()) {
			if (!BuildElement(*child, *native_element, elements_by_id, error)) {
				return false;
			}
		}
	}

	return true;
}

}

RmlUiTree::RmlUiTree() = default;

RmlUiTree::~RmlUiTree() {
	Close();
}

bool RmlUiTree::Build(Rml::Context& context, const HTMLDocument& document,
	std::unique_ptr<RmlUiTree>& tree, UIBuildError& error)
{
	auto new_tree = std::unique_ptr<RmlUiTree>(new RmlUiTree());

	new_tree->document_ = context.CreateDocument();
	if (!new_tree->document_) {
		SetBuildError(error, "Failed to create RmlUi document");
		return false;
	}

	auto default_stylesheet_container = std::make_shared<Rml::StyleSheetContainer>();
	Rml::StreamMemory default_stream(
		reinterpret_cast<const Rml::byte*>(DEFAULT_STYLE_SHEET),
		std::strlen(DEFAULT_STYLE_SHEET)
	);
	if (default_stylesheet_container->LoadStyleSheetContainer(&default_stream)) {
		new_tree->document_->SetStyleSheetContainer(default_stylesheet_container);
	}

	for (const auto& css : document.GetStyleSheets()) {
		auto stylesheet_container = std::make_shared<Rml::StyleSheetContainer>();
		Rml::StreamMemory stream(
			reinterpret_cast<const Rml::byte*>(css.c_str()),
			css.size()
		);
		if (stylesheet_container->LoadStyleSheetContainer(&stream)) {
			new_tree->document_->SetStyleSheetContainer(stylesheet_container);
		}
	}

	for (const auto& child : document.GetChildren()) {
		if (!BuildElement(*child, *new_tree->document_, new_tree->elements_by_id_, error)) {
			new_tree->Close();
			return false;
		}
	}

	tree = std::move(new_tree);
	return true;
}

Rml::ElementDocument* RmlUiTree::GetDocument() const noexcept {
	return document_;
}

Rml::Element* RmlUiTree::FindById(std::string_view id) const {
	auto it = elements_by_id_.find(std::string(id));
	if (it != elements_by_id_.end()) {
		return it->second;
	}
	return nullptr;
}

void RmlUiTree::Show() {
	if (document_) {
		document_->Show();
	}
}

void RmlUiTree::Close() {
	if (document_) {
		document_->Close();
		document_ = nullptr;
	}
	elements_by_id_.clear();
}

}
