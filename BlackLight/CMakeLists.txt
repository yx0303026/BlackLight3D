include(FetchContent)

# Configures UI dependencies in a local static-build scope, even with an old shared-library cache.
function(blacklight_configure_ui_dependencies)
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(BUILD_SHARED_LIBS OFF)
	find_package(RmlUi 6.3 EXACT CONFIG QUIET)
	if(NOT TARGET RmlUi::Core)
		if(NOT BLACKLIGHT_FETCH_DEPENDENCIES)
			message(FATAL_ERROR "RmlUi 6.3 was not found and BLACKLIGHT_FETCH_DEPENDENCIES is OFF.")
		endif()

		if(NOT TARGET Freetype::Freetype)
			# Keep optional system compression/shaping DLLs out of the standalone runtime.
			set(FT_DISABLE_ZLIB ON)
			set(FT_DISABLE_BZIP2 ON)
			set(FT_DISABLE_PNG ON)
			set(FT_DISABLE_HARFBUZZ ON)
			set(FT_DISABLE_BROTLI ON)
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

	if(NOT TARGET RmlUi::Core)
		message(FATAL_ERROR "The RmlUi package did not provide the expected RmlUi::Core target.")
	endif()

	get_target_property(blacklight_rmlui_type RmlUi::Core TYPE)
	if(NOT blacklight_rmlui_type STREQUAL "STATIC_LIBRARY")
		message(FATAL_ERROR
			"BlackLight requires static RmlUi. Point RmlUi_DIR at a static RmlUi 6.3 package "
			"or remove the shared package from CMAKE_PREFIX_PATH to fetch the pinned sources.")
	endif()
	if(TARGET Freetype::Freetype)
		get_target_property(blacklight_freetype_type Freetype::Freetype TYPE)
		if(blacklight_freetype_type STREQUAL "SHARED_LIBRARY")
			message(FATAL_ERROR "BlackLight requires static FreeType to avoid a font-engine DLL dependency.")
		endif()
	endif()
	set(rmlui_SOURCE_DIR "${rmlui_SOURCE_DIR}" PARENT_SCOPE)
	set(freetype_SOURCE_DIR "${freetype_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

blacklight_configure_ui_dependencies()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BlackLightEmbedBui.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BlackLightEmbedBinary.cmake")

# Use the official adapters matching the pinned core, including for installed packages.
set(BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY "${rmlui_SOURCE_DIR}/Backends" CACHE PATH
	"Directory containing the RmlUi 6.3 Win32, GL2, and DX11 adapters")
if(NOT EXISTS "${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}/RmlUi_Renderer_GL2.cpp" OR
   NOT EXISTS "${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}/RmlUi_Renderer_DX11.cpp" OR
   NOT EXISTS "${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}/RmlUi_Platform_Win32.cpp")
	if(NOT BLACKLIGHT_FETCH_DEPENDENCIES)
		message(FATAL_ERROR "Set BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY to the RmlUi 6.3 Backends directory.")
	endif()
	FetchContent_Declare(
		blacklight_rmlui_backends
		GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
		GIT_TAG 6.3
		GIT_SHALLOW TRUE
		SOURCE_SUBDIR blacklight_adapters_only
	)
	FetchContent_MakeAvailable(blacklight_rmlui_backends)
	set(BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY "${blacklight_rmlui_backends_SOURCE_DIR}/Backends")
endif()

set(BLACKLIGHT_ENGINE_SOURCES
	src/rmlui/BuiDocument.cpp
	src/rmlui/BuiLoader.cpp
	src/rmlui/BuiTree.cpp
	src/engine/Camera2D.cpp
	src/engine/Content.cpp
	src/engine/Ecs.cpp
	src/engine/FrameStats.cpp
	src/engine/Game.cpp
	src/engine/Image.cpp
	src/engine/Window.cpp
	src/engine/Input.cpp
	src/engine/Logger.cpp
	src/engine/SpriteBatch.cpp
	src/engine/SpriteAtlas.cpp
	src/engine/TextRenderer.cpp
	src/engine/Time.cpp
	src/engine/rendering_device/RenderingDevice.cpp
	src/engine/rendering_device/RenderingResources.cpp
	src/engine/rendering_device/CommandList.cpp
	src/engine/rendering_device/Shader.cpp
	src/engine/rendering_device/Win32Support.cpp
	src/engine/rendering_device/opengl/OpenGLDevice.cpp
	src/engine/rendering_device/d3d11/Direct3D11Device.cpp
	"${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}/RmlUi_Renderer_GL2.cpp"
	"${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}/RmlUi_Renderer_DX11.cpp"
	"${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}/RmlUi_Platform_Win32.cpp"
)

add_library(blacklight_engine SHARED ${BLACKLIGHT_ENGINE_SOURCES}
	include/engine/Logger.h
	include/engine/collision/Capsule.h
	include/engine/collision
	include/engine/collision/Circle2D.h
	include/engine/collision/Poly.h)
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
	PRIVATE "${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}"
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)
target_include_directories(blacklight_engine_static
	PRIVATE "${BLACKLIGHT_RMLUI_BACKENDS_DIRECTORY}"
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(blacklight_engine
	PUBLIC RmlUi::Core
	PRIVATE opengl32 d3d11 dxgi d3dcompiler comctl32 windowscodecs ole32 gdi32
)
target_link_libraries(blacklight_engine_static
	PUBLIC RmlUi::Core
	PRIVATE opengl32 d3d11 dxgi d3dcompiler comctl32 windowscodecs ole32 gdi32
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
	add_executable(blacklight_engine_tests tests/LoggerTests.cpp)
	target_link_libraries(blacklight_engine_tests PRIVATE BlackLight::EngineStatic)
	add_test(NAME blacklight_engine_tests COMMAND blacklight_engine_tests)

	if(MSVC)
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
install(
	DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cmake/"
	DESTINATION cmake
	FILES_MATCHING PATTERN "*.cmake"
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" BLACKLIGHT_PACKAGE_CONFIGURATION)
if(BLACKLIGHT_PACKAGE_CONFIGURATION STREQUAL "")
	set(BLACKLIGHT_PACKAGE_CONFIGURATION "default")
endif()
set(BLACKLIGHT_PACKAGE_STAGE "${CMAKE_BINARY_DIR}/blacklight_engine_package")
set(BLACKLIGHT_PACKAGE_ARCHIVE
	"${CMAKE_BINARY_DIR}/blacklight_engine_${BLACKLIGHT_ARCH}_${BLACKLIGHT_PACKAGE_CONFIGURATION}.zip"
)

# Static archives remain separate linker inputs for SDK consumers.
get_target_property(blacklight_rmlui_includes RmlUi::Core INTERFACE_INCLUDE_DIRECTORIES)
find_path(BLACKLIGHT_RMLUI_INCLUDE_DIRECTORY RmlUi/Core.h
	HINTS "${rmlui_SOURCE_DIR}/Include" ${blacklight_rmlui_includes}
	REQUIRED
)
set(BLACKLIGHT_UI_PACKAGE_COMMANDS
	COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:RmlUi::Core>"
		"${BLACKLIGHT_PACKAGE_STAGE}/$<TARGET_FILE_NAME:RmlUi::Core>"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${BLACKLIGHT_RMLUI_INCLUDE_DIRECTORY}/RmlUi"
		"${BLACKLIGHT_PACKAGE_STAGE}/include/RmlUi"
	COMMAND "${CMAKE_COMMAND}" -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
		"${BLACKLIGHT_PACKAGE_STAGE}/cmake"
)
if(TARGET Freetype::Freetype)
	list(APPEND BLACKLIGHT_UI_PACKAGE_COMMANDS
		COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:Freetype::Freetype>"
			"${BLACKLIGHT_PACKAGE_STAGE}/$<TARGET_FILE_NAME:Freetype::Freetype>"
	)
endif()
if(EXISTS "${rmlui_SOURCE_DIR}/LICENSE.txt")
	list(APPEND BLACKLIGHT_UI_PACKAGE_COMMANDS
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${BLACKLIGHT_PACKAGE_STAGE}/licenses"
		COMMAND "${CMAKE_COMMAND}" -E copy "${rmlui_SOURCE_DIR}/LICENSE.txt"
			"${BLACKLIGHT_PACKAGE_STAGE}/licenses/RmlUi.txt"
	)
endif()
if(EXISTS "${freetype_SOURCE_DIR}/LICENSE.TXT")
	list(APPEND BLACKLIGHT_UI_PACKAGE_COMMANDS
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${BLACKLIGHT_PACKAGE_STAGE}/licenses/FreeType"
		COMMAND "${CMAKE_COMMAND}" -E copy "${freetype_SOURCE_DIR}/LICENSE.TXT"
			"${freetype_SOURCE_DIR}/docs/FTL.TXT"
			"${BLACKLIGHT_PACKAGE_STAGE}/licenses/FreeType"
	)
endif()

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
	${BLACKLIGHT_UI_PACKAGE_COMMANDS}
	COMMAND "${CMAKE_COMMAND}" -E chdir "${BLACKLIGHT_PACKAGE_STAGE}"
		"${CMAKE_COMMAND}" -E tar "cf"
		"${BLACKLIGHT_PACKAGE_ARCHIVE}" --format=zip .
	DEPENDS blacklight_engine blacklight_engine_static
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/BlackLightEmbedBui.cmake"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBui.cmake"
	COMMENT "Packaging BlackLight Engine SDK"
	VERBATIM
)
add_custom_target(blacklight_engine_package DEPENDS "${BLACKLIGHT_PACKAGE_ARCHIVE}")
