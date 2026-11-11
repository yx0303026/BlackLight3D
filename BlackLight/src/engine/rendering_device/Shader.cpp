#include "engine/rendering_device/Shader.h"

#include <stdexcept>

namespace BlackLight::Engine::Rendering
{
	/// Selects the backend-specific shader variant or reports the missing variant.
	const char *ShaderContract::GetSource(const ShaderSource &source, RenderingBackend backend)
	{
		const char *selected = backend == RenderingBackend::OpenGL ? source.open_gl : source.direct3d11;
		if (!selected || !*selected)
			throw std::invalid_argument("Shader source does not contain a variant for the selected backend");
		return selected;
	}
}
