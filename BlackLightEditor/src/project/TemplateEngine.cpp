#include "editor/project/TemplateEngine.h"

#include <utility>

namespace BlackLight::Editor
{

	void TemplateContext::Set(std::string key, std::string value)
	{
		variables_[std::move(key)] = std::move(value);
	}

	bool TemplateContext::Contains(std::string_view key) const
	{
		return variables_.find(key) != variables_.end();
	}

	const std::string *TemplateContext::Find(std::string_view key) const
	{
		const auto it = variables_.find(key);
		return it == variables_.end() ? nullptr : &it->second;
	}

	const std::map<std::string, std::string, std::less<>> &
	TemplateContext::GetVariables() const noexcept
	{
		return variables_;
	}

	std::string TemplateContext::Render(std::string_view text, std::string *error) const
	{
		if (error)
			error->clear();

		std::string result;
		result.reserve(text.size());
		std::size_t index = 0;
		while (index < text.size())
		{
			if (text[index] == '$' && index + 2 < text.size() &&
				text[index + 1] == '$' && text[index + 2] == '{')
			{
				result += "${";
				index += 3;
				continue;
			}

			if (text[index] == '$' && index + 1 < text.size() && text[index + 1] == '{')
			{
				const std::size_t close = text.find('}', index + 2);
				if (close == std::string_view::npos)
				{
					if (error)
						*error = "Unterminated template variable";
					return {};
				}
				const std::string_view key = text.substr(index + 2, close - index - 2);
				const std::string *value = Find(key);
				if (!value)
				{
					if (error)
						*error = "Unknown template variable: ${" + std::string(key) + "}";
					return {};
				}
				result += *value;
				index = close + 1;
				continue;
			}

			result.push_back(text[index]);
			++index;
		}
		return result;
	}

} // namespace BlackLight::Editor
