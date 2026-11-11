#include "rmlui/BuiDocument.h"

namespace BlackLight::UI
{

	const std::string &BuiDocument::GetLayout() const noexcept
	{
		return layout_;
	}

	const std::string &BuiDocument::GetStyles() const noexcept
	{
		return styles_;
	}

	void BuiDocument::SetLayout(std::string layout)
	{
		layout_ = std::move(layout);
	}

	void BuiDocument::SetStyles(std::string styles)
	{
		styles_ = std::move(styles);
	}

} // namespace BlackLight::UI
