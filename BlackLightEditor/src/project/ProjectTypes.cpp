#include "editor/project/ProjectTypes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <random>

namespace BlackLight::Editor
{

	namespace
	{

		/// Lowercases an ASCII token.
		std::string ToLower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			return text;
		}

		/// Returns whether a character can appear in a project directory name.
		bool IsNameCharacter(char character)
		{
			const unsigned char value = static_cast<unsigned char>(character);
			if (std::isalnum(value) != 0)
				return true;
			return character == ' ' || character == '_' || character == '-';
		}

		/// Returns whether a name is a reserved Windows device name.
		bool IsReservedDeviceName(const std::string &lowered)
		{
			const std::size_t dot = lowered.find('.');
			const std::string base = dot == std::string::npos ? lowered : lowered.substr(0, dot);
			static const std::array<const char *, 22> reserved = {
				"con", "prn", "aux", "nul",
				"com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
				"lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
			return std::any_of(reserved.begin(), reserved.end(),
				[&base](const char *name) { return base == name; });
		}

		/// Formats one component of a generated identifier.
		void FormatHex(char *buffer, std::size_t size, unsigned long long value, int digits)
		{
			std::snprintf(buffer, size, "%0*llx", digits, value);
		}

	} // namespace

	const char *ToString(ProjectKind kind) noexcept
	{
		switch (kind)
		{
		case ProjectKind::WindowsApp: return "WindowsApp";
		case ProjectKind::BlackLightGame: return "BlackLightGame";
		case ProjectKind::BlockLightUIInject: return "BlockLightUIInject";
		}
		return "Unknown";
	}

	const char *ToString(GraphicsApi api) noexcept
	{
		switch (api)
		{
		case GraphicsApi::OpenGL: return "OpenGL";
		case GraphicsApi::DirectX11: return "DirectX11";
		case GraphicsApi::DirectX12: return "DirectX12";
		}
		return "Unknown";
	}

	bool ProjectKindFromString(std::string_view text, ProjectKind &kind) noexcept
	{
		const std::string lowered = ToLower(std::string(text));
		if (lowered == "windowsapp" || lowered == "windows-app" || lowered == "windows" ||
			lowered == "app")
		{
			kind = ProjectKind::WindowsApp;
			return true;
		}
		if (lowered == "blacklightgame" || lowered == "blacklight-game" || lowered == "game")
		{
			kind = ProjectKind::BlackLightGame;
			return true;
		}
		if (lowered == "blocklightuiinject" || lowered == "blocklight-ui-inject" ||
			lowered == "inject" || lowered == "dll")
		{
			kind = ProjectKind::BlockLightUIInject;
			return true;
		}
		return false;
	}

	bool GraphicsApiFromString(std::string_view text, GraphicsApi &api) noexcept
	{
		const std::string lowered = ToLower(std::string(text));
		if (lowered == "opengl" || lowered == "gl")
		{
			api = GraphicsApi::OpenGL;
			return true;
		}
		if (lowered == "directx11" || lowered == "dx11" || lowered == "d3d11")
		{
			api = GraphicsApi::DirectX11;
			return true;
		}
		if (lowered == "directx12" || lowered == "dx12" || lowered == "d3d12")
		{
			api = GraphicsApi::DirectX12;
			return true;
		}
		return false;
	}

	const char *GetEngineBackend(GraphicsApi api) noexcept
	{
		switch (api)
		{
		case GraphicsApi::OpenGL: return "OpenGL";
		case GraphicsApi::DirectX11:
		case GraphicsApi::DirectX12: return "Direct3D11";
		}
		return "OpenGL";
	}

	std::string GetGraphicsApiNote(GraphicsApi api)
	{
		switch (api)
		{
		case GraphicsApi::OpenGL:
			return "OpenGL is the reference backend and links against opengl32.";
		case GraphicsApi::DirectX11:
			return "Direct3D 11 uses the engine's Direct3D11 device and UI adapter.";
		case GraphicsApi::DirectX12:
			return "The engine does not expose a DirectX 12 device yet; this selection "
				"uses the Direct3D 11 backend and UI adapter.";
		}
		return {};
	}

	bool IsValidProjectName(std::string_view name)
	{
		if (name.empty() || name.size() > 64)
			return false;
		if (name == "." || name == "..")
			return false;

		const char first = name.front();
		if (std::isalnum(static_cast<unsigned char>(first)) == 0 && first != '_')
			return false;
		if (std::isdigit(static_cast<unsigned char>(first)) != 0)
			return false;
		if (name.back() == '.' || name.back() == ' ')
			return false;

		if (!std::all_of(name.begin(), name.end(), IsNameCharacter))
			return false;
		return !IsReservedDeviceName(ToLower(std::string(name)));
	}

	bool IsValidNamespace(std::string_view name_space)
	{
		if (name_space.empty())
			return false;
		const char first = name_space.front();
		if (std::isalpha(static_cast<unsigned char>(first)) == 0 && first != '_')
			return false;
		return std::all_of(name_space.begin(), name_space.end(), [](char character)
		{
			const unsigned char value = static_cast<unsigned char>(character);
			return std::isalnum(value) != 0 || character == '_';
		});
	}

	std::string DeriveNamespace(std::string_view name)
	{
		std::string result;
		result.reserve(name.size());
		bool start_segment = true;
		for (const char character : name)
		{
			const unsigned char value = static_cast<unsigned char>(character);
			if (std::isalnum(value) == 0)
			{
				start_segment = true;
				continue;
			}
			if (start_segment && std::isalpha(value) != 0)
				result.push_back(static_cast<char>(std::toupper(value)));
			else
				result.push_back(character);
			start_segment = false;
		}

		if (result.empty())
			return "Project";
		if (std::isdigit(static_cast<unsigned char>(result.front())) != 0)
			result.insert(result.begin(), 'P');
		if (!IsValidNamespace(result))
			return "Project";
		return result;
	}

	std::string SanitizeTargetName(std::string_view name)
	{
		std::string result;
		result.reserve(name.size() + 4);
		for (const char character : name)
		{
			const unsigned char value = static_cast<unsigned char>(character);
			if (std::isalnum(value) != 0)
				result.push_back(static_cast<char>(std::tolower(value)));
			else if (!result.empty() && result.back() != '_')
				result.push_back('_');
		}
		while (!result.empty() && result.back() == '_')
			result.pop_back();
		if (result.empty())
			result = "app";
		if (std::isdigit(static_cast<unsigned char>(result.front())) != 0)
			result.insert(result.begin(), '_');
		return result;
	}

	std::string GenerateProjectId()
	{
		std::random_device device;
		std::mt19937_64 engine((static_cast<std::uint64_t>(device()) << 32) ^ device());
		std::uniform_int_distribution<std::uint64_t> distribution;

		std::uint64_t high = distribution(engine);
		std::uint64_t low = distribution(engine);
		// Version 4 and RFC 4122 variant bits.
		high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
		low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

		char buffer[40]{};
		char high_text[17]{};
		char low_text[17]{};
		FormatHex(high_text, sizeof(high_text), high, 16);
		FormatHex(low_text, sizeof(low_text), low, 16);
		std::snprintf(buffer, sizeof(buffer), "{%c%c%c%c%c%c%c%c-%c%c%c%c-%c%c%c%c-%c%c%c%c-%c%c%c%c%c%c%c%c%c%c%c%c}",
			high_text[0], high_text[1], high_text[2], high_text[3],
			high_text[4], high_text[5], high_text[6], high_text[7],
			high_text[8], high_text[9], high_text[10], high_text[11],
			high_text[12], high_text[13], high_text[14], high_text[15],
			low_text[0], low_text[1], low_text[2], low_text[3],
			low_text[4], low_text[5], low_text[6], low_text[7],
			low_text[8], low_text[9], low_text[10], low_text[11],
			low_text[12], low_text[13], low_text[14], low_text[15]);
		return buffer;
	}

} // namespace BlackLight::Editor
