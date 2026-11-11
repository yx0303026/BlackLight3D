include_guard(GLOBAL)

# Embeds named .bui files into a target with an application-owned, declaration-only lookup header.
function(blacklight_embed_bui target)
	cmake_parse_arguments(PARSE_ARGV 1 BUI "" "ROOT;NAMESPACE" "FILES")
	if(NOT TARGET "${target}" OR NOT BUI_ROOT OR NOT BUI_FILES
		OR BUI_UNPARSED_ARGUMENTS OR BUI_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "Usage: blacklight_embed_bui(target ROOT directory [NAMESPACE identifier] FILES files...)")
	endif()
	if(NOT BUI_NAMESPACE)
		set(BUI_NAMESPACE "${target}_bui")
	endif()
	if(NOT BUI_NAMESPACE MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
		message(FATAL_ERROR "The embedded BUI namespace must be a C++ identifier.")
	endif()

	get_filename_component(BUI_ROOT "${BUI_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	set(output_directory "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}_bui")
	set(output_header "${output_directory}/${target}_bui.h")
	set(output_source "${output_directory}/${target}_bui.cpp")
	set(resource_dependencies)
	foreach(resource IN LISTS BUI_FILES)
		if(IS_ABSOLUTE "${resource}" OR resource MATCHES "(^|/)\\.\\.(/|$)"
			OR NOT resource MATCHES "^[A-Za-z0-9_./-]+\\.bui$")
			message(FATAL_ERROR "BUI resource names must be relative .bui paths using letters, digits, _, -, . or /.")
		endif()
		if(NOT EXISTS "${BUI_ROOT}/${resource}")
			message(FATAL_ERROR "BUI resource does not exist: ${BUI_ROOT}/${resource}")
		endif()
		list(APPEND resource_dependencies "${BUI_ROOT}/${resource}")
	endforeach()
	list(REMOVE_DUPLICATES BUI_FILES)

	# The dependency list makes edits rebuild the embedded data without editing C++.
	add_custom_command(
		OUTPUT "${output_header}" "${output_source}"
		COMMAND "${CMAKE_COMMAND}"
			"-DBUI_ROOT=${BUI_ROOT}"
			"-DBUI_FILES=${BUI_FILES}"
			"-DBUI_NAMESPACE=${BUI_NAMESPACE}"
			"-DBUI_HEADER=${output_header}"
			"-DBUI_SOURCE=${output_source}"
			-P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateBui.cmake"
		DEPENDS ${resource_dependencies}
			"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GenerateBui.cmake"
		COMMENT "Embedding .bui resources for ${target}"
		VERBATIM
	)
	target_sources("${target}" PRIVATE "${output_source}" "${output_header}")
	target_include_directories("${target}" PRIVATE "${output_directory}")
	target_compile_features("${target}" PRIVATE cxx_std_17)
endfunction()
