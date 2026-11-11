#pragma once

#include "RenderingDevice.h"
#include "RenderingResources.h"

namespace BlackLight::Engine::Rendering
{
	/// Documents the shader source contract accepted by the first device implementations.
	struct ShaderContract
	{
		/// Returns the source variant required by a selected backend.
		static const char *GetSource(const ShaderSource &source, RenderingBackend backend);
	};
}
