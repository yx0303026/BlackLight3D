#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace BlackLight::UI
{

	/// Identifies the supported node types in a parsed UI tree.
	enum class UIElementKind
	{
		Screen,
		Panel,
		Text
	};

	/// Describes the size behavior applied to a container's width and height.
	struct UIElementLayout
	{
		/// Sizes unspecified dimensions from child content when true; otherwise fills the parent.
		bool auto_size = false;
		/// Stores an optional width value normalized for RmlUi, such as "320px" or "50%".
		std::optional<std::string> width;
		/// Stores an optional height value normalized for RmlUi, such as "180px" or "50%".
		std::optional<std::string> height;
	};

	class UIContainer;

	/// Represents one node in the UI tree and exposes its identity and parent relationship.
	class UIElement
	{
	public:
		/// Destroys this node and its owned state.
		virtual ~UIElement();

		/// Returns the concrete node kind.
		UIElementKind GetKind() const noexcept;

		/// Returns the optional control identifier from the XML c:id attribute.
		const std::string &GetId() const noexcept;

		/// Returns the owning container, or nullptr for the root node.
		UIContainer *GetParent() noexcept;

		/// Returns the owning container without allowing mutation, or nullptr for the root node.
		const UIContainer *GetParent() const noexcept;

	protected:
		/// Constructs a node with its kind and optional identifier.
		UIElement(UIElementKind kind, std::string id);

	private:
		friend class UIContainer;

		UIElementKind kind_;
		std::string id_;
		UIContainer *parent_ = nullptr;
	};

	/// Represents a node that owns child elements and has container sizing rules.
	class UIContainer : public UIElement
	{
	public:
		/// Destroys this container and all of its child nodes.
		~UIContainer() override;

		/// Returns the sizing configuration for this container.
		const UIElementLayout &GetLayout() const noexcept;

		/// Returns the children in document order.
		const std::vector<std::unique_ptr<UIElement>> &GetChildren() const noexcept;

		/// Takes ownership of a child unless it is already parented or would introduce a cycle.
		bool AddChild(std::unique_ptr<UIElement> child);

	protected:
		/// Constructs a container with its kind, identifier, and layout.
		UIContainer(UIElementKind kind, std::string id, UIElementLayout layout);

	private:
		UIElementLayout layout_;
		std::vector<std::unique_ptr<UIElement>> children_;
	};

	/// Represents the root screen, which maps to an RmlUi document.
	class UIScreen final : public UIContainer
	{
	public:
		/// Constructs a screen with an optional identifier and sizing configuration.
		explicit UIScreen(std::string id = {}, UIElementLayout layout = {});
	};

	/// Represents a panel container that maps to an RmlUi flex element.
	class UIPanel final : public UIContainer
	{
	public:
		/// Constructs a panel with an optional identifier and sizing configuration.
		explicit UIPanel(std::string id = {}, UIElementLayout layout = {});
	};

	/// Represents text content occurring between XML container elements.
	class UIText final : public UIElement
	{
	public:
		/// Constructs a text node with its source text and optional identifier.
		explicit UIText(std::string text, std::string id = {});

		/// Returns the text content of this node.
		const std::string &GetText() const noexcept;

	private:
		std::string text_;
	};

} // namespace BlackLight::UI
