#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace BlackLight::Editor
{

	/// Describes the colors used by one editor skin.
	///
	/// The `.bui` base stylesheet contains `@token` placeholders; a theme replaces
	/// those placeholders when the document stylesheet is generated. Themes never
	/// carry layout or typography so they can be swapped without rebuilding the UI.
	struct EditorTheme
	{
		std::string id;
		std::string name;
		std::string background;
		std::string surface;
		std::string surface_alt;
		std::string elevated;
		std::string border;
		std::string border_strong;
		std::string text;
		std::string text_muted;
		std::string accent;
		std::string accent_hover;
		std::string accent_active;
		std::string hover;
		std::string selection;
		std::string warning;
		std::string error;
		std::string success;
	};

	/// Owns the built-in editor themes and resolves theme tokens in stylesheets.
	class ThemeRegistry final
	{
	public:
		/// Registers the built-in Dark, Light, and Midnight themes.
		ThemeRegistry();

		/// Returns a theme by identifier, or null when the identifier is unknown.
		const EditorTheme *Find(std::string_view id) const noexcept;
		/// Returns every registered theme in registration order.
		const std::vector<EditorTheme> &GetThemes() const noexcept;

		/// Replaces every `@token` in the supplied stylesheet with its theme color.
		static std::string Apply(const EditorTheme &theme, std::string_view css);

	private:
		std::vector<EditorTheme> themes_;
	};

} // namespace BlackLight::Editor
