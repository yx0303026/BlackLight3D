#pragma once

#include "../BlackLightApi.h"

#include <chrono>
#include <cstdint>

namespace BlackLight::Engine {

/// Contains the timing values for one rendered frame.
struct GameTime {
	double elapsed_seconds = 0.0;
	double total_seconds = 0.0;
	double fixed_elapsed_seconds = 1.0 / 60.0;
	double interpolation = 0.0;
	std::uint64_t frame_index = 0;
	std::uint32_t fixed_steps = 0;
};

BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

/// Provides a monotonic frame clock with a bounded accumulator for fixed updates.
class BLACKLIGHT_API FrameClock final {
public:
	/// Creates a clock using a 60 Hz fixed update and a 250 ms catch-up cap.
	explicit FrameClock(double fixed_step_seconds = 1.0 / 60.0,
		double maximum_delta_seconds = 0.25) noexcept;

	/// Resets elapsed and total time while retaining the configured step sizes.
	void Reset() noexcept;

	/// Samples the monotonic clock and advances the variable-time accumulator.
	void Tick() noexcept;

	/// Consumes one fixed update from the accumulator when one is available.
	bool ConsumeFixedStep() noexcept;

	/// Returns the current frame timing snapshot.
	const GameTime& GetGameTime() const noexcept;

	/// Returns the configured fixed update duration.
	double GetFixedStepSeconds() const noexcept;

private:
	using Clock = std::chrono::steady_clock;

	Clock::time_point previous_time_;
	double fixed_step_seconds_;
	double maximum_delta_seconds_;
	double accumulator_seconds_ = 0.0;
	GameTime game_time_{};
};

BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine
