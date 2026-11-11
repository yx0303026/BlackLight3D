#include "engine/IGraphicsBackend.h"

namespace BlackLight::Engine
{
	/// Allows existing backends to opt into frame preparation separately from UI rendering.
	GraphicsResult IGraphicsBackend::BeginFrame()
	{
		return GraphicsResult::Success;
	}

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
