#include "editor/core/EditorTheme.h"

namespace BlackLight::Editor
{

	namespace
	{

		/// Replaces every occurrence of a token with its replacement.
		void ReplaceAll(std::string &text, std::string_view token, std::string_view replacement)
		{
			if (token.empty())
				return;
			std::size_t position = 0;
			while ((position = text.find(token, position)) != std::string::npos)
			{
				text.replace(position, token.size(), replacement);
				position += replacement.size();
			}
		}

	} // namespace

	ThemeRegistry::ThemeRegistry()
	{
		themes_.push_back({
			"dark",
			"Dark",
			"#1e1f22",
			"#2b2d30",
			"#1a1b1e",
			"#2b2d30",
			"#3c3f41",
			"#4e5157",
			"#dfe1e5",
			"#9da0a8",
			"#3574f0",
			"#4a86f7",
			"#2f65d4",
			"#393b40",
			"#2e436e",
			"#e0a040",
			"#e05252",
			"#59a869",
		});

		themes_.push_back({
			"light",
			"Light",
			"#eef0f3",
			"#ffffff",
			"#e4e7ec",
			"#ffffff",
			"#d3d7de",
			"#b7bdc8",
			"#1f2329",
			"#6b7280",
			"#2f6fed",
			"#4a83f2",
			"#2559c6",
			"#e6eaf1",
			"#cfe0ff",
			"#b7791f",
			"#c53030",
			"#2f855a",
		});

		themes_.push_back({
			"midnight",
			"Midnight",
			"#0b1220",
			"#131c2e",
			"#0a1020",
			"#182338",
			"#25324d",
			"#34466b",
			"#dbe4f5",
			"#8494b3",
			"#5aa9ff",
			"#7dbcff",
			"#3c86db",
			"#1c2940",
			"#1f3a63",
			"#ffc857",
			"#ff6b6b",
			"#4ade80",
		});
	}

	const EditorTheme *ThemeRegistry::Find(std::string_view id) const noexcept
	{
		for (const auto &theme : themes_)
		{
			if (theme.id == id)
				return &theme;
		}
		return nullptr;
	}

	const std::vector<EditorTheme> &ThemeRegistry::GetThemes() const noexcept { return themes_; }

	std::string ThemeRegistry::Apply(const EditorTheme &theme, std::string_view css)
	{
		std::string result(css);
		// Replace compound tokens before their prefixes are matched.
		ReplaceAll(result, "@surface-alt", theme.surface_alt);
		ReplaceAll(result, "@border-strong", theme.border_strong);
		ReplaceAll(result, "@text-muted", theme.text_muted);
		ReplaceAll(result, "@accent-hover", theme.accent_hover);
		ReplaceAll(result, "@accent-active", theme.accent_active);
		ReplaceAll(result, "@bg", theme.background);
		ReplaceAll(result, "@surface", theme.surface);
		ReplaceAll(result, "@elevated", theme.elevated);
		ReplaceAll(result, "@border", theme.border);
		ReplaceAll(result, "@text", theme.text);
		ReplaceAll(result, "@accent", theme.accent);
		ReplaceAll(result, "@hover", theme.hover);
		ReplaceAll(result, "@selection", theme.selection);
		ReplaceAll(result, "@warning", theme.warning);
		ReplaceAll(result, "@error", theme.error);
		ReplaceAll(result, "@success", theme.success);
		return result;
	}

} // namespace BlackLight::Editor
