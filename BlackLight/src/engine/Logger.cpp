#include "engine/Logger.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <utility>
#include <vector>

namespace BlackLight::Engine
{

	namespace
	{

		LogLevel DefaultMinimumLevel() noexcept
		{
#if defined(NDEBUG)
			return LogLevel::Info;
#else
			return LogLevel::Debug;
#endif
		}

		bool IsLevelEnabled(LogLevel minimum_level, LogLevel level) noexcept
		{
			return minimum_level != LogLevel::Off && level != LogLevel::Off && level >= minimum_level;
		}

	} // namespace

	struct Logger::Impl
	{
		mutable std::mutex state_mutex;
		mutable std::mutex output_mutex;
		LogLevel minimum_level = DefaultMinimumLevel();
		std::vector<LogSink> sinks;
		bool console_enabled = true;
		std::unique_ptr<std::ofstream> file;
	};

	Logger::Logger()
		: impl_(std::make_unique<Impl>()) {}

	Logger::~Logger()
	{
		try
		{
			Flush();
		}
		catch (...)
		{
		}
	}

	Logger &Logger::Get()
	{
		// Deliberately leaked so logging stays valid while other static objects are destroyed.
		static Logger *instance = new Logger();
		return *instance;
	}

	void Logger::SetMinimumLevel(LogLevel level)
	{
		std::scoped_lock lock(impl_->state_mutex);
		impl_->minimum_level = level;
	}

	LogLevel Logger::GetMinimumLevel() const
	{
		std::scoped_lock lock(impl_->state_mutex);
		return impl_->minimum_level;
	}

	bool Logger::IsEnabled(LogLevel level) const
	{
		std::scoped_lock lock(impl_->state_mutex);
		return IsLevelEnabled(impl_->minimum_level, level);
	}

	void Logger::Log(LogLevel level, std::string_view message, std::string_view category)
	{
		std::vector<LogSink> sinks;
		bool console_enabled = false;
		{
			std::scoped_lock lock(impl_->state_mutex);
			if (!IsLevelEnabled(impl_->minimum_level, level))
			{
				return;
			}
			console_enabled = impl_->console_enabled;
			sinks = impl_->sinks;
		}

		LogRecord record;
		record.level = level;
		record.category.assign(category.data(), category.size());
		record.message.assign(message.data(), message.size());

		const std::string formatted = FormatLogRecord(record);
		{
			std::scoped_lock lock(impl_->output_mutex);
			if (console_enabled)
			{
				std::ostream &console = level >= LogLevel::Warning ? std::cerr : std::cout;
				console << formatted << std::flush;
			}
			if (impl_->file && impl_->file->is_open())
			{
				*impl_->file << formatted << std::flush;
			}
		}

		for (const LogSink &sink : sinks)
		{
			try
			{
				if (sink)
				{
					sink(record);
				}
			}
			catch (...)
			{
				// A failing sink must not break the caller's logging path.
			}
		}
	}

	LogStream Logger::Stream(LogLevel level, std::string_view category)
	{
		return LogStream(level, category, IsEnabled(level));
	}

	void Logger::AddSink(LogSink sink)
	{
		if (!sink)
		{
			return;
		}

		std::scoped_lock lock(impl_->state_mutex);
		impl_->sinks.push_back(std::move(sink));
	}

	void Logger::ClearSinks()
	{
		std::scoped_lock lock(impl_->state_mutex);
		impl_->sinks.clear();
	}

	void Logger::SetConsoleEnabled(bool enabled)
	{
		std::scoped_lock lock(impl_->state_mutex);
		impl_->console_enabled = enabled;
	}

	bool Logger::IsConsoleEnabled() const
	{
		std::scoped_lock lock(impl_->state_mutex);
		return impl_->console_enabled;
	}

	bool Logger::SetLogFile(const std::filesystem::path &path, bool append)
	{
		std::ofstream file(path, std::ios::out | (append ? std::ios::app : std::ios::trunc));
		if (!file.is_open())
		{
			return false;
		}

		auto opened_file = std::make_unique<std::ofstream>(std::move(file));
		std::scoped_lock lock(impl_->output_mutex);
		impl_->file = std::move(opened_file);
		return true;
	}

	void Logger::ClearLogFile()
	{
		std::scoped_lock lock(impl_->output_mutex);
		if (impl_->file)
		{
			impl_->file->flush();
			impl_->file.reset();
		}
	}

	void Logger::Flush()
	{
		std::scoped_lock lock(impl_->output_mutex);
		std::cout.flush();
		std::cerr.flush();
		if (impl_->file && impl_->file->is_open())
		{
			impl_->file->flush();
		}
	}

	const char *ToString(LogLevel level) noexcept
	{
		switch (level)
		{
		case LogLevel::Trace:
			return "TRACE";
		case LogLevel::Debug:
			return "DEBUG";
		case LogLevel::Info:
			return "INFO";
		case LogLevel::Warning:
			return "WARN";
		case LogLevel::Error:
			return "ERROR";
		case LogLevel::Critical:
			return "CRITICAL";
		case LogLevel::Off:
			return "OFF";
		}

		return "UNKNOWN";
	}

	std::string FormatLogRecord(const LogRecord &record)
	{
		const std::time_t time = std::chrono::system_clock::to_time_t(record.timestamp);
		std::tm local_time{};
#if defined(_WIN32)
		(void)localtime_s(&local_time, &time);
#else
		(void)localtime_r(&time, &local_time);
#endif

		const auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
											  record.timestamp.time_since_epoch())
											  .count();
		const auto milliseconds = static_cast<int>((elapsed_milliseconds % 1000 + 1000) % 1000);

		std::ostringstream stream;
		stream << '[' << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << '.'
			   << std::setfill('0') << std::setw(3) << milliseconds << "] ["
			   << ToString(record.level) << ']';
		if (!record.category.empty())
		{
			stream << " [" << record.category << ']';
		}
		stream << ' ' << record.message << '\n';
		return stream.str();
	}

	LogStream::LogStream(LogLevel level, std::string_view category, bool enabled)
		: level_(level), category_(category), enabled_(enabled)
	{
	}

	LogStream::~LogStream()
	{
		if (!enabled_)
		{
			return;
		}

		try
		{
			Logger::Get().Log(level_, stream_.str(), category_);
		}
		catch (...)
		{
		}
	}

	LogStream::LogStream(LogStream &&other) noexcept
		: level_(other.level_),
		  category_(std::move(other.category_)),
		  stream_(std::move(other.stream_)),
		  enabled_(other.enabled_)
	{
		other.enabled_ = false;
	}

	LogStream &LogStream::operator<<(std::ios_base &(*manipulator)(std::ios_base &))
	{
		if (enabled_)
		{
			stream_ << manipulator;
		}
		return *this;
	}

	LogStream &LogStream::operator<<(std::ostream &(*manipulator)(std::ostream &))
	{
		if (enabled_)
		{
			stream_ << manipulator;
		}
		return *this;
	}

	bool LogStream::IsEnabled() const noexcept
	{
		return enabled_;
	}

} // namespace BlackLight::Engine
