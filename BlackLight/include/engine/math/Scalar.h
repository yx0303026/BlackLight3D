#pragma once

namespace BlackLight::Math
{
	/// Circle ratio expressed with single precision.
	constexpr float kPi = 3.14159265358979323846f;
	/// Half of a full turn in radians.
	constexpr float kHalfPi = 1.57079632679489661923f;
	/// Full turn in radians.
	constexpr float kTwoPi = 6.28318530717958647692f;
	/// Multiplier converting degrees into radians.
	constexpr float kDegreesToRadians = 0.01745329251994329577f;
	/// Multiplier converting radians into degrees.
	constexpr float kRadiansToDegrees = 57.29577951308232087680f;
	/// Default tolerance used by approximate floating-point comparisons.
	constexpr float kEpsilon = 1.0e-6f;

	/// Converts an angle from degrees into radians.
	constexpr float ToRadians(float degrees) noexcept
	{
		return degrees * kDegreesToRadians;
	}

	/// Converts an angle from radians into degrees.
	constexpr float ToDegrees(float radians) noexcept
	{
		return radians * kRadiansToDegrees;
	}

	/// Returns the absolute value of a scalar.
	constexpr float Abs(float value) noexcept
	{
		return value < 0.0f ? -value : value;
	}

	/// Restricts a value to an inclusive range.
	constexpr float Clamp(float value, float minimum, float maximum) noexcept
	{
		return value < minimum ? minimum : (value > maximum ? maximum : value);
	}

	/// Restricts a value to the normalized zero-to-one range.
	constexpr float Clamp01(float value) noexcept
	{
		return Clamp(value, 0.0f, 1.0f);
	}

	/// Interpolates linearly from one scalar to another without clamping the factor.
	constexpr float Lerp(float from, float to, float factor) noexcept
	{
		return from + (to - from) * factor;
	}

	/// Returns -1, 0, or 1 matching the sign of a scalar.
	constexpr float Sign(float value) noexcept
	{
		return value < 0.0f ? -1.0f : (value > 0.0f ? 1.0f : 0.0f);
	}

	/// Returns whether two scalars differ by no more than a tolerance.
	constexpr bool Approximately(float left, float right, float tolerance = kEpsilon) noexcept
	{
		return Abs(left - right) <= tolerance;
	}
} // namespace BlackLight::Math
