#pragma once

#include "../BlackLightApi.h"
#include "Math.h"

#include <array>
#include <cstdint>

namespace BlackLight::Engine
{

	/// Identifies the supported mouse buttons.
	enum class MouseButton : std::uint8_t
	{
		Left = 0,
		Right,
		Middle,
		X1,
		X2,
		Count
	};

	/// Stores keyboard and mouse state for one frame.
	class BLACKLIGHT_API InputState final
	{
	public:
		/// Starts a new frame and clears transient press, release, and wheel values.
		void BeginFrame() noexcept;

		/// Sets a platform key code to its current pressed state.
		void SetKeyDown(std::uint32_t key_code, bool down) noexcept;

		/// Returns whether a key is currently held.
		bool IsKeyDown(std::uint32_t key_code) const noexcept;

		/// Returns whether a key became pressed during the current frame.
		bool WasKeyPressed(std::uint32_t key_code) const noexcept;

		/// Returns whether a key became released during the current frame.
		bool WasKeyReleased(std::uint32_t key_code) const noexcept;

		/// Sets a mouse button to its current pressed state.
		void SetMouseButtonDown(MouseButton button, bool down) noexcept;

		/// Returns whether a mouse button is currently held.
		bool IsMouseButtonDown(MouseButton button) const noexcept;

		/// Returns whether a mouse button became pressed during this frame.
		bool WasMouseButtonPressed(MouseButton button) const noexcept;

		/// Returns whether a mouse button became released during this frame.
		bool WasMouseButtonReleased(MouseButton button) const noexcept;

		/// Updates the absolute mouse position in client coordinates.
		void SetMousePosition(Math::Vector2 position) noexcept;

		/// Returns the current absolute mouse position.
		Math::Vector2 GetMousePosition() const noexcept;

		/// Adds a mouse wheel delta to the current frame.
		void AddMouseWheelDelta(float delta) noexcept;

		/// Returns the accumulated mouse wheel delta for this frame.
		float GetMouseWheelDelta() const noexcept;

	private:
		static constexpr std::size_t KeyCount = 512;

		std::array<bool, KeyCount> keys_{};
		std::array<bool, KeyCount> pressed_keys_{};
		std::array<bool, KeyCount> released_keys_{};
		std::array<bool, static_cast<std::size_t>(MouseButton::Count)> mouse_buttons_{};
		std::array<bool, static_cast<std::size_t>(MouseButton::Count)> pressed_mouse_buttons_{};
		std::array<bool, static_cast<std::size_t>(MouseButton::Count)> released_mouse_buttons_{};
		Math::Vector2 mouse_position_{};
		float mouse_wheel_delta_ = 0.0f;
	};

} // namespace BlackLight::Engine
