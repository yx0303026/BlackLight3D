#pragma once

#include "Math.h"
#include "rendering_device/CommandList.h"

namespace BlackLight::Engine
{
	/// Defines a 2D camera with a pixel viewport and world-space zoom.
	class Camera2D final
	{
	public:
		/// Sets the camera center in world coordinates.
		void SetPosition(Math::Vector2 position) noexcept;
		/// Returns the camera center in world coordinates.
		Math::Vector2 GetPosition() const noexcept;
		/// Sets the world-to-screen scale, clamped above zero.
		void SetZoom(float zoom) noexcept;
		/// Returns the current world-to-screen scale.
		float GetZoom() const noexcept;
		/// Sets the viewport origin and dimensions in drawable pixels.
		void SetViewport(float x, float y, float width, float height) noexcept;
		/// Returns the viewport in drawable pixel coordinates.
		Rendering::Viewport GetViewport() const noexcept;
		/// Converts a world coordinate into a drawable pixel coordinate.
		Math::Vector2 WorldToScreen(Math::Vector2 world) const noexcept;
		/// Converts a drawable pixel coordinate into a world coordinate.
		Math::Vector2 ScreenToWorld(Math::Vector2 screen) const noexcept;
		/// Builds the row-major orthographic matrix used by the sprite shaders.
		Math::Matrix4 GetProjection() const noexcept;

	private:
		Math::Vector2 position_{};
		float zoom_ = 1.0f;
		Rendering::Viewport viewport_{};
	};
}
