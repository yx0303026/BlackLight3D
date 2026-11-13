#pragma once

#include "Scalar.h"

#include <cmath>
#include <cstddef>

namespace BlackLight::Math
{
	/// Represents a two-dimensional floating-point vector.
	struct Vector2
	{
		float x = 0.0f;
		float y = 0.0f;

		/// Returns the zero vector.
		static constexpr Vector2 Zero() noexcept { return {}; }
		/// Returns a vector with every component set to one.
		static constexpr Vector2 One() noexcept { return {1.0f, 1.0f}; }
		/// Returns the positive X axis.
		static constexpr Vector2 UnitX() noexcept { return {1.0f, 0.0f}; }
		/// Returns the positive Y axis.
		static constexpr Vector2 UnitY() noexcept { return {0.0f, 1.0f}; }

		/// Adds another vector component by component.
		constexpr Vector2 operator+(const Vector2 &other) const noexcept
		{
			return {x + other.x, y + other.y};
		}

		/// Subtracts another vector component by component.
		constexpr Vector2 operator-(const Vector2 &other) const noexcept
		{
			return {x - other.x, y - other.y};
		}

		/// Negates both components.
		constexpr Vector2 operator-() const noexcept
		{
			return {-x, -y};
		}

		/// Multiplies both components by a scalar.
		constexpr Vector2 operator*(float scalar) const noexcept
		{
			return {x * scalar, y * scalar};
		}

		/// Divides both components by a scalar.
		constexpr Vector2 operator/(float scalar) const noexcept
		{
			return {x / scalar, y / scalar};
		}

		/// Adds another vector in place.
		constexpr Vector2 &operator+=(const Vector2 &other) noexcept
		{
			x += other.x;
			y += other.y;
			return *this;
		}

		/// Subtracts another vector in place.
		constexpr Vector2 &operator-=(const Vector2 &other) noexcept
		{
			x -= other.x;
			y -= other.y;
			return *this;
		}

		/// Scales both components in place.
		constexpr Vector2 &operator*=(float scalar) noexcept
		{
			x *= scalar;
			y *= scalar;
			return *this;
		}

		/// Divides both components in place.
		constexpr Vector2 &operator/=(float scalar) noexcept
		{
			x /= scalar;
			y /= scalar;
			return *this;
		}

		/// Accesses a component by axis index.
		constexpr float operator[](std::size_t index) const noexcept
		{
			return index == 0 ? x : y;
		}

		/// Returns the dot product of two vectors.
		constexpr float Dot(const Vector2 &other) const noexcept
		{
			return x * other.x + y * other.y;
		}

		/// Returns the Z component of the three-dimensional cross product.
		constexpr float Cross(const Vector2 &other) const noexcept
		{
			return x * other.y - y * other.x;
		}

		/// Returns the squared vector length without taking a square root.
		constexpr float LengthSquared() const noexcept
		{
			return x * x + y * y;
		}

		/// Returns the Euclidean vector length.
		float Length() const noexcept
		{
			return std::sqrt(LengthSquared());
		}

		/// Returns the squared distance to another vector.
		constexpr float DistanceSquared(const Vector2 &other) const noexcept
		{
			return (*this - other).LengthSquared();
		}

		/// Returns the distance to another vector.
		float Distance(const Vector2 &other) const noexcept
		{
			return std::sqrt(DistanceSquared(other));
		}

		/// Returns a unit-length copy, or the zero vector when the length is degenerate.
		Vector2 Normalized() const noexcept
		{
			const float length = Length();
			return length > kEpsilon ? Vector2{x / length, y / length} : Vector2{};
		}

		/// Returns the vector rotated ninety degrees counter-clockwise.
		constexpr Vector2 Perpendicular() const noexcept
		{
			return {-y, x};
		}

		/// Interpolates linearly toward another vector without clamping the factor.
		static constexpr Vector2 Lerp(const Vector2 &from, const Vector2 &to, float factor) noexcept
		{
			return {Math::Lerp(from.x, to.x, factor), Math::Lerp(from.y, to.y, factor)};
		}

		/// Returns whether both components match within a tolerance.
		constexpr bool ApproximatelyEquals(const Vector2 &other, float tolerance = kEpsilon) const noexcept
		{
			return Math::Approximately(x, other.x, tolerance) &&
				   Math::Approximately(y, other.y, tolerance);
		}
	};

	/// Multiplies a vector by a scalar.
	constexpr Vector2 operator*(float scalar, const Vector2 &value) noexcept
	{
		return value * scalar;
	}

	/// Represents a three-dimensional floating-point vector.
	struct Vector3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		/// Returns the zero vector.
		static constexpr Vector3 Zero() noexcept { return {}; }
		/// Returns a vector with every component set to one.
		static constexpr Vector3 One() noexcept { return {1.0f, 1.0f, 1.0f}; }
		/// Returns the positive X axis.
		static constexpr Vector3 UnitX() noexcept { return {1.0f, 0.0f, 0.0f}; }
		/// Returns the positive Y axis.
		static constexpr Vector3 UnitY() noexcept { return {0.0f, 1.0f, 0.0f}; }
		/// Returns the positive Z axis.
		static constexpr Vector3 UnitZ() noexcept { return {0.0f, 0.0f, 1.0f}; }

		/// Adds another vector component by component.
		constexpr Vector3 operator+(const Vector3 &other) const noexcept
		{
			return {x + other.x, y + other.y, z + other.z};
		}

		/// Subtracts another vector component by component.
		constexpr Vector3 operator-(const Vector3 &other) const noexcept
		{
			return {x - other.x, y - other.y, z - other.z};
		}

		/// Negates all components.
		constexpr Vector3 operator-() const noexcept
		{
			return {-x, -y, -z};
		}

		/// Multiplies all components by a scalar.
		constexpr Vector3 operator*(float scalar) const noexcept
		{
			return {x * scalar, y * scalar, z * scalar};
		}

		/// Divides all components by a scalar.
		constexpr Vector3 operator/(float scalar) const noexcept
		{
			return {x / scalar, y / scalar, z / scalar};
		}

		/// Adds another vector in place.
		constexpr Vector3 &operator+=(const Vector3 &other) noexcept
		{
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		/// Subtracts another vector in place.
		constexpr Vector3 &operator-=(const Vector3 &other) noexcept
		{
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		/// Scales all components in place.
		constexpr Vector3 &operator*=(float scalar) noexcept
		{
			x *= scalar;
			y *= scalar;
			z *= scalar;
			return *this;
		}

		/// Divides all components in place.
		constexpr Vector3 &operator/=(float scalar) noexcept
		{
			x /= scalar;
			y /= scalar;
			z /= scalar;
			return *this;
		}

		/// Accesses a component by axis index.
		constexpr float operator[](std::size_t index) const noexcept
		{
			return index == 0 ? x : (index == 1 ? y : z);
		}

		/// Returns the dot product of two vectors.
		constexpr float Dot(const Vector3 &other) const noexcept
		{
			return x * other.x + y * other.y + z * other.z;
		}

		/// Returns the cross product of two vectors.
		constexpr Vector3 Cross(const Vector3 &other) const noexcept
		{
			return {y * other.z - z * other.y,
					z * other.x - x * other.z,
					x * other.y - y * other.x};
		}

		/// Returns the squared vector length without taking a square root.
		constexpr float LengthSquared() const noexcept
		{
			return x * x + y * y + z * z;
		}

		/// Returns the Euclidean vector length.
		float Length() const noexcept
		{
			return std::sqrt(LengthSquared());
		}

		/// Returns the squared distance to another vector.
		constexpr float DistanceSquared(const Vector3 &other) const noexcept
		{
			return (*this - other).LengthSquared();
		}

		/// Returns the distance to another vector.
		float Distance(const Vector3 &other) const noexcept
		{
			return std::sqrt(DistanceSquared(other));
		}

		/// Returns a unit-length copy, or the zero vector when the length is degenerate.
		Vector3 Normalized() const noexcept
		{
			const float length = Length();
			return length > kEpsilon ? Vector3{x / length, y / length, z / length} : Vector3{};
		}

		/// Interpolates linearly toward another vector without clamping the factor.
		static constexpr Vector3 Lerp(const Vector3 &from, const Vector3 &to, float factor) noexcept
		{
			return {Math::Lerp(from.x, to.x, factor), Math::Lerp(from.y, to.y, factor),
					Math::Lerp(from.z, to.z, factor)};
		}

		/// Returns whether every component matches within a tolerance.
		constexpr bool ApproximatelyEquals(const Vector3 &other, float tolerance = kEpsilon) const noexcept
		{
			return Math::Approximately(x, other.x, tolerance) &&
				   Math::Approximately(y, other.y, tolerance) &&
				   Math::Approximately(z, other.z, tolerance);
		}
	};

	/// Multiplies a vector by a scalar.
	constexpr Vector3 operator*(float scalar, const Vector3 &value) noexcept
	{
		return value * scalar;
	}

	/// Represents a four-dimensional floating-point vector.
	struct Vector4
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;

		/// Returns the zero vector.
		static constexpr Vector4 Zero() noexcept { return {}; }
		/// Returns a vector with every component set to one.
		static constexpr Vector4 One() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }
		/// Returns the positive X axis.
		static constexpr Vector4 UnitX() noexcept { return {1.0f, 0.0f, 0.0f, 0.0f}; }
		/// Returns the positive Y axis.
		static constexpr Vector4 UnitY() noexcept { return {0.0f, 1.0f, 0.0f, 0.0f}; }
		/// Returns the positive Z axis.
		static constexpr Vector4 UnitZ() noexcept { return {0.0f, 0.0f, 1.0f, 0.0f}; }
		/// Returns the positive W axis.
		static constexpr Vector4 UnitW() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }

		/// Adds another vector component by component.
		constexpr Vector4 operator+(const Vector4 &other) const noexcept
		{
			return {x + other.x, y + other.y, z + other.z, w + other.w};
		}

		/// Subtracts another vector component by component.
		constexpr Vector4 operator-(const Vector4 &other) const noexcept
		{
			return {x - other.x, y - other.y, z - other.z, w - other.w};
		}

		/// Negates all components.
		constexpr Vector4 operator-() const noexcept
		{
			return {-x, -y, -z, -w};
		}

		/// Multiplies all components by a scalar.
		constexpr Vector4 operator*(float scalar) const noexcept
		{
			return {x * scalar, y * scalar, z * scalar, w * scalar};
		}

		/// Divides all components by a scalar.
		constexpr Vector4 operator/(float scalar) const noexcept
		{
			return {x / scalar, y / scalar, z / scalar, w / scalar};
		}

		/// Adds another vector in place.
		constexpr Vector4 &operator+=(const Vector4 &other) noexcept
		{
			x += other.x;
			y += other.y;
			z += other.z;
			w += other.w;
			return *this;
		}

		/// Subtracts another vector in place.
		constexpr Vector4 &operator-=(const Vector4 &other) noexcept
		{
			x -= other.x;
			y -= other.y;
			z -= other.z;
			w -= other.w;
			return *this;
		}

		/// Scales all components in place.
		constexpr Vector4 &operator*=(float scalar) noexcept
		{
			x *= scalar;
			y *= scalar;
			z *= scalar;
			w *= scalar;
			return *this;
		}

		/// Divides all components in place.
		constexpr Vector4 &operator/=(float scalar) noexcept
		{
			x /= scalar;
			y /= scalar;
			z /= scalar;
			w /= scalar;
			return *this;
		}

		/// Accesses a component by index.
		constexpr float operator[](std::size_t index) const noexcept
		{
			return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
		}

		/// Returns the dot product of two vectors.
		constexpr float Dot(const Vector4 &other) const noexcept
		{
			return x * other.x + y * other.y + z * other.z + w * other.w;
		}

		/// Returns the squared vector length without taking a square root.
		constexpr float LengthSquared() const noexcept
		{
			return x * x + y * y + z * z + w * w;
		}

		/// Returns the Euclidean vector length.
		float Length() const noexcept
		{
			return std::sqrt(LengthSquared());
		}

		/// Returns a unit-length copy, or the zero vector when the length is degenerate.
		Vector4 Normalized() const noexcept
		{
			const float length = Length();
			return length > kEpsilon ? Vector4{x / length, y / length, z / length, w / length}
									 : Vector4{};
		}

		/// Interpolates linearly toward another vector without clamping the factor.
		static constexpr Vector4 Lerp(const Vector4 &from, const Vector4 &to, float factor) noexcept
		{
			return {Math::Lerp(from.x, to.x, factor), Math::Lerp(from.y, to.y, factor),
					Math::Lerp(from.z, to.z, factor), Math::Lerp(from.w, to.w, factor)};
		}

		/// Returns whether every component matches within a tolerance.
		constexpr bool ApproximatelyEquals(const Vector4 &other, float tolerance = kEpsilon) const noexcept
		{
			return Math::Approximately(x, other.x, tolerance) &&
				   Math::Approximately(y, other.y, tolerance) &&
				   Math::Approximately(z, other.z, tolerance) &&
				   Math::Approximately(w, other.w, tolerance);
		}
	};

	/// Multiplies a vector by a scalar.
	constexpr Vector4 operator*(float scalar, const Vector4 &value) noexcept
	{
		return value * scalar;
	}
} // namespace BlackLight::Math
