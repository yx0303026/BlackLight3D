#include "engine/IGraphicsBackend.h"

namespace BlackLight::Engine
{

	void IGraphicsBackend::SetInputSink(InputState * /*input*/) noexcept {}

	uint32_t IGraphicsBackend::GetDrawableWidth() const noexcept
	{
		return 0;
	}

	uint32_t IGraphicsBackend::GetDrawableHeight() const noexcept
	{
		return 0;
	}

} // namespace BlackLight::Engine
