#pragma once

#include "../BlackLightApi.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace BlackLight::Engine
{

    /// Identifies the severity of a log record.
    enum class LogLevel : std::uint8_t
    {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        Critical,
        Off
    };

    /// Returns the upper-case name of a log level.
    BLACKLIGHT_API const char *ToString(LogLevel level) noexcept;

    /// Describes one message emitted by the logger.
    struct LogRecord
    {
        LogLevel level = LogLevel::Info;
        std::string category;
        std::string message;
        std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    };

    /// Receives emitted records. Sinks can be called from several threads at once.
    using LogSink = std::function<void(const LogRecord &)>;

    /// Formats a record as "[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [category] message".
    BLACKLIGHT_API std::string FormatLogRecord(const LogRecord &record);

    BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH

    /// Buffers one message and forwards it to the logger when the expression ends.
    class BLACKLIGHT_API LogStream final
    {
    public:
        LogStream(LogLevel level, std::string_view category, bool enabled);
        ~LogStream();

        LogStream(LogStream &&other) noexcept;
        LogStream &operator=(LogStream &&other) = delete;
        LogStream(const LogStream &) = delete;
        LogStream &operator=(const LogStream &) = delete;

        /// Appends a value that the stream supports, for example a string or number.
        template <typename Value>
        LogStream &operator<<(const Value &value)
        {
            if (enabled_)
            {
                stream_ << value;
            }
            return *this;
        }

        /// Appends a stream-state manipulator such as std::hex.
        LogStream &operator<<(std::ios_base &(*manipulator)(std::ios_base &));

        /// Appends an output manipulator such as std::endl.
        LogStream &operator<<(std::ostream &(*manipulator)(std::ostream &));

        /// Returns whether the stream will emit its buffered message.
        bool IsEnabled() const noexcept;

    private:
        LogLevel level_;
        std::string category_;
        std::ostringstream stream_;
        bool enabled_ = false;
    };

    /// Process-wide thread-safe logger with level filtering and pluggable output.
    class BLACKLIGHT_API Logger final
    {
    public:
        /// Returns the process-wide logger instance.
        static Logger &Get();

        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        /// Sets the lowest level that is emitted. Off disables every level.
        void SetMinimumLevel(LogLevel level);

        /// Returns the lowest level that is emitted.
        LogLevel GetMinimumLevel() const;

        /// Returns whether messages at the supplied level are currently emitted.
        bool IsEnabled(LogLevel level) const;

        /// Writes one message with an optional category.
        void Log(LogLevel level, std::string_view message, std::string_view category = {});

        /// Starts a streaming message, for example Logger::Get().Stream(LogLevel::Info) << "text".
        LogStream Stream(LogLevel level, std::string_view category = {});

        /// Adds a sink that receives every emitted record.
        void AddSink(LogSink sink);

        /// Removes every custom sink.
        void ClearSinks();

        /// Enables or disables the built-in console output.
        void SetConsoleEnabled(bool enabled);

        /// Returns whether the built-in console output is enabled.
        bool IsConsoleEnabled() const;

        /// Opens a file that receives every emitted record. Returns false on failure.
        bool SetLogFile(const std::filesystem::path &path, bool append = false);

        /// Closes the current log file.
        void ClearLogFile();

        /// Flushes the console and log file.
        void Flush();

    private:
        Logger();
        ~Logger();

        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP

} // namespace BlackLight::Engine

/// Emits one message at the requested level.
#define BL_LOG(level, category, message) \
    ::BlackLight::Engine::Logger::Get().Log((level), (message), (category))

/// Starts a streaming message at the requested level.
#define BL_LOG_STREAM(level, category) \
    ::BlackLight::Engine::Logger::Get().Stream((level), (category))

/// Starts a trace-level streaming message.
#define BL_LOG_TRACE(category) \
    BL_LOG_STREAM(::BlackLight::Engine::LogLevel::Trace, (category))

/// Starts a debug-level streaming message.
#define BL_LOG_DEBUG(category) \
    BL_LOG_STREAM(::BlackLight::Engine::LogLevel::Debug, (category))

/// Starts an info-level streaming message.
#define BL_LOG_INFO(category) \
    BL_LOG_STREAM(::BlackLight::Engine::LogLevel::Info, (category))

/// Starts a warning-level streaming message.
#define BL_LOG_WARNING(category) \
    BL_LOG_STREAM(::BlackLight::Engine::LogLevel::Warning, (category))

/// Starts an error-level streaming message.
#define BL_LOG_ERROR(category) \
    BL_LOG_STREAM(::BlackLight::Engine::LogLevel::Error, (category))

/// Starts a critical-level streaming message.
#define BL_LOG_CRITICAL(category) \
    BL_LOG_STREAM(::BlackLight::Engine::LogLevel::Critical, (category))
