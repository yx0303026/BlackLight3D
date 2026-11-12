#pragma once

#include <string_view>

namespace BlackLight::Editor::Icons
{

	/// Font family the editor host registers for the embedded Codicons font.
	inline constexpr const char *Family = "BlackLightIcons";

	/// One icon glyph with a plain-text fallback for hosts without the icon font.
	struct Glyph
	{
		const char *icon;
		const char *fallback;
	};

	// Codicons glyphs (microsoft/vscode-codicons 0.0.46-24), UTF-8 encoded. The font is
	// embedded from fonts/codicon.ttf; see fonts/README.md for provenance and licensing.
	inline constexpr Glyph Close{"\xEE\xA9\xB6", "\xC3\x97"};
	inline constexpr Glyph ChevronDown{"\xEE\xAA\xB4", "v"};
	inline constexpr Glyph ChevronRight{"\xEE\xAA\xB6", ">"};
	inline constexpr Glyph Folder{"\xEE\xAA\x83", ">"};
	inline constexpr Glyph FolderOpen{"\xEE\xAB\xB7", "v"};
	inline constexpr Glyph Layout{"\xEE\xAF\xAB", "UI"};
	inline constexpr Glyph FileCode{"\xEE\xAB\xA9", "C"};
	inline constexpr Glyph Project{"\xEE\xAC\xB0", "P"};
	inline constexpr Glyph Markdown{"\xEE\xAC\x9D", "M"};
	inline constexpr Glyph Json{"\xEE\xAC\x8F", "-"};
	inline constexpr Glyph Tools{"\xEE\xAD\xAD", "-"};
	inline constexpr Glyph FileText{"\xEE\xB1\x9E", "-"};
	inline constexpr Glyph FileMedia{"\xEE\xAB\xAA", "-"};
	inline constexpr Glyph FileZip{"\xEE\xAB\xAF", "-"};
	inline constexpr Glyph File{"\xEE\xA9\xBB", "-"};

	/// Returns the glyph for a file entry with its lower-case extension without the dot.
	inline const Glyph &ForFile(std::string_view file_name,
		std::string_view extension) noexcept
	{
		if (file_name == "CMakeLists.txt")
			return Tools;
		if (extension == "bui")
			return Layout;
		if (extension == "cpp" || extension == "c" || extension == "cc" ||
			extension == "cxx" || extension == "h" || extension == "hpp" ||
			extension == "inl")
		{
			return FileCode;
		}
		if (extension == "json")
			return Json;
		if (extension == "md" || extension == "markdown")
			return Markdown;
		if (extension == "cmake")
			return Tools;
		if (extension == "blacklight")
			return Project;
		if (extension == "txt" || extension == "log")
			return FileText;
		if (extension == "png" || extension == "jpg" || extension == "jpeg" ||
			extension == "bmp" || extension == "tga" || extension == "dds" ||
			extension == "hdr")
		{
			return FileMedia;
		}
		if (extension == "zip" || extension == "7z" || extension == "gz" ||
			extension == "tar")
		{
			return FileZip;
		}
		return File;
	}

} // namespace BlackLight::Editor::Icons
