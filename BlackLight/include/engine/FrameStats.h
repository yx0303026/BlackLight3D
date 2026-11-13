#pragma once

#include "../BlackLightApi.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace BlackLight::Engine
{
	/// Reports CPU/GPU frame timing and sprite submission counters.
	struct FrameStatsSnapshot
	{
		double frames_per_second = 0.0;
		double cpu_frame_milliseconds = 0.0;
		double gpu_frame_milliseconds = 0.0;
		std::uint64_t draw_calls = 0;
		std::uint64_t vertices = 0;
		std::uint64_t texture_switches = 0;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

	/// Accumulates per-frame measurements for a game host.
	class BLACKLIGHT_API FrameStats final
	{
	public:
		/// Creates an empty statistics accumulator.
		FrameStats();
		/// Releases private timing storage.
		~FrameStats();
		/// Prevents copying the timing accumulator.
		FrameStats(const FrameStats &) = delete;
		/// Prevents assigning the timing accumulator.
		FrameStats &operator=(const FrameStats &) = delete;
		/// Starts a new measurement window and clears current-frame draw counters.
		void BeginFrame();
		/// Publishes the active frame's CPU timing and draw counters.
		void EndFrame();
		/// Adds one sprite draw and its submitted vertex count.
		void RecordDraw(std::uint64_t vertices, bool texture_changed) noexcept;
		/// Stores the latest completed GPU timing when available.
		void SetGpuFrameMilliseconds(double milliseconds) noexcept;
		/// Returns completed-frame counters, FPS, and the latest available GPU duration.
		FrameStatsSnapshot GetSnapshot() const noexcept;

	private:
		class Impl;
		std::unique_ptr<Impl> impl_;
	};

	BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP
}
