#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace BlackLight::UI
{

/// Identifies the supported node types in a parsed HTML UI tree.
enum class HTMLElementKind
{
	Document,    // Root HTML document
	Container,   // div, section, etc.
	Button,      // button
	Input,       // input
	Select,      // select
	TextArea,    // textarea
	Label,       // label
	Link,        // a
	Image,       // img
	List,        // ul, ol
	ListItem,    // li
	Table,       // table
	TableRow,    // tr
	TableCell,   // td, th
	Text,        // Text content
	Custom       // Unknown/custom element
};

/// Describes the HTML element's attributes and properties.
struct HTMLElementAttributes
{
	std::string id;                              // Element ID
	std::vector<std::string> classes;            // CSS classes
	std::string style;                           // Inline CSS
	std::string tag_name;                        // Original HTML tag name
	std::unordered_map<std::string, std::string> custom_attributes;  // Other attributes
};

class HTMLContainer;

/// Represents one node in the HTML UI tree.
class HTMLElement
{
public:
	/// Destroys this node and its owned state.
	virtual ~HTMLElement();

	/// Returns the concrete node kind.
	HTMLElementKind GetKind() const noexcept;

	/// Returns the element ID.
	const std::string& GetId() const noexcept;

	/// Returns all CSS classes applied to this element.
	const std::vector<std::string>& GetClasses() const noexcept;

	/// Returns inline style attribute.
	const std::string& GetStyle() const noexcept;

	/// Returns the original HTML tag name.
	const std::string& GetTagName() const noexcept;

	/// Returns the owning container, or nullptr for the root node.
	HTMLContainer* GetParent() noexcept;

	/// Returns the owning container without allowing mutation, or nullptr for the root node.
	const HTMLContainer* GetParent() const noexcept;

	/// Returns all attributes of this element.
	const HTMLElementAttributes& GetAttributes() const noexcept;

	/// Gets a custom attribute value, or empty string if not found.
	std::string GetAttribute(const std::string& name) const;

protected:
	/// Constructs a node with its kind and attributes.
	HTMLElement(HTMLElementKind kind, const HTMLElementAttributes& attributes);

private:
	friend class HTMLContainer;

	HTMLElementKind kind_;
	HTMLElementAttributes attributes_;
	HTMLContainer* parent_ = nullptr;
};

/// Represents a container element that owns child elements.
class HTMLContainer : public HTMLElement
{
public:
	/// Destroys this container and all of its child nodes.
	~HTMLContainer() override;

	/// Returns the children in document order.
	const std::vector<std::unique_ptr<HTMLElement>>& GetChildren() const noexcept;

	/// Takes ownership of a child unless it is already parented.
	bool AddChild(std::unique_ptr<HTMLElement> child);

	/// Finds a descendant element by ID.
	HTMLElement* FindById(const std::string& id);

	/// Finds all descendants with a specific class.
	std::vector<HTMLElement*> FindByClass(const std::string& class_name);

protected:
	/// Constructs a container with its kind and attributes.
	HTMLContainer(HTMLElementKind kind, const HTMLElementAttributes& attributes);

private:
	std::vector<std::unique_ptr<HTMLElement>> children_;
};

/// Represents the root HTML document, which maps to an RmlUi document.
class HTMLDocument final : public HTMLContainer
{
public:
	/// Constructs a document with optional attributes.
	explicit HTMLDocument(const HTMLElementAttributes& attributes = {});

	/// Adds a stylesheet to this document.
	void AddStyleSheet(std::string css);

	/// Returns all stylesheets in declaration order.
	const std::vector<std::string>& GetStyleSheets() const noexcept;

	/// Sets the document's charset.
	void SetCharset(std::string charset);

	/// Gets the document's charset.
	const std::string& GetCharset() const noexcept;

	/// Sets the document's language.
	void SetLanguage(std::string language);

	/// Gets the document's language.
	const std::string& GetLanguage() const noexcept;

private:
	std::vector<std::string> style_sheets_;
	std::string charset_ = "UTF-8";
	std::string language_ = "en";
};

/// Represents a text node containing raw text content.
class HTMLText final : public HTMLElement
{
public:
	/// Constructs a text node with content.
	explicit HTMLText(std::string text);

	/// Returns the text content of this node.
	const std::string& GetText() const noexcept;

	/// Sets the text content.
	void SetText(std::string text);

private:
	std::string text_;
};

} // namespace BlackLight::UI
