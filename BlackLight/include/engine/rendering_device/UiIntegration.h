#pragma once

#include "RenderingDevice.h"

namespace Rml
{
	class Context;
	class SystemInterface;
}

namespace BlackLight::Engine::Rendering
{
	/// Provides optional access to UI services without making them part of device ownership.
	class BLACKLIGHT_API UiIntegration
	{
	public:
		/// Releases the polymorphic UI adapter.
		virtual ~UiIntegration();
		/// Returns the UI context or null when UI support is disabled.
		virtual Rml::Context *GetContext() const noexcept = 0;
		/// Returns the system interface or null when UI support is disabled.
		virtual Rml::SystemInterface *GetSystemInterface() const noexcept = 0;
		/// Updates the UI state after native events are processed.
		virtual void Update() = 0;
		/// Renders the UI overlay into the active device frame.
		virtual void Render() = 0;
	};
}
