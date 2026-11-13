#pragma once

#include "Scalar.h"

#include <cstdint>

namespace BlackLight::Math
{
	/// Stores a four-component RGBA color in normalized floating-point form.
	struct Color
	{
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;

		/// Returns opaque white, which leaves sprite and texture colors unchanged.
		static constexpr Color White() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }
		/// Returns opaque black.
		static constexpr Color Black() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }
		/// Returns fully transparent black.
		static constexpr Color Transparent() noexcept { return {0.0f, 0.0f, 0.0f, 0.0f}; }

		/// Creates a color from 8-bit RGBA channels.
		static constexpr Color FromRgba8(
			std::uint8_t red,
			std::uint8_t green,
			std::uint8_t blue,
			std::uint8_t alpha = 255) noexcept
		{
			return {
				static_cast<float>(red) / 255.0f,
				static_cast<float>(green) / 255.0f,
				static_cast<float>(blue) / 255.0f,
				static_cast<float>(alpha) / 255.0f};
		}

		/// Creates a color from a packed 0xRRGGBBAA value.
		static constexpr Color FromHex(std::uint32_t rgba) noexcept
		{
			return {
				static_cast<float>((rgba >> 24) & 0xFFu) / 255.0f,
				static_cast<float>((rgba >> 16) & 0xFFu) / 255.0f,
				static_cast<float>((rgba >> 8) & 0xFFu) / 255.0f,
				static_cast<float>(rgba & 0xFFu) / 255.0f};
		}

		/// Returns the color with a replaced alpha channel.
		constexpr Color WithAlpha(float alpha) const noexcept
		{
			return {r, g, b, alpha};
		}

		/// Returns the packed 0xRRGGBBAA representation with channels clamped to 8 bits.
		constexpr std::uint32_t ToRgba8() const noexcept
		{
			return (ToByte(r) << 24) | (ToByte(g) << 16) | (ToByte(b) << 8) | ToByte(a);
		}

		/// Returns the component-wise product used to tint a color.
		constexpr Color Modulate(const Color &other) const noexcept
		{
			return {r * other.r, g * other.g, b * other.b, a * other.a};
		}

		/// Returns the color scaled uniformly in every channel.
		constexpr Color Scaled(float scalar) const noexcept
		{
			return {r * scalar, g * scalar, b * scalar, a * scalar};
		}

		/// Interpolates linearly toward another color without clamping the factor.
		static constexpr Color Lerp(const Color &from, const Color &to, float factor) noexcept
		{
			return {
				Math::Lerp(from.r, to.r, factor),
				Math::Lerp(from.g, to.g, factor),
				Math::Lerp(from.b, to.b, factor),
				Math::Lerp(from.a, to.a, factor)};
		}

		/// Composites a source color over a destination color using straight alpha.
		static constexpr Color Blend(const Color &source, const Color &destination) noexcept
		{
			const float destination_weight = destination.a * (1.0f - source.a);
			const float alpha = source.a + destination_weight;
			if (alpha <= kEpsilon)
				return Transparent();
			return {
				(source.r * source.a + destination.r * destination_weight) / alpha,
				(source.g * source.a + destination.g * destination_weight) / alpha,
				(source.b * source.a + destination.b * destination_weight) / alpha,
				alpha};
		}

		/// Returns whether every channel matches within a tolerance.
		constexpr bool ApproximatelyEquals(const Color &other, float tolerance = kEpsilon) const noexcept
		{
			return Math::Approximately(r, other.r, tolerance) &&
				   Math::Approximately(g, other.g, tolerance) &&
				   Math::Approximately(b, other.b, tolerance) &&
				   Math::Approximately(a, other.a, tolerance);
		}

	private:
		/// Converts one normalized channel into a rounded 8-bit value.
		static constexpr std::uint32_t ToByte(float value) noexcept
		{
			return static_cast<std::uint32_t>(Clamp01(value) * 255.0f + 0.5f);
		}
	};
} // namespace BlackLight::Math
