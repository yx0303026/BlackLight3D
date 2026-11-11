#include "engine/Time.h"

#include <algorithm>

namespace BlackLight::Engine
{

	FrameClock::FrameClock(double fixed_step_seconds, double maximum_delta_seconds) noexcept
		: previous_time_(Clock::now()), fixed_step_seconds_(fixed_step_seconds > 0.0 ? fixed_step_seconds : 1.0 / 60.0), maximum_delta_seconds_(maximum_delta_seconds > 0.0 ? maximum_delta_seconds : 0.25)
	{
		game_time_.fixed_elapsed_seconds = fixed_step_seconds_;
	}

	void FrameClock::Reset() noexcept
	{
		previous_time_ = Clock::now();
		accumulator_seconds_ = 0.0;
		game_time_ = {};
		game_time_.fixed_elapsed_seconds = fixed_step_seconds_;
	}

	void FrameClock::Tick() noexcept
	{
		const auto now = Clock::now();
		const std::chrono::duration<double> elapsed = now - previous_time_;
		previous_time_ = now;

		const double delta = std::clamp(elapsed.count(), 0.0, maximum_delta_seconds_);
		accumulator_seconds_ += delta;
		game_time_.elapsed_seconds = delta;
		game_time_.total_seconds += delta;
		game_time_.interpolation = accumulator_seconds_ / fixed_step_seconds_;
		game_time_.fixed_steps = 0;
		++game_time_.frame_index;
	}

	bool FrameClock::ConsumeFixedStep() noexcept
	{
		if (accumulator_seconds_ < fixed_step_seconds_)
		{
			game_time_.interpolation = accumulator_seconds_ / fixed_step_seconds_;
			return false;
		}

		accumulator_seconds_ -= fixed_step_seconds_;
		game_time_.fixed_steps++;
		game_time_.interpolation = accumulator_seconds_ / fixed_step_seconds_;
		return true;
	}

	const GameTime &FrameClock::GetGameTime() const noexcept
	{
		return game_time_;
	}

	double FrameClock::GetFixedStepSeconds() const noexcept
	{
		return fixed_step_seconds_;
	}

} // namespace BlackLight::Engine
