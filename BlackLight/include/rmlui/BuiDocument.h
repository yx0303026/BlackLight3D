#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace BlackLight::UI
{

	/// Carries a human-readable .bui file load error and its source line.
	struct BuiLoadError
	{
		/// Describes the first loading or parsing failure.
		std::string message;
		/// Contains the one-based source line when available, or zero otherwise.
		std::size_t line = 0;
	};

	/// Represents a loaded .bui file with RmlUi-native HTML and CSS.
	/// .bui files are XML with two sections: <styles> and <layout>.
	class BuiDocument final
	{
	public:
		/// Creates an empty document.
		BuiDocument() = default;

		/// Returns the RmlUi HTML/layout content.
		const std::string &GetLayout() const noexcept;

		/// Returns the RmlUi CSS stylesheet content.
		const std::string &GetStyles() const noexcept;

		/// Sets the layout (HTML) content.
		void SetLayout(std::string layout);

		/// Sets the styles (CSS) content.
		void SetStyles(std::string styles);

		/// Prevents copying to avoid ambiguous ownership.
		BuiDocument(const BuiDocument &) = delete;
		/// Prevents copying to avoid ambiguous ownership.
		BuiDocument &operator=(const BuiDocument &) = delete;

		/// Allows moving.
		BuiDocument(BuiDocument &&) = default;
		/// Allows moving.
		BuiDocument &operator=(BuiDocument &&) = default;

	private:
		std::string layout_;
		std::string styles_;
	};

} // namespace BlackLight::UI
