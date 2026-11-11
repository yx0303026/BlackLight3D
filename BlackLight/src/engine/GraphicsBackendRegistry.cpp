#include "engine/IGraphicsBackend.h"
#include "engine/backends/WindowsOpenGLBackend.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

namespace BlackLight::Engine
{
	namespace
	{

		using BackendFactory = std::function<std::unique_ptr<IGraphicsBackend>()>;

		static std::unordered_map<std::string, BackendFactory> &GetRegistry()
		{
			static std::unordered_map<std::string, BackendFactory> registry;
			return registry;
		}

	} // namespace

	void RegisterGraphicsBackend(const std::string &name, BackendFactory factory)
	{
		GetRegistry()[name] = factory;
	}

	std::unique_ptr<IGraphicsBackend> CreateGraphicsBackend(const char *backend_name)
	{
		const auto &registry = GetRegistry();
		auto it = registry.find(backend_name);
		if (it == registry.end())
		{
			return nullptr;
		}
		return it->second();
	}

	// Register built-in backends
	namespace
	{
		struct BackendRegistration
		{
			BackendRegistration()
			{
				RegisterGraphicsBackend("windows-opengl", []()
										{ return std::make_unique<Windows::WindowsOpenGLBackend>(); });
			}
		};

		static BackendRegistration g_registration;
	} // namespace

} // namespace BlackLight::Engine
