#include "rmlui/BuiCustomElement.h"

#include <cctype>

#include <RmlUi/Core.h>

namespace BlackLight::UI
{

	/// Returns the process-wide custom element registry.
	BuiCustomElementRegistry& BuiCustomElementRegistry::GetInstance()
	{
		static BuiCustomElementRegistry instance;
		return instance;
	}

	/// Registers a factory under a normalized tag name without replacing existing entries.
	bool BuiCustomElementRegistry::RegisterElement(
		const std::string& tag_name,
		std::function<std::unique_ptr<BuiCustomElement>()> factory)
	{
		if (tag_name.empty() || !factory)
		{
			return false;
		}

		// Ensure lowercase tag name for case-insensitive matching
		std::string lower_tag = tag_name;
		for (char& c : lower_tag)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		// Check if already registered
		if (factories_.find(lower_tag) != factories_.end())
		{
			return false;
		}

		factories_[lower_tag] = std::move(factory);
		return true;
	}

	/// Removes the factory matching the case-insensitive tag name.
	void BuiCustomElementRegistry::UnregisterElement(const std::string& tag_name)
	{
		std::string lower_tag = tag_name;
		for (char& c : lower_tag)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		factories_.erase(lower_tag);
	}

	/// Reports whether a factory exists for the case-insensitive tag name.
	bool BuiCustomElementRegistry::IsCustomElement(const std::string& tag_name) const
	{
		std::string lower_tag = tag_name;
		for (char& c : lower_tag)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return factories_.find(lower_tag) != factories_.end();
	}

	/// Instantiates a registered renderer, or returns null for an unknown tag.
	std::unique_ptr<BuiCustomElement> BuiCustomElementRegistry::CreateElement(
		const std::string& tag_name) const
	{
		std::string lower_tag = tag_name;
		for (char& c : lower_tag)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		auto it = factories_.find(lower_tag);
		if (it != factories_.end())
		{
			return it->second();
		}
		return nullptr;
	}

	/// Removes all registered custom element factories.
	void BuiCustomElementRegistry::Clear()
	{
		factories_.clear();
	}

} // namespace BlackLight::UI
