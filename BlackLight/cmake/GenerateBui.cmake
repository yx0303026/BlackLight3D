# Generates application resource storage and a lookup implementation from the original bytes.
get_filename_component(output_directory "${BUI_HEADER}" DIRECTORY)
get_filename_component(header_name "${BUI_HEADER}" NAME)
file(MAKE_DIRECTORY "${output_directory}")

set(header "#pragma once\n\n#include <string_view>\n\nnamespace ${BUI_NAMESPACE}\n{\n\t/// Returns immutable embedded .bui bytes, or an empty view when the name is unknown.\n\tstd::string_view Find(std::string_view name) noexcept;\n}\n")
set(source "#include \"${header_name}\"\n\nnamespace\n{\n")
set(lookup "")
set(resource_index 0)
foreach(resource IN LISTS BUI_FILES)
	file(READ "${BUI_ROOT}/${resource}" resource_hex HEX)
	string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," resource_bytes "${resource_hex}")
	# Limit generated line lengths, and retain one extra byte so empty input is valid C++.
	string(REGEX REPLACE "(0x[0-9a-fA-F]+,0x[0-9a-fA-F]+,0x[0-9a-fA-F]+,0x[0-9a-fA-F]+,)" "\\1\n" resource_bytes "${resource_bytes}")
	string(APPEND source "\tconst unsigned char resource_${resource_index}[] = {\n${resource_bytes}0x00\n\t};\n")
	string(APPEND lookup "\t\tif (name == \"${resource}\")\n\t\t\treturn {reinterpret_cast<const char*>(resource_${resource_index}), sizeof(resource_${resource_index}) - 1};\n")
	math(EXPR resource_index "${resource_index} + 1")
endforeach()
string(APPEND source "}\n\nnamespace ${BUI_NAMESPACE}\n{\n\t/// Resolves a resource name without accessing or extracting files.\n\tstd::string_view Find(std::string_view name) noexcept\n\t{\n${lookup}\t\treturn {};\n\t}\n}\n")

file(WRITE "${BUI_HEADER}" "${header}")
file(WRITE "${BUI_SOURCE}" "${source}")
