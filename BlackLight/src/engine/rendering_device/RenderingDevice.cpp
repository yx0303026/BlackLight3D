#include "DeviceImplementation.h"
#include "engine/rendering_device/UiIntegration.h"

#include <sstream>
#include <utility>

namespace BlackLight::Engine::Rendering
{
	/// Creates a backend-specific diagnostic exception.
	RenderingException::RenderingException(RenderingError code, std::string backend, std::string message)
		: std::runtime_error(backend + ": " + message), code_(code), backend_(std::move(backend))
	{
	}

	/// Returns the normalized error category.
	RenderingError RenderingException::GetCode() const noexcept { return code_; }
	/// Returns the backend name.
	const std::string &RenderingException::GetBackend() const noexcept { return backend_; }
	/// Releases the polymorphic rendering device.
	RenderingDevice::~RenderingDevice() = default;
	/// Creates and initializes a backend device.
	std::unique_ptr<RenderingDevice> RenderingDevice::Create(const DeviceConfig &config)
	{
		std::unique_ptr<RenderingDevice> device;
		switch (config.backend)
		{
		case RenderingBackend::OpenGL:
			device = std::make_unique<Detail::OpenGLDevice>();
			break;
		case RenderingBackend::Direct3D11:
			device = std::make_unique<Detail::Direct3D11Device>();
			break;
		default:
			throw RenderingException(RenderingError::InvalidConfiguration, "unknown", "unsupported rendering backend");
		}
		return device;
	}

	/// Releases the polymorphic UI service.
	UiIntegration::~UiIntegration() = default;
}
