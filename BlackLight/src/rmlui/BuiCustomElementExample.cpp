/// Example demonstrating how to create and register custom UI elements
/// with BlackLight's BUI system.

#include "rmlui/BuiCustomElement.h"
#include <RmlUi/Core.h>

namespace BlackLight::UI::Examples
{

	/// Example custom button with default styling
	class CustomButton : public BuiCustomElement, public Rml::EventListener
	{
	public:
		/// Returns the tag handled by the custom button.
		std::string GetTagName() const override
		{
			return "custom-button";
		}

		/// Initializes the button's styling and default label.
		void OnElementCreate(Rml::Element* element, Rml::Context* context) override
		{
			// Apply default styles when element is created
			ApplyDefaultStyles(element);

			// Set default content if empty
			if (element->GetInnerRML().empty())
			{
				element->SetInnerRML("Button");
			}
		}

		/// Applies the button's default inline properties.
		void ApplyDefaultStyles(Rml::Element* element) override
		{
			// Set default inline styles
			element->SetProperty("display", "inline-block");
			element->SetProperty("padding", "10dp 20dp");
			element->SetProperty("background-color", "#4CAF50");
			element->SetProperty("color", "white");
			element->SetProperty("border-radius", "4dp");
			element->SetProperty("cursor", "pointer");
			element->SetProperty("text-align", "center");
		}

		/// Subscribes the button to hover events while attached.
		void OnElementAttach(Rml::Element* element) override
		{
			// Add hover effect listener when attached
			element->AddEventListener(Rml::EventId::Mouseover, this);
			element->AddEventListener(Rml::EventId::Mouseout, this);
		}

		/// Removes hover event subscriptions before detachment.
		void OnElementDetach(Rml::Element* element) override
		{
			// Remove listeners when detached
			element->RemoveEventListener(Rml::EventId::Mouseover, this);
			element->RemoveEventListener(Rml::EventId::Mouseout, this);
		}

		/// Updates the background color when the pointer enters or leaves.
		void ProcessEvent(Rml::Event& event) override
		{
			Rml::Element* element = event.GetTargetElement();

			if (event == Rml::EventId::Mouseover)
			{
				element->SetProperty("background-color", "#45a049");
			}
			else if (event == Rml::EventId::Mouseout)
			{
				element->SetProperty("background-color", "#4CAF50");
			}
		}
	};

	/// Example custom progress bar
	class CustomProgressBar : public BuiCustomElement
	{
	public:
		/// Returns the tag handled by the custom progress bar.
		std::string GetTagName() const override
		{
			return "custom-progress";
		}

		/// Creates the fill element and applies the initial progress value.
		void OnElementCreate(Rml::Element* element, Rml::Context* context) override
		{
			// Create inner structure: container with fill bar
			element->SetInnerRML("<div class=\"progress-fill\"></div>");
			ApplyDefaultStyles(element);

			// Get initial value from attribute
			Rml::Variant* value_attr = element->GetAttribute("value");
			if (value_attr)
			{
				float value = value_attr->Get<float>();
				UpdateProgress(element, value);
			}
		}

		/// Applies the default container and fill properties.
		void ApplyDefaultStyles(Rml::Element* element) override
		{
			// Container styles
			element->SetProperty("display", "block");
			element->SetProperty("width", "100%");
			element->SetProperty("height", "20dp");
			element->SetProperty("background-color", "#e0e0e0");
			element->SetProperty("border-radius", "10dp");
			element->SetProperty("overflow", "hidden");

			// Fill bar styles
			Rml::Element* fill = element->GetFirstChild();
			if (fill)
			{
				fill->SetProperty("height", "100%");
				fill->SetProperty("background-color", "#2196F3");
				fill->SetProperty("transition", "width 0.3s");
				fill->SetProperty("width", "0%");
			}
		}

		/// Clamps the percentage and updates the fill width.
		void UpdateProgress(Rml::Element* element, float value)
		{
			// Clamp value between 0 and 100
			value = (value < 0.0f) ? 0.0f : (value > 100.0f) ? 100.0f : value;

			Rml::Element* fill = element->GetFirstChild();
			if (fill)
			{
				fill->SetProperty(Rml::PropertyId::Width, Rml::Property(value, Rml::Unit::PERCENT));
			}
		}
	};

	/// Registration helper - call this at application startup
	void RegisterExampleCustomElements()
	{
		auto& registry = BuiCustomElementRegistry::GetInstance();

		// Register custom button
		registry.RegisterElement("custom-button", []() {
			return std::make_unique<CustomButton>();
		});

		// Register custom progress bar
		registry.RegisterElement("custom-progress", []() {
			return std::make_unique<CustomProgressBar>();
		});
	}

} // namespace BlackLight::UI::Examples

/* Usage example in .bui file:

<styles>
	custom-button.primary {
		background-color: #2196F3;
	}

	custom-button.danger {
		background-color: #f44336;
	}
</styles>

<layout>
	<div>
		<custom-button>Click Me</custom-button>
		<custom-button class="primary">Primary Button</custom-button>
		<custom-button class="danger">Delete</custom-button>

		<custom-progress value="75"></custom-progress>
	</div>
</layout>

*/
