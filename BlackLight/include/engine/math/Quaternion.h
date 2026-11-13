#pragma once

#include "Matrix.h"
#include "Vector.h"

#include <cmath>

namespace BlackLight::Math
{
	/// Represents a rotation as a unit quaternion in (x, y, z, w) order.
	struct Quaternion
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;

		/// Returns the identity rotation.
		static constexpr Quaternion Identity() noexcept { return {}; }

		/// Builds a rotation around an axis of any non-zero length.
		static Quaternion FromAxisAngle(const Vector3 &axis, float radians) noexcept
		{
			const Vector3 normalized = axis.Normalized();
			if (normalized.LengthSquared() <= kEpsilon)
				return Identity();
			const float half_angle = radians * 0.5f;
			const float sine = std::sin(half_angle);
			return {normalized.x * sine, normalized.y * sine, normalized.z * sine,
					std::cos(half_angle)};
		}

		/// Composes roll around Z, then pitch around X, then yaw around Y.
		static Quaternion FromYawPitchRoll(float yaw, float pitch, float roll) noexcept
		{
			return FromAxisAngle(Vector3::UnitZ(), roll) *
				   FromAxisAngle(Vector3::UnitX(), pitch) *
				   FromAxisAngle(Vector3::UnitY(), yaw);
		}

		/// Extracts a rotation from a row-vector rotation matrix.
		static Quaternion FromRotationMatrix(const Matrix3 &matrix) noexcept
		{
			const float trace = matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2];
			Quaternion result{};
			if (trace > 0.0f)
			{
				const float scale = std::sqrt(trace + 1.0f) * 2.0f;
				result.w = 0.25f * scale;
				result.x = (matrix.m[1][2] - matrix.m[2][1]) / scale;
				result.y = (matrix.m[2][0] - matrix.m[0][2]) / scale;
				result.z = (matrix.m[0][1] - matrix.m[1][0]) / scale;
			}
			else if (matrix.m[0][0] > matrix.m[1][1] && matrix.m[0][0] > matrix.m[2][2])
			{
				const float scale = std::sqrt(1.0f + matrix.m[0][0] - matrix.m[1][1] -
											  matrix.m[2][2]) *
									2.0f;
				result.w = (matrix.m[1][2] - matrix.m[2][1]) / scale;
				result.x = 0.25f * scale;
				result.y = (matrix.m[1][0] + matrix.m[0][1]) / scale;
				result.z = (matrix.m[2][0] + matrix.m[0][2]) / scale;
			}
			else if (matrix.m[1][1] > matrix.m[2][2])
			{
				const float scale = std::sqrt(1.0f + matrix.m[1][1] - matrix.m[0][0] -
											  matrix.m[2][2]) *
									2.0f;
				result.w = (matrix.m[2][0] - matrix.m[0][2]) / scale;
				result.x = (matrix.m[1][0] + matrix.m[0][1]) / scale;
				result.y = 0.25f * scale;
				result.z = (matrix.m[2][1] + matrix.m[1][2]) / scale;
			}
			else
			{
				const float scale = std::sqrt(1.0f + matrix.m[2][2] - matrix.m[0][0] -
											  matrix.m[1][1]) *
									2.0f;
				result.w = (matrix.m[0][1] - matrix.m[1][0]) / scale;
				result.x = (matrix.m[2][0] + matrix.m[0][2]) / scale;
				result.y = (matrix.m[2][1] + matrix.m[1][2]) / scale;
				result.z = 0.25f * scale;
			}
			return result.Normalized();
		}

		/// Extracts a rotation from the upper-left block of a row-vector rotation matrix.
		static Quaternion FromRotationMatrix(const Matrix4 &matrix) noexcept
		{
			return FromRotationMatrix(matrix.ToMatrix3());
		}

		/// Returns the squared quaternion length.
		constexpr float LengthSquared() const noexcept
		{
			return x * x + y * y + z * z + w * w;
		}

		/// Returns the quaternion length.
		float Length() const noexcept
		{
			return std::sqrt(LengthSquared());
		}

		/// Returns a unit-length copy, or the identity rotation when the length is degenerate.
		Quaternion Normalized() const noexcept
		{
			const float length = Length();
			if (length <= kEpsilon)
				return Identity();
			const float inverse_length = 1.0f / length;
			return {x * inverse_length, y * inverse_length, z * inverse_length,
					w * inverse_length};
		}

		/// Returns the conjugate rotation.
		constexpr Quaternion Conjugate() const noexcept
		{
			return {-x, -y, -z, w};
		}

		/// Returns the inverse rotation, or the identity rotation when the length is degenerate.
		Quaternion Inverted() const noexcept
		{
			const float length_squared = LengthSquared();
			if (length_squared <= kEpsilon)
				return Identity();
			const float inverse_length_squared = 1.0f / length_squared;
			return {-x * inverse_length_squared, -y * inverse_length_squared,
					-z * inverse_length_squared, w * inverse_length_squared};
		}

		/// Returns the dot product of two quaternions.
		constexpr float Dot(const Quaternion &other) const noexcept
		{
			return x * other.x + y * other.y + z * other.z + w * other.w;
		}

		/// Composes two rotations, applying the left operand first to match Matrix4 multiplication.
		constexpr Quaternion operator*(const Quaternion &other) const noexcept
		{
			return {
				other.w * x + w * other.x + other.y * z - other.z * y,
				other.w * y + w * other.y + other.z * x - other.x * z,
				other.w * z + w * other.z + other.x * y - other.y * x,
				other.w * w - other.x * x - other.y * y - other.z * z};
		}

		/// Rotates a vector with the quaternion treated as a unit rotation.
		constexpr Vector3 Rotate(const Vector3 &vector) const noexcept
		{
			const Vector3 axis{x, y, z};
			const Vector3 doubled_cross = axis.Cross(vector) * 2.0f;
			return vector + doubled_cross * w + axis.Cross(doubled_cross);
		}

		/// Interpolates two rotations along the shortest arc using spherical interpolation.
		static Quaternion Slerp(const Quaternion &from, const Quaternion &to, float factor) noexcept
		{
			float cosine = from.Dot(to);
			Quaternion target = to;
			if (cosine < 0.0f)
			{
				target = {-to.x, -to.y, -to.z, -to.w};
				cosine = -cosine;
			}
			if (cosine > 0.9995f)
			{
				return Quaternion{
					Lerp(from.x, target.x, factor),
					Lerp(from.y, target.y, factor),
					Lerp(from.z, target.z, factor),
					Lerp(from.w, target.w, factor)}
					.Normalized();
			}
			const float angle = std::acos(Clamp(cosine, -1.0f, 1.0f));
			const float sine = std::sin(angle);
			const float from_weight = std::sin((1.0f - factor) * angle) / sine;
			const float to_weight = std::sin(factor * angle) / sine;
			return {
				from.x * from_weight + target.x * to_weight,
				from.y * from_weight + target.y * to_weight,
				from.z * from_weight + target.z * to_weight,
				from.w * from_weight + target.w * to_weight};
		}

		/// Returns the upper-left rotation block of the equivalent row-vector matrix.
		constexpr Matrix3 ToMatrix3() const noexcept
		{
			const float xx = x * x;
			const float yy = y * y;
			const float zz = z * z;
			const float xy = x * y;
			const float xz = x * z;
			const float yz = y * z;
			const float wx = w * x;
			const float wy = w * y;
			const float wz = w * z;
			Matrix3 result = Matrix3::Identity();
			result.m[0][0] = 1.0f - 2.0f * (yy + zz);
			result.m[0][1] = 2.0f * (xy + wz);
			result.m[0][2] = 2.0f * (xz - wy);
			result.m[1][0] = 2.0f * (xy - wz);
			result.m[1][1] = 1.0f - 2.0f * (xx + zz);
			result.m[1][2] = 2.0f * (yz + wx);
			result.m[2][0] = 2.0f * (xz + wy);
			result.m[2][1] = 2.0f * (yz - wx);
			result.m[2][2] = 1.0f - 2.0f * (xx + yy);
			return result;
		}

		/// Returns the equivalent row-vector rotation matrix.
		constexpr Matrix4 ToMatrix4() const noexcept
		{
			const Matrix3 rotation = ToMatrix3();
			Matrix4 result = Matrix4::Identity();
			for (int row = 0; row < 3; ++row)
			{
				for (int column = 0; column < 3; ++column)
					result.m[row][column] = rotation.m[row][column];
			}
			return result;
		}

		/// Returns whether every component matches within a tolerance.
		constexpr bool ApproximatelyEquals(const Quaternion &other, float tolerance = kEpsilon) const noexcept
		{
			return Math::Approximately(x, other.x, tolerance) &&
				   Math::Approximately(y, other.y, tolerance) &&
				   Math::Approximately(z, other.z, tolerance) &&
				   Math::Approximately(w, other.w, tolerance);
		}
	};
} // namespace BlackLight::Math
