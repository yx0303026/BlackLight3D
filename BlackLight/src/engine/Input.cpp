#include "engine/Input.h"

namespace BlackLight::Engine
{

	void InputState::BeginFrame() noexcept
	{
		pressed_keys_.fill(false);
		released_keys_.fill(false);
		pressed_mouse_buttons_.fill(false);
		released_mouse_buttons_.fill(false);
		mouse_wheel_delta_ = 0.0f;
	}

	void InputState::SetKeyDown(std::uint32_t key_code, bool down) noexcept
	{
		if (key_code >= KeyCount)
		{
			return;
		}
		const bool previous = keys_[key_code];
		keys_[key_code] = down;
		if (down && !previous)
		{
			pressed_keys_[key_code] = true;
		}
		else if (!down && previous)
		{
			released_keys_[key_code] = true;
		}
	}

	bool InputState::IsKeyDown(std::uint32_t key_code) const noexcept
	{
		return key_code < KeyCount && keys_[key_code];
	}

	bool InputState::WasKeyPressed(std::uint32_t key_code) const noexcept
	{
		return key_code < KeyCount && pressed_keys_[key_code];
	}

	bool InputState::WasKeyReleased(std::uint32_t key_code) const noexcept
	{
		return key_code < KeyCount && released_keys_[key_code];
	}

	void InputState::SetMouseButtonDown(MouseButton button, bool down) noexcept
	{
		const auto index = static_cast<std::size_t>(button);
		if (index >= mouse_buttons_.size())
		{
			return;
		}
		const bool previous = mouse_buttons_[index];
		mouse_buttons_[index] = down;
		if (down && !previous)
		{
			pressed_mouse_buttons_[index] = true;
		}
		else if (!down && previous)
		{
			released_mouse_buttons_[index] = true;
		}
	}

	bool InputState::IsMouseButtonDown(MouseButton button) const noexcept
	{
		const auto index = static_cast<std::size_t>(button);
		return index < mouse_buttons_.size() && mouse_buttons_[index];
	}

	bool InputState::WasMouseButtonPressed(MouseButton button) const noexcept
	{
		const auto index = static_cast<std::size_t>(button);
		return index < pressed_mouse_buttons_.size() && pressed_mouse_buttons_[index];
	}

	bool InputState::WasMouseButtonReleased(MouseButton button) const noexcept
	{
		const auto index = static_cast<std::size_t>(button);
		return index < released_mouse_buttons_.size() && released_mouse_buttons_[index];
	}

	void InputState::SetMousePosition(Math::Vector2 position) noexcept
	{
		mouse_position_ = position;
	}

	Math::Vector2 InputState::GetMousePosition() const noexcept
	{
		return mouse_position_;
	}

	void InputState::AddMouseWheelDelta(float delta) noexcept
	{
		mouse_wheel_delta_ += delta;
	}

	float InputState::GetMouseWheelDelta() const noexcept
	{
		return mouse_wheel_delta_;
	}

} // namespace BlackLight::Engine
