#pragma once

#include "engine/Logger.h"

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

namespace BlackLight::Editor
{

	/// Collects recent log records for the editor Output panel.
	///
	/// Logger sinks can be called from any thread, so records are guarded by a
	/// mutex and consumers receive a snapshot. A monotonic revision lets the UI
	/// refresh only when new records arrive.
	class LogBuffer final
	{
	public:
		/// Creates an empty buffer retaining up to the supplied number of records.
		explicit LogBuffer(std::size_t capacity = 500);

		/// Appends one record, dropping the oldest when at capacity.
		void Add(const Engine::LogRecord &record);
		/// Removes every record.
		void Clear();
		/// Returns a thread-safe copy of the retained records, oldest first.
		std::vector<Engine::LogRecord> Snapshot() const;
		/// Returns the number of retained records.
		std::size_t GetSize() const;
		/// Returns a counter that changes on every add or clear.
		std::size_t GetRevision() const noexcept;

	private:
		mutable std::mutex mutex_;
		std::deque<Engine::LogRecord> records_;
		std::size_t capacity_ = 500;
		std::atomic<std::size_t> revision_{0};
	};

} // namespace BlackLight::Editor
