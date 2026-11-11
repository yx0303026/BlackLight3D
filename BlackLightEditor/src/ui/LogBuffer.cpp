#include "editor/ui/LogBuffer.h"

#include <algorithm>
#include <utility>

namespace BlackLight::Editor
{

	LogBuffer::LogBuffer(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {}

	void LogBuffer::Add(const Engine::LogRecord &record)
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			records_.push_back(record);
			while (records_.size() > capacity_)
				records_.pop_front();
		}
		revision_.fetch_add(1, std::memory_order_relaxed);
	}

	void LogBuffer::Clear()
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			records_.clear();
		}
		revision_.fetch_add(1, std::memory_order_relaxed);
	}

	std::vector<Engine::LogRecord> LogBuffer::Snapshot() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return {records_.begin(), records_.end()};
	}

	std::size_t LogBuffer::GetSize() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return records_.size();
	}

	std::size_t LogBuffer::GetRevision() const noexcept
	{
		return revision_.load(std::memory_order_relaxed);
	}

} // namespace BlackLight::Editor
