add_executable(blacklight_demo
	src/main.cpp
)

set_target_properties(blacklight_demo PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

target_link_libraries(blacklight_demo
	PRIVATE BlackLight::RmlUi
	PRIVATE opengl32
)

target_include_directories(blacklight_demo
	PRIVATE
		${CMAKE_SOURCE_DIR}/BlackLight/include
)

if(MSVC)
	target_compile_options(blacklight_demo PRIVATE /W4 /permissive-)
else()
	target_compile_options(blacklight_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy .bui UI files to build directory
add_custom_command(TARGET blacklight_demo POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/ui
	$<TARGET_FILE_DIR:blacklight_demo>/UI
	COMMENT "Copying .bui UI files to build directory"
)
