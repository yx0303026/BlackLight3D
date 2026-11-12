# Generates embedded binary resource storage from the original file bytes.
get_filename_component(output_directory "${BINARY_HEADER}" DIRECTORY)
get_filename_component(header_name "${BINARY_HEADER}" NAME)
file(MAKE_DIRECTORY "${output_directory}")

file(READ "${BINARY_FILE}" resource_hex HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," resource_bytes "${resource_hex}")
# Limit generated line lengths, and retain one extra byte so empty input is valid C++.
string(REGEX REPLACE "(0x[0-9a-fA-F]+,0x[0-9a-fA-F]+,0x[0-9a-fA-F]+,0x[0-9a-fA-F]+,)" "\\1\n" resource_bytes "${resource_bytes}")

get_filename_component(binary_file_name "${BINARY_FILE}" NAME)

set(header "#pragma once\n\n#include <cstddef>\n\nnamespace ${BINARY_NAMESPACE}\n{\n\t/// Embedded bytes of ${binary_file_name}.\n\textern const unsigned char ${BINARY_NAME}[];\n\t/// Size in bytes of ${BINARY_NAME}, excluding the internal zero terminator.\n\textern const std::size_t ${BINARY_NAME}_size;\n}\n")
set(source "#include \"${header_name}\"\n\nnamespace ${BINARY_NAMESPACE}\n{\n\tconst unsigned char ${BINARY_NAME}[] = {\n${resource_bytes}0x00\n\t};\n\tconst std::size_t ${BINARY_NAME}_size = sizeof(${BINARY_NAME}) - 1;\n}\n")

file(WRITE "${BINARY_HEADER}" "${header}")
file(WRITE "${BINARY_SOURCE}" "${source}")
