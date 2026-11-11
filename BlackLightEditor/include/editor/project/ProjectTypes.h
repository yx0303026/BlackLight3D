#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace BlackLight::Editor
{

	/// Selects the scaffold generated for a new project.
	enum class ProjectKind : std::uint8_t
	{
		WindowsApp,
		BlackLightGame,
		BlockLightUIInject
	};

	/// Selects the graphics API requested by a generated project.
	enum class GraphicsApi : std::uint8_t
	{
		OpenGL,
		DirectX11,
		DirectX12
	};

	/// Returns the stable name of a project kind.
	const char *ToString(ProjectKind kind) noexcept;
	/// Returns the stable name of a graphics API.
	const char *ToString(GraphicsApi api) noexcept;
	/// Parses a project kind written by the project file format.
	bool ProjectKindFromString(std::string_view text, ProjectKind &kind) noexcept;
	/// Parses a graphics API written by the project file format.
	bool GraphicsApiFromString(std::string_view text, GraphicsApi &api) noexcept;

	/// Returns the engine backend used to compile the requested API.
	/// DirectX 12 currently falls back to Direct3D 11 until the engine exposes a DX12 device.
	const char *GetEngineBackend(GraphicsApi api) noexcept;
	/// Returns a human-readable note about the selected graphics API.
	std::string GetGraphicsApiNote(GraphicsApi api);

	/// Describes a project that the editor should create.
	struct ProjectOptions
	{
		std::string name;
		/// Parent directory that receives a directory named after the project.
		std::filesystem::path directory;
		std::string name_space;
		/// RFC 4122 style identifier; generated when empty.
		std::string id;
		ProjectKind kind = ProjectKind::WindowsApp;
		GraphicsApi graphics_api = GraphicsApi::OpenGL;
		/// Directory of the prebuilt engine SDK downloaded from the GitHub releases.
		std::filesystem::path engine_root;
		/// Release tag of that SDK, for example `v0.0.1`.
		std::string engine_version;
	};

	/// Returns whether the text can be used as a project name and directory name.
	bool IsValidProjectName(std::string_view name);
	/// Returns whether the text is a valid C++ namespace identifier.
	bool IsValidNamespace(std::string_view name_space);
	/// Builds a C++ namespace from a display name, for example `My Game` to `MyGame`.
	std::string DeriveNamespace(std::string_view name);
	/// Builds a lower-case CMake target name from a display name.
	std::string SanitizeTargetName(std::string_view name);
	/// Returns a new RFC 4122 version 4 identifier.
	std::string GenerateProjectId();

	/// Extension used by project metadata files.
	inline constexpr std::string_view ProjectFileExtension = ".blacklight";
	/// File name used inside a project directory.
	inline constexpr std::string_view ProjectFileName = "project.blacklight";
	/// Metadata format version written by this editor.
	inline constexpr int ProjectFormatVersion = 1;

} // namespace BlackLight::Editor
