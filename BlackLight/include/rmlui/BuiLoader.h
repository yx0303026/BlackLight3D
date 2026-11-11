#pragma once

#include "rmlui/BuiDocument.h"

#include <memory>
#include <string_view>

namespace BlackLight::UI
{

	/// Loads and parses .bui files directly into RmlUi-native format.
	/// .bui files are lightweight XML containers with <styles> and <layout> sections
	/// containing RmlUi-compatible CSS and HTML respectively.
	class BuiLoader final
	{
	public:
		/// Loads a .bui file from the filesystem.
		///
		/// @param file_path Path to the .bui file
		/// @param document Output: the loaded document
		/// @param error Output: load error details if loading fails
		/// @return true if loading succeeded, false if an error occurred
		static bool LoadFromFile(std::string_view file_path,
								 std::unique_ptr<BuiDocument> &document, BuiLoadError &error);

		/// Parses .bui XML content directly.
		///
		/// @param bui_content The .bui XML content as a string
		/// @param document Output: the parsed document
		/// @param error Output: parse error details if parsing fails
		/// @return true if parsing succeeded, false if an error occurred
		static bool Parse(std::string_view bui_content,
						  std::unique_ptr<BuiDocument> &document, BuiLoadError &error);
	};

} // namespace BlackLight::UI
