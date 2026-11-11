#include "rmlui/UIXmlParser.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>

#include <tinyxml2.h>

namespace BlackLight::UI
{
	namespace
	{
		// ============== Constants ==============
		constexpr std::string_view CONTROL_ID_ATTR = "c:id";
		constexpr std::string_view ID_ATTR = "id";
		constexpr std::string_view XMLNS_ATTR = "xmlns";
		constexpr std::string_view XMLNS_PREFIX = "xmlns:";
		constexpr std::string_view AUTO_SIZE_ATTR = "auto_size";
		constexpr std::string_view WIDTH_ATTR = "width";
		constexpr std::string_view HEIGHT_ATTR = "height";

		constexpr std::string_view SCREEN_TAG = "Screen";
		constexpr std::string_view PANEL_TAG = "Panel";

		constexpr std::string_view BOOLEAN_TRUE = "true";
		constexpr std::string_view BOOLEAN_FALSE = "false";
		constexpr std::string_view DIMENSION_AUTO = "auto";
		constexpr std::string_view DIMENSION_PX = "px";
		constexpr std::string_view DIMENSION_PERCENT = "%";
		constexpr std::string_view DIMENSION_DP = "dp";
		constexpr std::string_view DIMENSION_EM = "em";

		// ============== Helper Functions ==============

		/// Trims XML attribute whitespace from both ends of a view.
		std::string_view Trim(std::string_view value)
		{
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
			{
				value.remove_prefix(1);
			}
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
			{
				value.remove_suffix(1);
			}
			return value;
		}

		/// Stores a parse failure and the source line when the XML node is known.
		void SetError(UIParseError &error, std::string message, const tinyxml2::XMLNode *node = nullptr)
		{
			error.message = std::move(message);
			error.line = node == nullptr ? 0 : static_cast<std::size_t>(node->GetLineNum());
		}

		/// Returns true when a text node contains only XML whitespace.
		bool IsWhitespaceOnly(std::string_view value)
		{
			return std::all_of(value.begin(), value.end(), [](unsigned char character)
							   { return std::isspace(character) != 0; });
		}

		// ============== Attribute Validators ==============

		/// Validates and normalizes a boolean attribute value (case-insensitive).
		bool ValidateBoolean(std::string_view value, bool &result)
		{
			std::string normalized(value);
			std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character)
						   { return static_cast<char>(std::tolower(character)); });
			if (normalized == BOOLEAN_TRUE)
			{
				result = true;
				return true;
			}
			if (normalized == BOOLEAN_FALSE)
			{
				result = false;
				return true;
			}
			return false;
		}

		/// Checks if a unit string is a valid RmlUi dimension unit.
		bool IsValidDimensionUnit(std::string_view unit)
		{
			return unit == DIMENSION_PX || unit == DIMENSION_PERCENT || unit == DIMENSION_DP || unit == DIMENSION_EM;
		}

		/// Validates and normalizes a non-negative dimension value for RmlUi.
		bool ValidateDimension(std::string_view value, std::string &normalized)
		{
			value = Trim(value);
			if (value == DIMENSION_AUTO)
			{
				normalized = std::string(DIMENSION_AUTO);
				return true;
			}
			if (value.empty())
			{
				return false;
			}

			double numeric_value = 0.0;
			const char *const begin = value.data();
			const char *const end = value.data() + value.size();
			const auto parsed = std::from_chars(begin, end, numeric_value, std::chars_format::fixed);
			if (parsed.ec != std::errc{} || parsed.ptr == begin || !std::isfinite(numeric_value) || numeric_value < 0.0)
			{
				return false;
			}

			std::string_view unit(parsed.ptr, static_cast<std::size_t>(end - parsed.ptr));
			if (unit.empty())
			{
				normalized.assign(value);
				normalized += DIMENSION_PX;
				return true;
			}
			if (!IsValidDimensionUnit(unit))
			{
				return false;
			}

			normalized.assign(value);
			return true;
		}

		/// Checks if an attribute name should be skipped during validation.
		bool IsReservedAttribute(std::string_view name)
		{
			return name == CONTROL_ID_ATTR || name == ID_ATTR || name == XMLNS_ATTR ||
				   (name.size() >= XMLNS_PREFIX.size() && name.substr(0, XMLNS_PREFIX.size()) == XMLNS_PREFIX);
		}

		// ============== Element Validators ==============

		/// Validates the element tag and returns true if it matches expected container type.
		bool ValidateElementTag(std::string_view tag, bool is_root, UIParseError &error, const tinyxml2::XMLElement *element)
		{
			if (is_root && tag != SCREEN_TAG)
			{
				SetError(error, "The root element must be Screen.", element);
				return false;
			}
			if (!is_root && tag != PANEL_TAG)
			{
				if (tag == SCREEN_TAG)
				{
					SetError(error, "Screen may only appear as the document root.", element);
				}
				else
				{
					SetError(error, std::string("Unsupported UI element: ") + std::string(tag), element);
				}
				return false;
			}
			return true;
		}

		/// Extracts and validates the control identifier from c:id or id attributes.
		bool ExtractIdentifier(const tinyxml2::XMLElement &element, std::string &id, UIParseError &error)
		{
			const tinyxml2::XMLAttribute *control_id = nullptr;
			const tinyxml2::XMLAttribute *plain_id = nullptr;
			for (const tinyxml2::XMLAttribute *attribute = element.FirstAttribute(); attribute != nullptr;
				 attribute = attribute->Next())
			{
				const std::string_view name(attribute->Name());
				if (name == CONTROL_ID_ATTR)
				{
					control_id = attribute;
				}
				else if (name == ID_ATTR)
				{
					plain_id = attribute;
				}
			}

			if (control_id != nullptr && plain_id != nullptr)
			{
				SetError(error, "Use either c:id or id, not both.", &element);
				return false;
			}
			const tinyxml2::XMLAttribute *selected = control_id != nullptr ? control_id : plain_id;
			if (selected == nullptr)
			{
				id.clear();
				return true;
			}

			const std::string_view value = Trim(selected->Value());
			if (value.empty() || std::any_of(value.begin(), value.end(), [](unsigned char character)
											 { return std::isspace(character) != 0; }))
			{
				SetError(error, "Control identifiers must be non-empty and contain no whitespace.", &element);
				return false;
			}
			id.assign(value);
			return true;
		}

		/// Validates and applies a single layout attribute to the element layout.
		bool ApplyLayoutAttribute(std::string_view name, std::string_view value, UIElementLayout &layout,
								  UIParseError &error, const tinyxml2::XMLElement *element)
		{
			if (name == AUTO_SIZE_ATTR)
			{
				if (!ValidateBoolean(Trim(value), layout.auto_size))
				{
					SetError(error, "auto_size must be true or false.", element);
					return false;
				}
				return true;
			}

			if (name == WIDTH_ATTR || name == HEIGHT_ATTR)
			{
				std::string dimension;
				if (!ValidateDimension(value, dimension))
				{
					SetError(error, std::string("Invalid ") + std::string(name) + " dimension: " + std::string(value), element);
					return false;
				}
				if (name == WIDTH_ATTR)
				{
					layout.width = std::move(dimension);
				}
				else
				{
					layout.height = std::move(dimension);
				}
				return true;
			}

			return false;
		}

		/// Parses layout attributes and returns the configured UIElementLayout.
		bool ParseLayoutAttributes(const tinyxml2::XMLElement &element, UIElementLayout &layout, UIParseError &error)
		{
			for (const tinyxml2::XMLAttribute *attribute = element.FirstAttribute(); attribute != nullptr;
				 attribute = attribute->Next())
			{
				const std::string_view name(attribute->Name());
				const std::string_view value(attribute->Value());

				if (IsReservedAttribute(name))
				{
					continue;
				}

				if (!ApplyLayoutAttribute(name, value, layout, error, &element))
				{
					SetError(error, std::string("Unsupported attribute: ") + std::string(name), &element);
					return false;
				}
			}
			return true;
		}

		/// Parses one Screen or Panel element and its ordered text and element children.
		bool ParseContainerElement(const tinyxml2::XMLElement &element, bool is_root,
								   std::unique_ptr<UIElement> &output, UIParseError &error, std::unordered_set<std::string> &identifiers)
		{
			const std::string_view tag(element.Name());
			if (!ValidateElementTag(tag, is_root, error, &element))
			{
				return false;
			}

			std::string id;
			if (!ExtractIdentifier(element, id, error))
			{
				return false;
			}
			if (!id.empty() && !identifiers.insert(id).second)
			{
				SetError(error, std::string("Control identifiers must be unique: ") + id, &element);
				return false;
			}

			UIElementLayout layout;
			if (!ParseLayoutAttributes(element, layout, error))
			{
				return false;
			}

			std::unique_ptr<UIContainer> container;
			if (is_root)
			{
				container = std::make_unique<UIScreen>(std::move(id), std::move(layout));
			}
			else
			{
				container = std::make_unique<UIPanel>(std::move(id), std::move(layout));
			}

			for (const tinyxml2::XMLNode *child = element.FirstChild(); child != nullptr; child = child->NextSibling())
			{
				if (const tinyxml2::XMLElement *child_element = child->ToElement())
				{
					std::unique_ptr<UIElement> parsed_child;
					if (!ParseContainerElement(*child_element, false, parsed_child, error, identifiers))
					{
						return false;
					}
					if (!container->AddChild(std::move(parsed_child)))
					{
						SetError(error, "Could not attach a parsed UI child.", child_element);
						return false;
					}
					continue;
				}

				if (const tinyxml2::XMLText *text = child->ToText())
				{
					const std::string_view value(text->Value());
					if (!IsWhitespaceOnly(value))
					{
						auto text_element = std::make_unique<UIText>(std::string(value));
						if (!container->AddChild(std::move(text_element)))
						{
							SetError(error, "Could not attach parsed text content.", text);
							return false;
						}
					}
				}
			}

			output = std::move(container);
			return true;
		}

	} // namespace

	/// Converts an XML document into a validated Screen tree.
	bool UIXmlParser::Parse(std::string_view xml, std::unique_ptr<UIScreen> &screen, UIParseError &error)
	{
		error = {};
		tinyxml2::XMLDocument document;
		const tinyxml2::XMLError parse_result = document.Parse(xml.data(), xml.size());
		if (parse_result != tinyxml2::XML_SUCCESS)
		{
			SetError(error, document.ErrorStr() == nullptr ? "Invalid XML." : document.ErrorStr());
			error.line = static_cast<std::size_t>(document.ErrorLineNum());
			return false;
		}

		const tinyxml2::XMLElement *root = document.RootElement();
		if (root == nullptr)
		{
			SetError(error, "XML must contain a Screen root element.");
			return false;
		}

		std::unordered_set<std::string> identifiers;
		std::unique_ptr<UIElement> parsed_root;
		if (!ParseContainerElement(*root, true, parsed_root, error, identifiers))
		{
			return false;
		}

		screen.reset(static_cast<UIScreen *>(parsed_root.release()));
		return true;
	}

} // namespace BlackLight::UI
