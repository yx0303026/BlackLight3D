include_guard(GLOBAL)

# Embeds a named binary file into a target with an application-owned, declaration-only header.
function(blacklight_embed_binary target)
	cmake_parse_arguments(PARSE_ARGV 1 BINARY "" "ROOT;NAMESPACE;NAME;FILE" "")
	if(NOT TARGET "${target}" OR NOT BINARY_ROOT OR NOT BINARY_NAMESPACE
		OR NOT BINARY_NAME OR NOT BINARY_FILE
		OR BINARY_UNPARSED_ARGUMENTS OR BINARY_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "Usage: blacklight_embed_binary(target ROOT directory NAMESPACE identifier NAME identifier FILE file)")
	endif()
	if(NOT BINARY_NAMESPACE MATCHES "^[A-Za-z_][A-Za-z0-9_]*$"
		OR NOT BINARY_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
		message(FATAL_ERROR "The embedded binary namespace and name must be C++ identifiers.")
	endif()
	if(IS_ABSOLUTE "${BINARY_FILE}" OR BINARY_FILE MATCHES "(^|/)\\.\\.(/|$)"
		OR NOT BINARY_FILE MATCHES "^[A-Za-z0-9_./-]+$")
		message(FATAL_ERROR "Binary resource names must be relative paths using letters, digits, _, -, . or /.")
	endif()
	if(NOT EXISTS "${BINARY_ROOT}/${BINARY_FILE}")
		message(FATAL_ERROR "Binary resource does not exist: ${BINARY_ROOT}/${BINARY_FILE}")
	endif()

	get_filename_component(BINARY_ROOT "${BINARY_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	set(output_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}_binary")
	set(output_header "${output_directory}/${target}_binary.h")
	set(output_source "${output_directory}/${target}_binary.cpp")

	# The dependency list makes edits rebuild the embedded data without editing C++.
	add_custom_command(
		OUTPUT "${output_header}" "${output_source}"
		COMMAND "${CMAKE_COMMAND}"
			"-DBINARY_FILE=${BINARY_ROOT}/${BINARY_FILE}"
			"-DBINARY_NAME=${BINARY_NAME}"
			"-DBINARY_NAMESPACE=${BINARY_NAMESPACE}"
			"-DBINARY_HEADER=${output_header}"
			"-DBINARY_SOURCE=${output_source}"
			-P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateBinary.cmake"
		DEPENDS "${BINARY_ROOT}/${BINARY_FILE}"
			"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateBinary.cmake"
		COMMENT "Embedding ${BINARY_FILE} into ${target}"
		VERBATIM
	)
	target_sources("${target}" PRIVATE "${output_source}" "${output_header}")
	target_include_directories("${target}" PRIVATE "${output_directory}")
	target_compile_features("${target}" PRIVATE cxx_std_17)
endfunction()
