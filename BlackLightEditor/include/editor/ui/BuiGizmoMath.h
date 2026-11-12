#pragma once

#include <algorithm>
#include <cmath>

namespace BlackLight::Editor
{

	/// Describes the editable rectangle of one preview element.
	///
	/// All values are density-independent pixels relative to the element's offset
	/// parent, which is the coordinate space that `position: absolute` resolves in.
	/// Reporting that space directly means committed `left`/`top` values match what
	/// the author sees instead of accidentally being document coordinates.
	struct BuiGizmoGeometry
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;

		/// Returns whether both rectangles describe the same position and size.
		bool NearlyEquals(const BuiGizmoGeometry &other,
			float tolerance = 0.5f) const noexcept
		{
			return std::fabs(x - other.x) <= tolerance &&
				std::fabs(y - other.y) <= tolerance &&
				std::fabs(width - other.width) <= tolerance &&
				std::fabs(height - other.height) <= tolerance;
		}
	};

	/// Identifies which part of the selection rectangle a gesture grabbed.
	enum class BuiGizmoHandle : unsigned char
	{
		None,
		Move,
		NorthWest,
		North,
		NorthEast,
		East,
		SouthEast,
		South,
		SouthWest,
		West
	};

	/// Returns whether one coordinate lies within a border band.
	inline bool InGizmoBand(float position, float start, float end,
		float grab) noexcept
	{
		return position >= start - grab && position <= end + grab;
	}

	/// Resolves the handle under one local point.
	///
	/// Corners win over edges and edges win over the interior, so a point in a
	/// corner band always resizes both axes. `grab` is the half-width of the
	/// border band in the same units as the rectangle.
	inline BuiGizmoHandle HitTestGizmo(const BuiGizmoGeometry &geometry, float x,
		float y, float grab) noexcept
	{
		const float right = geometry.x + geometry.width;
		const float bottom = geometry.y + geometry.height;
		if (x < geometry.x - grab || y < geometry.y - grab ||
			x > right + grab || y > bottom + grab)
		{
			return BuiGizmoHandle::None;
		}

		const bool left = InGizmoBand(x, geometry.x, geometry.x, grab);
		const bool right_edge = InGizmoBand(x, right, right, grab);
		const bool top = InGizmoBand(y, geometry.y, geometry.y, grab);
		const bool bottom_edge = InGizmoBand(y, bottom, bottom, grab);

		if (top && left)
			return BuiGizmoHandle::NorthWest;
		if (top && right_edge)
			return BuiGizmoHandle::NorthEast;
		if (bottom_edge && left)
			return BuiGizmoHandle::SouthWest;
		if (bottom_edge && right_edge)
			return BuiGizmoHandle::SouthEast;
		if (top)
			return BuiGizmoHandle::North;
		if (bottom_edge)
			return BuiGizmoHandle::South;
		if (left)
			return BuiGizmoHandle::West;
		if (right_edge)
			return BuiGizmoHandle::East;
		return BuiGizmoHandle::Move;
	}

	/// Applies one drag delta while keeping the opposite edge anchored.
	///
	/// West and north edges move the origin and resize in the same step, so the
	/// rectangle never inverts; a minimum size of one unit is preserved.
	inline BuiGizmoGeometry ApplyGizmoDrag(const BuiGizmoGeometry &start,
		BuiGizmoHandle handle, float delta_x, float delta_y) noexcept
	{
		BuiGizmoGeometry result = start;
		if (handle == BuiGizmoHandle::None)
			return result;
		if (handle == BuiGizmoHandle::Move)
		{
			result.x += delta_x;
			result.y += delta_y;
			return result;
		}

		const bool west = handle == BuiGizmoHandle::West ||
			handle == BuiGizmoHandle::NorthWest || handle == BuiGizmoHandle::SouthWest;
		const bool east = handle == BuiGizmoHandle::East ||
			handle == BuiGizmoHandle::NorthEast || handle == BuiGizmoHandle::SouthEast;
		const bool north = handle == BuiGizmoHandle::North ||
			handle == BuiGizmoHandle::NorthWest || handle == BuiGizmoHandle::NorthEast;
		const bool south = handle == BuiGizmoHandle::South ||
			handle == BuiGizmoHandle::SouthWest || handle == BuiGizmoHandle::SouthEast;

		const float right = start.x + start.width;
		const float bottom = start.y + start.height;
		if (west)
		{
			result.x = (std::min)(start.x + delta_x, right - 1.0f);
			result.width = right - result.x;
		}
		else if (east)
		{
			result.width = (std::max)(1.0f, start.width + delta_x);
		}
		if (north)
		{
			result.y = (std::min)(start.y + delta_y, bottom - 1.0f);
			result.height = bottom - result.y;
		}
		else if (south)
		{
			result.height = (std::max)(1.0f, start.height + delta_y);
		}
		return result;
	}

} // namespace BlackLight::Editor
