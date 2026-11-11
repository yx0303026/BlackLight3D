add_library(blacklight_editor_core STATIC
	src/core/CommandHistory.cpp
	src/core/CommandRegistry.cpp
	src/core/Commands.cpp
	src/core/EditorConfig.cpp
	src/core/EditorEvent.cpp
	src/core/EditorState.cpp
	src/core/EditorTheme.cpp
	src/project/ProjectFile.cpp
	src/project/ProjectGenerator.cpp
	src/project/ProjectManager.cpp
	src/project/ProjectTemplates.cpp
	src/project/ProjectTypes.cpp
	src/project/ProjectValidator.cpp
	src/project/RecentProjects.cpp
	src/project/TemplateEngine.cpp
)

target_include_directories(blacklight_editor_core
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(blacklight_editor_core PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

add_executable(blacklight_editor
	src/ui/EditorApplication.cpp
	src/ui/EditorMenu.cpp
	src/ui/EditorUi.cpp
	src/ui/LogBuffer.cpp
	src/ui/PanelManager.cpp
	src/ui/ProjectWizard.cpp
	src/ui/Win32Dialogs.cpp
	src/ui/main.cpp
)

if(MSVC)
	target_sources(blacklight_editor PRIVATE app.manifest)
endif()

set_target_properties(blacklight_editor PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

target_link_libraries(blacklight_editor
	PRIVATE blacklight_editor_core
	PRIVATE BlackLight::EngineStatic
	PRIVATE opengl32
	PRIVATE comdlg32
	PRIVATE shell32
	PRIVATE ole32
)

target_include_directories(blacklight_editor
	PRIVATE ${CMAKE_SOURCE_DIR}/BlackLight/include
)

target_compile_definitions(blacklight_editor
	PRIVATE BLACKLIGHT_ENGINE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
)

if(MSVC)
	target_compile_options(blacklight_editor_core PRIVATE /W4 /permissive-)
	target_compile_options(blacklight_editor PRIVATE /W4 /EHsc /permissive-)
else()
	target_compile_options(blacklight_editor_core PRIVATE -Wall -Wextra -Wpedantic)
	target_compile_options(blacklight_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

blacklight_embed_bui(blacklight_editor
	ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ui"
	NAMESPACE EditorResources
	FILES Editor.bui
)

if(BLACKLIGHT_BUILD_TESTS)
	add_executable(blacklight_editor_tests tests/EditorCoreTests.cpp)
	target_link_libraries(blacklight_editor_tests PRIVATE blacklight_editor_core)
	set_target_properties(blacklight_editor_tests PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
	)
	if(MSVC)
		target_compile_options(blacklight_editor_tests PRIVATE /W4 /EHsc /permissive-)
	else()
		target_compile_options(blacklight_editor_tests PRIVATE -Wall -Wextra -Wpedantic)
	endif()
	add_test(NAME blacklight_editor_tests COMMAND blacklight_editor_tests)
endif()
