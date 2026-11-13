#pragma once

#include <functional>
#include <string>
#include <memory>
#include <unordered_map>

namespace Rml
{
	class Element;
	class Context;
}

namespace BlackLight::UI
{

	/// Base class for custom UI element renderers.
	/// Subclasses implement their own default styling and rendering logic.
	class BuiCustomElement
	{
	public:
		/// Destroys the custom renderer through its base interface.
		virtual ~BuiCustomElement() = default;

		/// Returns the tag name this renderer handles (e.g., "custom-button", "my-widget").
		virtual std::string GetTagName() const = 0;

		/// Called when the element is first created in the DOM.
		/// Subclasses should set up default styles, attributes, and structure here.
		/// @param element The RmlUi element being initialized
		/// @param context The RmlUi context containing the element
		virtual void OnElementCreate(Rml::Element* element, Rml::Context* context) = 0;

		/// Called when the element is attached to the document.
		/// @param element The RmlUi element being attached
		virtual void OnElementAttach(Rml::Element* element) {}

		/// Called when the element is detached from the document.
		/// @param element The RmlUi element being detached
		virtual void OnElementDetach(Rml::Element* element) {}

		/// Called before the element is destroyed.
		/// Clean up any resources associated with this element here.
		/// @param element The RmlUi element being destroyed
		virtual void OnElementDestroy(Rml::Element* element) {}

		/// Applies default inline styles to the element.
		/// Override this to provide custom default styling.
		/// @param element The element to style
		virtual void ApplyDefaultStyles(Rml::Element* element) {}
	};

	/// Registry for custom element types.
	/// Manages the lifecycle and instantiation of custom elements.
	class BuiCustomElementRegistry final
	{
	public:
		/// Gets the singleton instance of the registry.
		static BuiCustomElementRegistry& GetInstance();

		/// Registers a custom element factory.
		/// @param tag_name The tag name to register (must be lowercase)
		/// @param factory Function that creates instances of the custom element
		/// @return true if registration succeeded, false if tag_name was already registered
		bool RegisterElement(const std::string& tag_name,
							 std::function<std::unique_ptr<BuiCustomElement>()> factory);

		/// Unregisters a custom element type.
		/// @param tag_name The tag name to unregister
		void UnregisterElement(const std::string& tag_name);

		/// Checks if a tag name is registered as a custom element.
		bool IsCustomElement(const std::string& tag_name) const;

		/// Creates an instance of a registered custom element.
		/// @param tag_name The tag name of the element to create
		/// @return A new instance, or nullptr if tag_name is not registered
		std::unique_ptr<BuiCustomElement> CreateElement(const std::string& tag_name) const;

		/// Clears all registered custom elements.
		void Clear();

	private:
		/// Creates the process-wide registry on first use.
		BuiCustomElementRegistry() = default;
		std::unordered_map<std::string, std::function<std::unique_ptr<BuiCustomElement>()>> factories_;
	};

} // namespace BlackLight::UI
