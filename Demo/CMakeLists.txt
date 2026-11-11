add_executable(blacklight_demo
	src/main.cpp
)

if(MSVC)
	target_sources(blacklight_demo PRIVATE app.manifest)
endif()

set_target_properties(blacklight_demo PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

target_link_libraries(blacklight_demo
	PRIVATE BlackLight::EngineStatic
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

blacklight_embed_bui(blacklight_demo
	ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ui"
	NAMESPACE DemoUi
	FILES Main.bui
)
