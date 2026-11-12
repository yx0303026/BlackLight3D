#include "engine/FrameStats.h"

#include <chrono>

namespace BlackLight::Engine
{
	/// Owns timing samples and the current public snapshot.
	class FrameStats::Impl
	{
	public:
		std::chrono::steady_clock::time_point frame_start{};
		std::chrono::steady_clock::time_point previous_frame_start{};
		FrameStatsSnapshot snapshot{};
		double fps_elapsed = 0.0;
		std::uint64_t fps_frames = 0;
		std::uint64_t frame_draw_calls = 0;
		std::uint64_t frame_vertices = 0;
		std::uint64_t frame_texture_switches = 0;
		bool has_previous_frame = false;
	};

	/// Creates the private timing state.
	FrameStats::FrameStats() : impl_(std::make_unique<Impl>()) {}
	/// Releases the private timing state.
	FrameStats::~FrameStats() = default;
	/// Starts the current CPU frame and resets render counters.
	void FrameStats::BeginFrame()
	{
		impl_->frame_start = std::chrono::steady_clock::now();
		if (impl_->has_previous_frame)
		{
			impl_->fps_elapsed +=
				std::chrono::duration<double>(impl_->frame_start - impl_->previous_frame_start).count();
			++impl_->fps_frames;
			if (impl_->fps_elapsed >= 1.0)
			{
				impl_->snapshot.frames_per_second =
					static_cast<double>(impl_->fps_frames) / impl_->fps_elapsed;
				impl_->fps_elapsed = 0.0;
				impl_->fps_frames = 0;
			}
		}
		impl_->previous_frame_start = impl_->frame_start;
		impl_->has_previous_frame = true;
		impl_->frame_draw_calls = 0;
		impl_->frame_vertices = 0;
		impl_->frame_texture_switches = 0;
	}
	/// Records the CPU duration for the active frame.
	void FrameStats::EndFrame()
	{
		const auto now = std::chrono::steady_clock::now();
		impl_->snapshot.cpu_frame_milliseconds =
			std::chrono::duration<double, std::milli>(now - impl_->frame_start).count();
		impl_->snapshot.draw_calls = impl_->frame_draw_calls;
		impl_->snapshot.vertices = impl_->frame_vertices;
		impl_->snapshot.texture_switches = impl_->frame_texture_switches;
	}
	/// Adds one rendered draw and its texture-switch count.
	void FrameStats::RecordDraw(std::uint64_t vertices, bool texture_changed) noexcept
	{
		++impl_->frame_draw_calls;
		impl_->frame_vertices += vertices;
		impl_->frame_texture_switches += texture_changed ? 1u : 0u;
	}
	/// Updates the last completed GPU frame duration.
	void FrameStats::SetGpuFrameMilliseconds(double milliseconds) noexcept
	{
		impl_->snapshot.gpu_frame_milliseconds = milliseconds;
	}
	/// Returns a copy of the current measurement snapshot.
	FrameStatsSnapshot FrameStats::GetSnapshot() const noexcept { return impl_->snapshot; }
}
