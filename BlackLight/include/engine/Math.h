#pragma once

#include "../BlackLightApi.h"

#include <cmath>
#include <cstdint>

namespace BlackLight::Math {

/// Represents a two-dimensional floating-point vector.
struct Vector2 {
	float x = 0.0f;
	float y = 0.0f;

	/// Adds another vector component by component.
	constexpr Vector2 operator+(const Vector2& other) const noexcept {
		return {x + other.x, y + other.y};
	}

	/// Subtracts another vector component by component.
	constexpr Vector2 operator-(const Vector2& other) const noexcept {
		return {x - other.x, y - other.y};
	}

	/// Multiplies both components by a scalar.
	constexpr Vector2 operator*(float scalar) const noexcept {
		return {x * scalar, y * scalar};
	}

	/// Returns the squared vector length without taking a square root.
	constexpr float LengthSquared() const noexcept {
		return x * x + y * y;
	}

	/// Returns the Euclidean vector length.
	float Length() const noexcept {
		return std::sqrt(LengthSquared());
	}
};

/// Represents a three-dimensional floating-point vector.
struct Vector3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	/// Adds another vector component by component.
	constexpr Vector3 operator+(const Vector3& other) const noexcept {
		return {x + other.x, y + other.y, z + other.z};
	}

	/// Subtracts another vector component by component.
	constexpr Vector3 operator-(const Vector3& other) const noexcept {
		return {x - other.x, y - other.y, z - other.z};
	}

	/// Multiplies all components by a scalar.
	constexpr Vector3 operator*(float scalar) const noexcept {
		return {x * scalar, y * scalar, z * scalar};
	}

	/// Returns the squared vector length.
	constexpr float LengthSquared() const noexcept {
		return x * x + y * y + z * z;
	}

	/// Returns the Euclidean vector length.
	float Length() const noexcept {
		return std::sqrt(LengthSquared());
	}
};

/// Stores a four-component RGBA color in normalized floating-point form.
struct Color {
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;

	/// Creates an opaque color from 8-bit RGBA channels.
	static constexpr Color FromRgba8(
		std::uint8_t red,
		std::uint8_t green,
		std::uint8_t blue,
		std::uint8_t alpha = 255) noexcept {
		return {
			static_cast<float>(red) / 255.0f,
			static_cast<float>(green) / 255.0f,
			static_cast<float>(blue) / 255.0f,
			static_cast<float>(alpha) / 255.0f
		};
	}
};

/// Represents a row-major 4x4 transformation matrix.
struct Matrix4 {
	float m[4][4]{};

	/// Returns an identity matrix.
	static constexpr Matrix4 Identity() noexcept {
		return {{
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 0.0f, 1.0f}
		}};
	}

	/// Builds a translation matrix.
	static constexpr Matrix4 CreateTranslation(const Vector3& translation) noexcept {
		Matrix4 result = Identity();
		result.m[3][0] = translation.x;
		result.m[3][1] = translation.y;
		result.m[3][2] = translation.z;
		return result;
	}

	/// Builds a non-uniform scale matrix.
	static constexpr Matrix4 CreateScale(const Vector3& scale) noexcept {
		Matrix4 result = Identity();
		result.m[0][0] = scale.x;
		result.m[1][1] = scale.y;
		result.m[2][2] = scale.z;
		return result;
	}

	/// Multiplies two transformation matrices.
	constexpr Matrix4 operator*(const Matrix4& other) const noexcept {
		Matrix4 result{};
		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				for (int element = 0; element < 4; ++element) {
					result.m[row][column] += m[row][element] * other.m[element][column];
				}
			}
		}
		return result;
	}
};

} // namespace BlackLight::Math
