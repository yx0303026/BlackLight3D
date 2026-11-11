#include "editor/core/EditorConfig.h"
#include "editor/ui/EditorApplication.h"
#include "engine/Logger.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{

	/// Locates the executable directory for logs and default workspace paths.
	std::filesystem::path GetExecutableDirectory()
	{
		std::wstring buffer(32768, L'\0');
		const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
			static_cast<DWORD>(buffer.size()));
		if (!length || length >= buffer.size())
			throw std::runtime_error("Failed to locate the editor executable");
		buffer.resize(length);
		return std::filesystem::path(buffer).parent_path();
	}

	/// Resolves the roaming editor configuration file.
	std::filesystem::path GetConfigPath(const std::filesystem::path &executable_directory)
	{
		wchar_t buffer[MAX_PATH]{};
		const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, MAX_PATH);
		if (length > 0 && length < MAX_PATH)
			return std::filesystem::path(buffer) / "BlackLight3D" / "editor.ini";
		return executable_directory / "editor.ini";
	}

	/// Builds a DPI-aware window configuration from the persisted editor settings.
	BlackLight::Engine::WindowConfig CreateWindowConfig(
		const BlackLight::Editor::EditorConfig &config)
	{
		BlackLight::Engine::WindowConfig window_config;
		const UINT dpi = GetDpiForSystem();
		const int width_dp = std::clamp(config.GetInt("window", "width", 1440), 800, 6000);
		const int height_dp = std::clamp(config.GetInt("window", "height", 900), 600, 4000);

		window_config.rendering.surface.title = "BlackLightEditor";
		window_config.rendering.surface.width = static_cast<std::uint32_t>(
			MulDiv(width_dp, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI));
		window_config.rendering.surface.height = static_cast<std::uint32_t>(
			MulDiv(height_dp, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI));
		window_config.rendering.backend = BlackLight::Engine::Rendering::RenderingBackend::OpenGL;
		window_config.rendering.enable_ui = true;
		return window_config;
	}

} // namespace

int main()
{
	try
	{
		using namespace BlackLight;

		const std::filesystem::path executable_directory = GetExecutableDirectory();
		Engine::Logger::Get().SetLogFile(executable_directory / "blacklight_editor.log");
		BL_LOG(Engine::LogLevel::Info, "Editor", "Starting BlackLightEditor");

		const std::filesystem::path config_path = GetConfigPath(executable_directory);
		Editor::EditorConfig config_probe;
		config_probe.Load(config_path);

		Editor::EditorApplication application(CreateWindowConfig(config_probe), config_path);
		return application.Run();
	}
	catch (const std::exception &exception)
	{
		BL_LOG(BlackLight::Engine::LogLevel::Error, "Editor",
			std::string("Unhandled exception: ") + exception.what());
		return 1;
	}
	catch (...)
	{
		BL_LOG(BlackLight::Engine::LogLevel::Error, "Editor", "Unknown error occurred");
		return 1;
	}
}
