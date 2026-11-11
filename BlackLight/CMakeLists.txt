include(FetchContent)

find_package(RmlUi 6.3 CONFIG QUIET)
if(NOT TARGET RmlUi::Core)
	if(NOT BLACKLIGHT_FETCH_DEPENDENCIES)
		message(FATAL_ERROR "RmlUi 6.3 was not found and BLACKLIGHT_FETCH_DEPENDENCIES is OFF.")
	endif()

	find_package(Freetype QUIET)
	if(NOT TARGET Freetype::Freetype)
		FetchContent_Declare(
			freetype
			GIT_REPOSITORY https://github.com/freetype/freetype.git
			GIT_TAG VER-2-13-3
			GIT_SHALLOW TRUE
		)
		FetchContent_MakeAvailable(freetype)
		if(TARGET freetype AND NOT TARGET Freetype::Freetype)
			add_library(Freetype::Freetype ALIAS freetype)
		endif()
	endif()

	set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "RmlUi font engine" FORCE)
	set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
	set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
	set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
	set(RMLUI_TRACY_MEMORY_PROFILING OFF CACHE BOOL "" FORCE)
	if(NOT DEFINED BUILD_TESTING)
		set(BUILD_TESTING OFF CACHE BOOL "Build third-party tests")
	endif()

	FetchContent_Declare(
		rmlui
		GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
		GIT_TAG 6.3
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(rmlui)
endif()

find_package(tinyxml2 11 CONFIG QUIET)
if(NOT TARGET tinyxml2::tinyxml2)
	if(NOT BLACKLIGHT_FETCH_DEPENDENCIES)
		message(FATAL_ERROR "TinyXML2 11 was not found and BLACKLIGHT_FETCH_DEPENDENCIES is OFF.")
	endif()

	set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build TinyXML2 tests" FORCE)
	FetchContent_Declare(
		tinyxml2
		GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
		GIT_TAG 11.0.0
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(tinyxml2)
endif()

if(NOT TARGET RmlUi::Core)
	message(FATAL_ERROR "The RmlUi package did not provide the expected RmlUi::Core target.")
endif()

set(BLACKLIGHT_ENGINE_SOURCES
	src/rmlui/RmlUiTree.cpp
	src/rmlui/HTMLElement.cpp
	src/rmlui/HTMLParser.cpp
	src/rmlui/HtmlCssParser.cpp
	src/engine/Content.cpp
	src/engine/Ecs.cpp
	src/engine/Game.cpp
	src/engine/GraphicsBackendRegistry.cpp
	src/engine/GraphicsBackend.cpp
	src/engine/Input.cpp
	src/engine/Logger.cpp
	src/engine/SpriteBatch.cpp
	src/engine/Time.cpp
	src/engine/backends/WindowsOpenGLBackend.cpp
)

add_library(blacklight_engine SHARED ${BLACKLIGHT_ENGINE_SOURCES}
	include/engine/Logger.h)
add_library(blacklight_engine_static STATIC ${BLACKLIGHT_ENGINE_SOURCES})
add_library(BlackLight::Engine ALIAS blacklight_engine)
add_library(BlackLight::EngineStatic ALIAS blacklight_engine_static)
add_library(BlackLight::RmlUi ALIAS blacklight_engine_static)

set_target_properties(blacklight_engine blacklight_engine_static PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)
set_target_properties(blacklight_engine PROPERTIES
	OUTPUT_NAME "blacklight_engine_${BLACKLIGHT_ARCH}"
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)
set_target_properties(blacklight_engine_static PROPERTIES
	OUTPUT_NAME "blacklight_engine_${BLACKLIGHT_ARCH}_static"
)

target_include_directories(blacklight_engine
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)
target_include_directories(blacklight_engine_static
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(blacklight_engine
	PUBLIC RmlUi::Core
	PRIVATE tinyxml2::tinyxml2 opengl32
)
target_link_libraries(blacklight_engine_static
	PUBLIC RmlUi::Core
	PRIVATE tinyxml2::tinyxml2 opengl32
)
target_compile_definitions(blacklight_engine PRIVATE BLACKLIGHT_BUILD_SHARED)
target_compile_definitions(blacklight_engine_static PUBLIC BLACKLIGHT_STATIC)

if(MSVC)
	target_compile_options(blacklight_engine PRIVATE /W4 /EHsc /permissive-)
	target_compile_options(blacklight_engine_static PRIVATE /W4 /EHsc /permissive-)
else()
	target_compile_options(blacklight_engine PRIVATE -Wall -Wextra -Wpedantic)
	target_compile_options(blacklight_engine_static PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BLACKLIGHT_BUILD_TESTS)
	add_executable(blacklight_rmlui_tests tests/HTMLParserTests.cpp)
	target_link_libraries(blacklight_rmlui_tests PRIVATE BlackLight::EngineStatic)
	add_test(NAME blacklight_rmlui_tests COMMAND blacklight_rmlui_tests)

	add_executable(blacklight_htmlcss_tests tests/HtmlCssParserTests.cpp)
	target_link_libraries(blacklight_htmlcss_tests PRIVATE BlackLight::EngineStatic)
	add_test(NAME blacklight_htmlcss_tests COMMAND blacklight_htmlcss_tests)

	add_executable(blacklight_engine_tests tests/LoggerTests.cpp)
	target_link_libraries(blacklight_engine_tests PRIVATE BlackLight::EngineStatic)
	add_test(NAME blacklight_engine_tests COMMAND blacklight_engine_tests)

	if(MSVC)
		target_compile_options(blacklight_rmlui_tests PRIVATE /EHsc)
		target_compile_options(blacklight_htmlcss_tests PRIVATE /EHsc)
		target_compile_options(blacklight_engine_tests PRIVATE /EHsc)
	endif()
endif()

install(
	TARGETS blacklight_engine blacklight_engine_static
	RUNTIME DESTINATION .
	LIBRARY DESTINATION .
	ARCHIVE DESTINATION .
)
install(
	DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
	DESTINATION include
	FILES_MATCHING PATTERN "*.h"
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" BLACKLIGHT_PACKAGE_CONFIGURATION)
if(BLACKLIGHT_PACKAGE_CONFIGURATION STREQUAL "")
	set(BLACKLIGHT_PACKAGE_CONFIGURATION "default")
endif()
set(BLACKLIGHT_PACKAGE_STAGE "${CMAKE_BINARY_DIR}/blacklight_engine_package")
set(BLACKLIGHT_PACKAGE_ARCHIVE
	"${CMAKE_BINARY_DIR}/blacklight_engine_${BLACKLIGHT_ARCH}_${BLACKLIGHT_PACKAGE_CONFIGURATION}.zip"
)

add_custom_command(
	OUTPUT "${BLACKLIGHT_PACKAGE_ARCHIVE}"
	COMMAND "${CMAKE_COMMAND}" -E rm -rf "${BLACKLIGHT_PACKAGE_STAGE}"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${BLACKLIGHT_PACKAGE_STAGE}/include"
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:blacklight_engine>"
		"${BLACKLIGHT_PACKAGE_STAGE}/$<TARGET_FILE_NAME:blacklight_engine>"
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_LINKER_FILE:blacklight_engine>"
		"${BLACKLIGHT_PACKAGE_STAGE}/$<TARGET_LINKER_FILE_NAME:blacklight_engine>"
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:blacklight_engine_static>"
		"${BLACKLIGHT_PACKAGE_STAGE}/$<TARGET_FILE_NAME:blacklight_engine_static>"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${BLACKLIGHT_PACKAGE_STAGE}/include"
	COMMAND "${CMAKE_COMMAND}" -E chdir "${BLACKLIGHT_PACKAGE_STAGE}"
		"${CMAKE_COMMAND}" -E tar "cf"
		"${BLACKLIGHT_PACKAGE_ARCHIVE}" --format=zip .
	DEPENDS blacklight_engine blacklight_engine_static
	COMMENT "Packaging BlackLight Engine SDK"
	VERBATIM
)
add_custom_target(blacklight_engine_package DEPENDS "${BLACKLIGHT_PACKAGE_ARCHIVE}")
