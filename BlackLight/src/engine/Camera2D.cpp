#include "engine/Camera2D.h"

#include <algorithm>

namespace BlackLight::Engine
{
	/// Updates the camera center.
	void Camera2D::SetPosition(Math::Vector2 position) noexcept { position_ = position; }
	/// Returns the current camera center.
	Math::Vector2 Camera2D::GetPosition() const noexcept { return position_; }
	/// Updates zoom while rejecting zero and negative scales.
	void Camera2D::SetZoom(float zoom) noexcept { zoom_ = (std::max)(0.001f, zoom); }
	/// Returns the current camera zoom.
	float Camera2D::GetZoom() const noexcept { return zoom_; }
	/// Sets the drawable viewport.
	void Camera2D::SetViewport(float x, float y, float width, float height) noexcept
	{
		viewport_ = {x, y, (std::max)(0.0f, width), (std::max)(0.0f, height), 0.0f, 1.0f};
	}
	/// Returns the current drawable viewport.
	Rendering::Viewport Camera2D::GetViewport() const noexcept { return viewport_; }
	/// Converts a world coordinate to a top-left-origin screen coordinate.
	Math::Vector2 Camera2D::WorldToScreen(Math::Vector2 world) const noexcept
	{
		return {viewport_.x + viewport_.width * 0.5f + (world.x - position_.x) * zoom_,
				viewport_.y + viewport_.height * 0.5f + (world.y - position_.y) * zoom_};
	}
	/// Converts a top-left-origin screen coordinate to world space.
	Math::Vector2 Camera2D::ScreenToWorld(Math::Vector2 screen) const noexcept
	{
		if (zoom_ <= 0.0f)
			return position_;
		return {position_.x + (screen.x - viewport_.x - viewport_.width * 0.5f) / zoom_,
				position_.y + (screen.y - viewport_.y - viewport_.height * 0.5f) / zoom_};
	}
	/// Builds a row-vector orthographic projection with screen Y increasing downward.
	Math::Matrix4 Camera2D::GetProjection() const noexcept
	{
		if (viewport_.width <= 0.0f || viewport_.height <= 0.0f)
			return Math::Matrix4::Identity();
		const float half_width = viewport_.width * 0.5f / zoom_;
		const float half_height = viewport_.height * 0.5f / zoom_;
		return Math::Matrix4::CreateOrthographicOffCenter(
			position_.x - half_width, position_.x + half_width,
			position_.y + half_height, position_.y - half_height, 0.0f, 1.0f);
	}
}
