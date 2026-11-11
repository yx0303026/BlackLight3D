#pragma once

namespace BlackLight::Editor::Icons
{

	/// Identifies the icon font loaded by the editor host.
	inline constexpr const char *Family = "BlackLightIcons";

	/// Segoe Fluent Icons / Segoe MDL2 Assets glyph U+E8BB (ChromeClose).
	inline constexpr const char *Close = "\xEE\xA2\xBB";

	/// Arial-safe fallback used when no system icon font is available.
	inline constexpr const char *CloseFallback = "\xC3\x97";

} // namespace BlackLight::Editor::Icons
