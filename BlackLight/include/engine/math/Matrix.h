#pragma once

#include "Vector.h"

#include <cmath>

namespace BlackLight::Math
{
	/// Represents a row-major 3x3 matrix used for two-dimensional transforms.
	struct Matrix3
	{
		float m[3][3]{};

		/// Returns an identity matrix.
		static constexpr Matrix3 Identity() noexcept
		{
			return {{{1.0f, 0.0f, 0.0f},
					 {0.0f, 1.0f, 0.0f},
					 {0.0f, 0.0f, 1.0f}}};
		}

		/// Builds a two-dimensional translation matrix.
		static constexpr Matrix3 CreateTranslation(const Vector2 &translation) noexcept
		{
			Matrix3 result = Identity();
			result.m[2][0] = translation.x;
			result.m[2][1] = translation.y;
			return result;
		}

		/// Builds a two-dimensional scale matrix.
		static constexpr Matrix3 CreateScale(const Vector2 &scale) noexcept
		{
			Matrix3 result = Identity();
			result.m[0][0] = scale.x;
			result.m[1][1] = scale.y;
			return result;
		}

		/// Builds a counter-clockwise rotation matrix around Z.
		static Matrix3 CreateRotation(float radians) noexcept
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			Matrix3 result = Identity();
			result.m[0][0] = cosine;
			result.m[0][1] = sine;
			result.m[1][0] = -sine;
			result.m[1][1] = cosine;
			return result;
		}

		/// Multiplies two transformation matrices.
		constexpr Matrix3 operator*(const Matrix3 &other) const noexcept
		{
			Matrix3 result{};
			for (int row = 0; row < 3; ++row)
			{
				for (int column = 0; column < 3; ++column)
				{
					result.m[row][column] =
						m[row][0] * other.m[0][column] +
						m[row][1] * other.m[1][column] +
						m[row][2] * other.m[2][column];
				}
			}
			return result;
		}

		/// Returns the transposed matrix.
		constexpr Matrix3 Transposed() const noexcept
		{
			Matrix3 result{};
			for (int row = 0; row < 3; ++row)
			{
				for (int column = 0; column < 3; ++column)
					result.m[row][column] = m[column][row];
			}
			return result;
		}

		/// Returns the signed matrix determinant.
		constexpr float Determinant() const noexcept
		{
			return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
				   m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
				   m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
		}

		/// Returns the inverse matrix, or the identity matrix when the determinant is degenerate.
		constexpr Matrix3 Inverted() const noexcept
		{
			const float determinant = Determinant();
			if (Abs(determinant) <= kEpsilon)
				return Identity();
			const float inverse_determinant = 1.0f / determinant;
			Matrix3 result{};
			result.m[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inverse_determinant;
			result.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inverse_determinant;
			result.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inverse_determinant;
			result.m[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inverse_determinant;
			result.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inverse_determinant;
			result.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inverse_determinant;
			result.m[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inverse_determinant;
			result.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inverse_determinant;
			result.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inverse_determinant;
			return result;
		}

		/// Transforms a point with translation applied.
		constexpr Vector2 TransformPoint(const Vector2 &point) const noexcept
		{
			return {
				point.x * m[0][0] + point.y * m[1][0] + m[2][0],
				point.x * m[0][1] + point.y * m[1][1] + m[2][1]};
		}

		/// Transforms a direction with translation ignored.
		constexpr Vector2 TransformDirection(const Vector2 &direction) const noexcept
		{
			return {
				direction.x * m[0][0] + direction.y * m[1][0],
				direction.x * m[0][1] + direction.y * m[1][1]};
		}

		/// Returns whether every element matches within a tolerance.
		constexpr bool ApproximatelyEquals(const Matrix3 &other, float tolerance = kEpsilon) const noexcept
		{
			for (int row = 0; row < 3; ++row)
			{
				for (int column = 0; column < 3; ++column)
				{
					if (!Approximately(m[row][column], other.m[row][column], tolerance))
						return false;
				}
			}
			return true;
		}
	};

	/// Represents a row-major 4x4 transformation matrix using row-vector convention.
	/// Points are transformed as ``point * matrix``, so translation resides in the fourth row
	/// and rotation matrices are the transpose of their column-vector equivalents.
	struct Matrix4
	{
		float m[4][4]{};

		/// Returns an identity matrix.
		static constexpr Matrix4 Identity() noexcept
		{
			return {{{1.0f, 0.0f, 0.0f, 0.0f},
					 {0.0f, 1.0f, 0.0f, 0.0f},
					 {0.0f, 0.0f, 1.0f, 0.0f},
					 {0.0f, 0.0f, 0.0f, 1.0f}}};
		}

		/// Builds a translation matrix.
		static constexpr Matrix4 CreateTranslation(const Vector3 &translation) noexcept
		{
			Matrix4 result = Identity();
			result.m[3][0] = translation.x;
			result.m[3][1] = translation.y;
			result.m[3][2] = translation.z;
			return result;
		}

		/// Builds a non-uniform scale matrix.
		static constexpr Matrix4 CreateScale(const Vector3 &scale) noexcept
		{
			Matrix4 result = Identity();
			result.m[0][0] = scale.x;
			result.m[1][1] = scale.y;
			result.m[2][2] = scale.z;
			return result;
		}

		/// Builds a right-handed rotation matrix around X.
		static Matrix4 CreateRotationX(float radians) noexcept
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			Matrix4 result = Identity();
			result.m[1][1] = cosine;
			result.m[1][2] = sine;
			result.m[2][1] = -sine;
			result.m[2][2] = cosine;
			return result;
		}

		/// Builds a right-handed rotation matrix around Y.
		static Matrix4 CreateRotationY(float radians) noexcept
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			Matrix4 result = Identity();
			result.m[0][0] = cosine;
			result.m[0][2] = -sine;
			result.m[2][0] = sine;
			result.m[2][2] = cosine;
			return result;
		}

		/// Builds a counter-clockwise rotation matrix around Z.
		static Matrix4 CreateRotationZ(float radians) noexcept
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			Matrix4 result = Identity();
			result.m[0][0] = cosine;
			result.m[0][1] = sine;
			result.m[1][0] = -sine;
			result.m[1][1] = cosine;
			return result;
		}

		/// Builds a right-handed rotation around an arbitrary axis of any non-zero length.
		static Matrix4 CreateRotationAxisAngle(const Vector3 &axis, float radians) noexcept
		{
			const Vector3 normalized = axis.Normalized();
			if (normalized.LengthSquared() <= kEpsilon)
				return Identity();
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			const float inverse_cosine = 1.0f - cosine;
			Matrix4 result = Identity();
			result.m[0][0] = normalized.x * normalized.x * inverse_cosine + cosine;
			result.m[0][1] = normalized.x * normalized.y * inverse_cosine + normalized.z * sine;
			result.m[0][2] = normalized.x * normalized.z * inverse_cosine - normalized.y * sine;
			result.m[1][0] = normalized.x * normalized.y * inverse_cosine - normalized.z * sine;
			result.m[1][1] = normalized.y * normalized.y * inverse_cosine + cosine;
			result.m[1][2] = normalized.y * normalized.z * inverse_cosine + normalized.x * sine;
			result.m[2][0] = normalized.x * normalized.z * inverse_cosine + normalized.y * sine;
			result.m[2][1] = normalized.y * normalized.z * inverse_cosine - normalized.x * sine;
			result.m[2][2] = normalized.z * normalized.z * inverse_cosine + cosine;
			return result;
		}

		/// Composes roll around Z, then pitch around X, then yaw around Y.
		static Matrix4 CreateRotationYawPitchRoll(float yaw, float pitch, float roll) noexcept
		{
			return CreateRotationZ(roll) * CreateRotationX(pitch) * CreateRotationY(yaw);
		}

		/// Builds a right-handed view matrix that places the eye at the origin looking down -Z.
		static Matrix4 CreateLookAt(const Vector3 &eye, const Vector3 &target, const Vector3 &up) noexcept
		{
			const Vector3 to_target = target - eye;
			if (to_target.LengthSquared() <= kEpsilon)
				return Identity();
			Vector3 forward = (eye - target).Normalized();
			Vector3 right = up.Cross(forward).Normalized();
			if (right.LengthSquared() <= kEpsilon)
				right = Vector3::UnitX();
			const Vector3 view_up = forward.Cross(right);
			Matrix4 result = Identity();
			result.m[0][0] = right.x;
			result.m[0][1] = view_up.x;
			result.m[0][2] = forward.x;
			result.m[1][0] = right.y;
			result.m[1][1] = view_up.y;
			result.m[1][2] = forward.y;
			result.m[2][0] = right.z;
			result.m[2][1] = view_up.z;
			result.m[2][2] = forward.z;
			result.m[3][0] = -right.Dot(eye);
			result.m[3][1] = -view_up.Dot(eye);
			result.m[3][2] = -forward.Dot(eye);
			return result;
		}

		/// Builds a right-handed perspective projection mapping the view depth range to zero-to-one.
		static Matrix4 CreatePerspectiveFieldOfView(
			float vertical_field_of_view,
			float aspect_ratio,
			float near_plane,
			float far_plane) noexcept
		{
			if (vertical_field_of_view <= 0.0f || vertical_field_of_view >= kPi ||
				aspect_ratio <= 0.0f || near_plane <= 0.0f || far_plane <= near_plane)
				return Identity();
			const float y_scale = 1.0f / std::tan(vertical_field_of_view * 0.5f);
			Matrix4 result{};
			result.m[0][0] = y_scale / aspect_ratio;
			result.m[1][1] = y_scale;
			result.m[2][2] = far_plane / (near_plane - far_plane);
			result.m[2][3] = -1.0f;
			result.m[3][2] = near_plane * far_plane / (near_plane - far_plane);
			return result;
		}

		/// Builds a right-handed perspective projection from physical near-plane dimensions.
		static constexpr Matrix4 CreatePerspective(
			float width,
			float height,
			float near_plane,
			float far_plane) noexcept
		{
			if (width <= 0.0f || height <= 0.0f || near_plane <= 0.0f || far_plane <= near_plane)
				return Identity();
			Matrix4 result{};
			result.m[0][0] = 2.0f * near_plane / width;
			result.m[1][1] = 2.0f * near_plane / height;
			result.m[2][2] = far_plane / (near_plane - far_plane);
			result.m[2][3] = -1.0f;
			result.m[3][2] = near_plane * far_plane / (near_plane - far_plane);
			return result;
		}

		/// Builds a centered right-handed orthographic projection mapping depth to zero-to-one.
		static constexpr Matrix4 CreateOrthographic(
			float width,
			float height,
			float near_plane,
			float far_plane) noexcept
		{
			return CreateOrthographicOffCenter(
				-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, near_plane, far_plane);
		}

		/// Builds a right-handed orthographic projection mapping the view depth range to zero-to-one.
		static constexpr Matrix4 CreateOrthographicOffCenter(
			float left,
			float right,
			float bottom,
			float top,
			float near_plane,
			float far_plane) noexcept
		{
			if (right == left || top == bottom || far_plane == near_plane)
				return Identity();
			Matrix4 result{};
			result.m[0][0] = 2.0f / (right - left);
			result.m[1][1] = 2.0f / (top - bottom);
			result.m[2][2] = 1.0f / (near_plane - far_plane);
			result.m[3][0] = -(right + left) / (right - left);
			result.m[3][1] = -(top + bottom) / (top - bottom);
			result.m[3][2] = near_plane / (near_plane - far_plane);
			result.m[3][3] = 1.0f;
			return result;
		}

		/// Multiplies two transformation matrices, applying the left operand first.
		constexpr Matrix4 operator*(const Matrix4 &other) const noexcept
		{
			Matrix4 result{};
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
				{
					result.m[row][column] =
						m[row][0] * other.m[0][column] +
						m[row][1] * other.m[1][column] +
						m[row][2] * other.m[2][column] +
						m[row][3] * other.m[3][column];
				}
			}
			return result;
		}

		/// Transforms a homogeneous vector by the full matrix.
		constexpr Vector4 Transform(const Vector4 &vector) const noexcept
		{
			return {
				vector.x * m[0][0] + vector.y * m[1][0] + vector.z * m[2][0] + vector.w * m[3][0],
				vector.x * m[0][1] + vector.y * m[1][1] + vector.z * m[2][1] + vector.w * m[3][1],
				vector.x * m[0][2] + vector.y * m[1][2] + vector.z * m[2][2] + vector.w * m[3][2],
				vector.x * m[0][3] + vector.y * m[1][3] + vector.z * m[2][3] + vector.w * m[3][3]};
		}

		/// Transforms a point with translation applied.
		constexpr Vector3 TransformPoint(const Vector3 &point) const noexcept
		{
			return {
				point.x * m[0][0] + point.y * m[1][0] + point.z * m[2][0] + m[3][0],
				point.x * m[0][1] + point.y * m[1][1] + point.z * m[2][1] + m[3][1],
				point.x * m[0][2] + point.y * m[1][2] + point.z * m[2][2] + m[3][2]};
		}

		/// Transforms a direction with translation ignored.
		constexpr Vector3 TransformDirection(const Vector3 &direction) const noexcept
		{
			return {
				direction.x * m[0][0] + direction.y * m[1][0] + direction.z * m[2][0],
				direction.x * m[0][1] + direction.y * m[1][1] + direction.z * m[2][1],
				direction.x * m[0][2] + direction.y * m[1][2] + direction.z * m[2][2]};
		}

		/// Returns the signed matrix determinant.
		constexpr float Determinant() const noexcept
		{
			return m[0][0] * MinorDeterminant(0, 0) -
				   m[0][1] * MinorDeterminant(0, 1) +
				   m[0][2] * MinorDeterminant(0, 2) -
				   m[0][3] * MinorDeterminant(0, 3);
		}

		/// Returns the transposed matrix.
		constexpr Matrix4 Transposed() const noexcept
		{
			Matrix4 result{};
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
					result.m[row][column] = m[column][row];
			}
			return result;
		}

		/// Returns the inverse matrix, or the identity matrix when the determinant is degenerate.
		constexpr Matrix4 Inverted() const noexcept
		{
			const float determinant = Determinant();
			if (Abs(determinant) <= kEpsilon)
				return Identity();
			float cofactors[4][4]{};
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
				{
					const float minor = MinorDeterminant(row, column);
					cofactors[row][column] = ((row + column) & 1) != 0 ? -minor : minor;
				}
			}
			const float inverse_determinant = 1.0f / determinant;
			Matrix4 result{};
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
					result.m[row][column] = cofactors[column][row] * inverse_determinant;
			}
			return result;
		}

		/// Returns the upper-left rotation and scale block.
		constexpr Matrix3 ToMatrix3() const noexcept
		{
			Matrix3 result{};
			for (int row = 0; row < 3; ++row)
			{
				for (int column = 0; column < 3; ++column)
					result.m[row][column] = m[row][column];
			}
			return result;
		}

		/// Returns whether every element matches within a tolerance.
		constexpr bool ApproximatelyEquals(const Matrix4 &other, float tolerance = kEpsilon) const noexcept
		{
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
				{
					if (!Approximately(m[row][column], other.m[row][column], tolerance))
						return false;
				}
			}
			return true;
		}

	private:
		/// Returns the determinant of the 3x3 block left after removing one row and column.
		constexpr float MinorDeterminant(int removed_row, int removed_column) const noexcept
		{
			float block[3][3]{};
			for (int row = 0, target_row = 0; row < 4; ++row)
			{
				if (row == removed_row)
					continue;
				for (int column = 0, target_column = 0; column < 4; ++column)
				{
					if (column == removed_column)
						continue;
					block[target_row][target_column] = m[row][column];
					++target_column;
				}
				++target_row;
			}
			return block[0][0] * (block[1][1] * block[2][2] - block[1][2] * block[2][1]) -
				   block[0][1] * (block[1][0] * block[2][2] - block[1][2] * block[2][0]) +
				   block[0][2] * (block[1][0] * block[2][1] - block[1][1] * block[2][0]);
		}
	};
} // namespace BlackLight::Math
