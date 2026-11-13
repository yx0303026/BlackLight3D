#pragma once

#include "Matrix.h"
#include "Vector.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace BlackLight::Math
{
	/// Represents an axis-aligned rectangle defined by its minimum and maximum corners.
	struct Rect
	{
		Vector2 minimum{};
		Vector2 maximum{};

		/// Builds a rectangle from its top-left corner and size.
		static constexpr Rect FromPositionSize(const Vector2 &position, const Vector2 &size) noexcept
		{
			return {position, {position.x + size.x, position.y + size.y}};
		}

		/// Builds a rectangle from its center and size.
		static constexpr Rect FromCenterSize(const Vector2 &center, const Vector2 &size) noexcept
		{
			const Vector2 half{size.x * 0.5f, size.y * 0.5f};
			return {center - half, center + half};
		}

		/// Returns the rectangle size.
		constexpr Vector2 GetSize() const noexcept
		{
			return maximum - minimum;
		}

		/// Returns the rectangle center.
		constexpr Vector2 GetCenter() const noexcept
		{
			return (minimum + maximum) * 0.5f;
		}

		/// Returns whether a point lies inside or on the rectangle boundary.
		constexpr bool Contains(const Vector2 &point) const noexcept
		{
			return point.x >= minimum.x && point.x <= maximum.x &&
				   point.y >= minimum.y && point.y <= maximum.y;
		}

		/// Returns whether two rectangles overlap with positive area.
		constexpr bool Intersects(const Rect &other) const noexcept
		{
			return minimum.x < other.maximum.x && maximum.x > other.minimum.x &&
				   minimum.y < other.maximum.y && maximum.y > other.minimum.y;
		}

		/// Returns a copy grown by a non-negative amount on every side.
		constexpr Rect Expanded(float amount) const noexcept
		{
			const Vector2 padding{amount, amount};
			return {minimum - padding, maximum + padding};
		}
	};

	/// Represents an axis-aligned three-dimensional bounding box.
	struct AABB
	{
		Vector3 minimum{};
		Vector3 maximum{};

		/// Builds a box from its center and size.
		static constexpr AABB FromCenterSize(const Vector3 &center, const Vector3 &size) noexcept
		{
			const Vector3 half{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
			return {center - half, center + half};
		}

		/// Returns the box size.
		constexpr Vector3 GetSize() const noexcept
		{
			return maximum - minimum;
		}

		/// Returns the box center.
		constexpr Vector3 GetCenter() const noexcept
		{
			return (minimum + maximum) * 0.5f;
		}

		/// Returns whether a point lies inside or on the box boundary.
		constexpr bool Contains(const Vector3 &point) const noexcept
		{
			return point.x >= minimum.x && point.x <= maximum.x &&
				   point.y >= minimum.y && point.y <= maximum.y &&
				   point.z >= minimum.z && point.z <= maximum.z;
		}

		/// Returns whether two boxes overlap with positive volume.
		constexpr bool Intersects(const AABB &other) const noexcept
		{
			return minimum.x < other.maximum.x && maximum.x > other.minimum.x &&
				   minimum.y < other.maximum.y && maximum.y > other.minimum.y &&
				   minimum.z < other.maximum.z && maximum.z > other.minimum.z;
		}

		/// Returns a copy grown by a non-negative amount on every side.
		constexpr AABB Expanded(float amount) const noexcept
		{
			const Vector3 padding{amount, amount, amount};
			return {minimum - padding, maximum + padding};
		}
	};

	/// Represents a sphere defined by its center and radius.
	struct Sphere
	{
		Vector3 center{};
		float radius = 0.0f;

		/// Returns whether a point lies inside or on the sphere surface.
		constexpr bool Contains(const Vector3 &point) const noexcept
		{
			return (point - center).LengthSquared() <= radius * radius;
		}

		/// Returns whether two spheres overlap or touch.
		constexpr bool Intersects(const Sphere &other) const noexcept
		{
			const float combined_radius = radius + other.radius;
			return (other.center - center).LengthSquared() <= combined_radius * combined_radius;
		}

		/// Returns whether the sphere overlaps or touches a box.
		constexpr bool Intersects(const AABB &bounds) const noexcept
		{
			const Vector3 closest{
				Clamp(center.x, bounds.minimum.x, bounds.maximum.x),
				Clamp(center.y, bounds.minimum.y, bounds.maximum.y),
				Clamp(center.z, bounds.minimum.z, bounds.maximum.z)};
			return (closest - center).LengthSquared() <= radius * radius;
		}
	};

	/// Represents a plane as a unit normal and a signed offset: ``dot(normal, point) + distance = 0``.
	struct Plane
	{
		Vector3 normal{0.0f, 1.0f, 0.0f};
		float distance = 0.0f;

		/// Builds a plane through a point with a normal of any non-zero length.
		static Plane FromPointNormal(const Vector3 &point, const Vector3 &normal) noexcept
		{
			const Vector3 unit_normal = normal.Normalized();
			if (unit_normal.LengthSquared() <= kEpsilon)
				return Plane{};
			return {unit_normal, -unit_normal.Dot(point)};
		}

		/// Builds a plane through three counter-clockwise points.
		static Plane FromPoints(const Vector3 &first, const Vector3 &second, const Vector3 &third) noexcept
		{
			const Vector3 unit_normal = (second - first).Cross(third - first).Normalized();
			if (unit_normal.LengthSquared() <= kEpsilon)
				return Plane{};
			return {unit_normal, -unit_normal.Dot(first)};
		}

		/// Builds a normalized plane from ``normal.x/y/z`` and the offset stored in ``w``.
		static Plane FromCoefficients(const Vector4 &coefficients) noexcept
		{
			Plane plane{{coefficients.x, coefficients.y, coefficients.z}, coefficients.w};
			return plane.Normalized();
		}

		/// Returns the signed distance of a point, positive on the normal side.
		constexpr float GetSignedDistance(const Vector3 &point) const noexcept
		{
			return normal.Dot(point) + distance;
		}

		/// Returns a copy with a unit normal, or the default plane when the normal is degenerate.
		Plane Normalized() const noexcept
		{
			const float length = normal.Length();
			if (length <= kEpsilon)
				return Plane{};
			const float inverse_length = 1.0f / length;
			return {normal * inverse_length, distance * inverse_length};
		}

		/// Projects a point onto the plane.
		constexpr Vector3 Project(const Vector3 &point) const noexcept
		{
			return point - normal * GetSignedDistance(point);
		}

		/// Returns whether the plane cuts through or touches a sphere.
		constexpr bool Intersects(const Sphere &sphere) const noexcept
		{
			return Abs(GetSignedDistance(sphere.center)) <= sphere.radius;
		}
	};

	/// Represents a ray with a normalized direction.
	struct Ray
	{
		Vector3 origin{};
		Vector3 direction{0.0f, 0.0f, -1.0f};

		/// Returns the point at a distance along the ray.
		constexpr Vector3 PointAt(float distance) const noexcept
		{
			return origin + direction * distance;
		}

		/// Finds the nearest forward intersection with a plane.
		bool Intersects(const Plane &plane, float *distance = nullptr) const noexcept
		{
			const float denominator = plane.normal.Dot(direction);
			if (Abs(denominator) <= kEpsilon)
				return false;
			const float result = -(plane.normal.Dot(origin) + plane.distance) / denominator;
			if (result < 0.0f)
				return false;
			if (distance)
				*distance = result;
			return true;
		}

		/// Finds the nearest forward intersection with a sphere surface.
		bool Intersects(const Sphere &sphere, float *distance = nullptr) const noexcept
		{
			const Vector3 offset = origin - sphere.center;
			const float half_projection = offset.Dot(direction);
			const float discriminant =
				half_projection * half_projection - (offset.LengthSquared() - sphere.radius * sphere.radius);
			if (discriminant < 0.0f)
				return false;
			const float root = std::sqrt(discriminant);
			float result = -half_projection - root;
			if (result < 0.0f)
				result = -half_projection + root;
			if (result < 0.0f)
				return false;
			if (distance)
				*distance = result;
			return true;
		}

		/// Finds the nearest forward intersection with a box using the slab test.
		bool Intersects(const AABB &bounds, float *distance = nullptr) const noexcept
		{
			float closest = 0.0f;
			float farthest = (std::numeric_limits<float>::max)();
			for (int axis = 0; axis < 3; ++axis)
			{
				const float origin_component = origin[axis];
				const float direction_component = direction[axis];
				if (Abs(direction_component) <= kEpsilon)
				{
					if (origin_component < bounds.minimum[axis] ||
						origin_component > bounds.maximum[axis])
						return false;
					continue;
				}
				const float inverse = 1.0f / direction_component;
				float near_distance = (bounds.minimum[axis] - origin_component) * inverse;
				float far_distance = (bounds.maximum[axis] - origin_component) * inverse;
				if (near_distance > far_distance)
				{
					const float swap = near_distance;
					near_distance = far_distance;
					far_distance = swap;
				}
				closest = near_distance > closest ? near_distance : closest;
				farthest = far_distance < farthest ? far_distance : farthest;
				if (closest > farthest)
					return false;
			}
			if (distance)
				*distance = closest;
			return true;
		}
	};

	/// Represents the six clipping planes of a view-projection volume.
	struct Frustum
	{
		/// Identifies a clipping plane by its index inside the plane array.
		enum class Side : std::size_t
		{
			Left = 0,
			Right,
			Bottom,
			Top,
			Near,
			Far
		};

		std::array<Plane, 6> planes{};

		/// Extracts world-space planes from a row-vector view-projection matrix using zero-to-one depth.
		static Frustum FromViewProjection(const Matrix4 &view_projection) noexcept
		{
			const Vector4 column0{
				view_projection.m[0][0], view_projection.m[1][0],
				view_projection.m[2][0], view_projection.m[3][0]};
			const Vector4 column1{
				view_projection.m[0][1], view_projection.m[1][1],
				view_projection.m[2][1], view_projection.m[3][1]};
			const Vector4 column2{
				view_projection.m[0][2], view_projection.m[1][2],
				view_projection.m[2][2], view_projection.m[3][2]};
			const Vector4 column3{
				view_projection.m[0][3], view_projection.m[1][3],
				view_projection.m[2][3], view_projection.m[3][3]};
			Frustum result;
			result.planes[static_cast<std::size_t>(Side::Left)] =
				Plane::FromCoefficients(column3 + column0);
			result.planes[static_cast<std::size_t>(Side::Right)] =
				Plane::FromCoefficients(column3 - column0);
			result.planes[static_cast<std::size_t>(Side::Bottom)] =
				Plane::FromCoefficients(column3 + column1);
			result.planes[static_cast<std::size_t>(Side::Top)] =
				Plane::FromCoefficients(column3 - column1);
			result.planes[static_cast<std::size_t>(Side::Near)] =
				Plane::FromCoefficients(column2);
			result.planes[static_cast<std::size_t>(Side::Far)] =
				Plane::FromCoefficients(column3 - column2);
			return result;
		}

		/// Returns the clipping plane for one side.
		constexpr const Plane &GetPlane(Side side) const noexcept
		{
			return planes[static_cast<std::size_t>(side)];
		}

		/// Returns whether a point lies inside or on the frustum boundary.
		constexpr bool Contains(const Vector3 &point) const noexcept
		{
			for (const Plane &plane : planes)
			{
				if (plane.GetSignedDistance(point) < 0.0f)
					return false;
			}
			return true;
		}

		/// Returns whether a sphere overlaps or touches the frustum.
		constexpr bool Intersects(const Sphere &sphere) const noexcept
		{
			for (const Plane &plane : planes)
			{
				if (plane.GetSignedDistance(sphere.center) < -sphere.radius)
					return false;
			}
			return true;
		}

		/// Returns whether a box overlaps or touches the frustum.
		constexpr bool Intersects(const AABB &bounds) const noexcept
		{
			for (const Plane &plane : planes)
			{
				const Vector3 positive{
					plane.normal.x >= 0.0f ? bounds.maximum.x : bounds.minimum.x,
					plane.normal.y >= 0.0f ? bounds.maximum.y : bounds.minimum.y,
					plane.normal.z >= 0.0f ? bounds.maximum.z : bounds.minimum.z};
				if (plane.GetSignedDistance(positive) < 0.0f)
					return false;
			}
			return true;
		}
	};
} // namespace BlackLight::Math
