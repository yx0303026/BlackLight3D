#pragma once

#include "rmlui/HTMLElement.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Rml {
class Context;
class Element;
class ElementDocument;
}

namespace BlackLight::UI {

/// Carries a runtime error raised while creating native RmlUi elements.
struct UIBuildError {
	/// Describes the first document or element creation failure.
	std::string message;
};

/// Builds and owns the RmlUi document corresponding to a parsed HTML document tree.
class RmlUiTree final {
public:
	/// Closes the owned document; its RmlUi context must still be alive.
	~RmlUiTree();

	/// Builds a document from an HTML element tree and replaces tree only after the full tree is created.
	static bool Build(Rml::Context& context, const HTMLDocument& document,
		std::unique_ptr<RmlUiTree>& tree, UIBuildError& error);

	/// Returns the root RmlUi document, or nullptr after Close.
	Rml::ElementDocument* GetDocument() const noexcept;

	/// Finds a native RmlUi element by its c:id value.
	Rml::Element* FindById(std::string_view id) const;

	/// Shows the document if it has not been closed.
	void Show();

	/// Closes the document and invalidates all native element lookups.
	void Close();

	/// Prevents copying because a wrapper uniquely owns a context-bound document.
	RmlUiTree(const RmlUiTree&) = delete;
	/// Prevents copying because a wrapper uniquely owns a context-bound document.
	RmlUiTree& operator=(const RmlUiTree&) = delete;
	/// Prevents moving a wrapper while native document pointers are indexed.
	RmlUiTree(RmlUiTree&&) = delete;
	/// Prevents moving a wrapper while native document pointers are indexed.
	RmlUiTree& operator=(RmlUiTree&&) = delete;

private:
	/// Creates an empty wrapper before the document is populated.
	RmlUiTree();

	Rml::ElementDocument* document_ = nullptr;
	std::unordered_map<std::string, Rml::Element*> elements_by_id_;
};

} // namespace BlackLight::UI
