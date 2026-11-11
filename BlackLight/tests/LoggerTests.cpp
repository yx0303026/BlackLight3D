#include "engine/Logger.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

using BlackLight::Engine::FormatLogRecord;
using BlackLight::Engine::LogLevel;
using BlackLight::Engine::LogRecord;
using BlackLight::Engine::Logger;

namespace {

/// Terminates the test process with the failed expression and source line.
void Check(bool condition, const char* expression, int line) {
	if (!condition) {
		std::fprintf(stderr, "Check failed at line %d: %s\n", line, expression);
		std::abort();
	}
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

/// Restores the process-wide logger settings after a test.
class LoggerSettingsGuard final {
public:
	LoggerSettingsGuard()
		: logger_(Logger::Get()),
		  minimum_level_(logger_.GetMinimumLevel()),
		  console_enabled_(logger_.IsConsoleEnabled()) {
		logger_.SetConsoleEnabled(false);
	}

	~LoggerSettingsGuard() {
		logger_.ClearSinks();
		logger_.ClearLogFile();
		logger_.SetMinimumLevel(minimum_level_);
		logger_.SetConsoleEnabled(console_enabled_);
	}

	LoggerSettingsGuard(const LoggerSettingsGuard&) = delete;
	LoggerSettingsGuard& operator=(const LoggerSettingsGuard&) = delete;

private:
	Logger& logger_;
	LogLevel minimum_level_;
	bool console_enabled_;
};

/// Verifies level comparison, filtering, and the Off switch.
void TestLevelFiltering() {
	LoggerSettingsGuard guard;
	Logger& logger = Logger::Get();

	logger.SetMinimumLevel(LogLevel::Warning);
	CHECK(!logger.IsEnabled(LogLevel::Trace));
	CHECK(!logger.IsEnabled(LogLevel::Info));
	CHECK(logger.IsEnabled(LogLevel::Warning));
	CHECK(logger.IsEnabled(LogLevel::Critical));
	CHECK(!logger.IsEnabled(LogLevel::Off));

	logger.SetMinimumLevel(LogLevel::Off);
	CHECK(!logger.IsEnabled(LogLevel::Debug));
	CHECK(!logger.IsEnabled(LogLevel::Critical));

	logger.SetMinimumLevel(LogLevel::Trace);
	CHECK(logger.IsEnabled(LogLevel::Trace));
	CHECK(BlackLight::Engine::ToString(LogLevel::Warning) == std::string("WARN"));
}

/// Verifies that direct and streaming messages reach custom sinks.
void TestStreamingAndSinks() {
	LoggerSettingsGuard guard;
	Logger& logger = Logger::Get();
	logger.SetMinimumLevel(LogLevel::Info);

	std::vector<LogRecord> records;
	logger.AddSink([&records](const LogRecord& record) { records.push_back(record); });

	logger.Log(LogLevel::Info, "plain message", "Unit");
	logger.Log(LogLevel::Debug, "filtered message", "Unit");
	BL_LOG(LogLevel::Error, "Direct", "macro message");
	BL_LOG_WARNING("Macro") << "value " << 42;

	CHECK(records.size() == 3);
	CHECK(records[0].level == LogLevel::Info);
	CHECK(records[0].category == "Unit");
	CHECK(records[0].message == "plain message");
	CHECK(records[1].level == LogLevel::Error);
	CHECK(records[1].category == "Direct");
	CHECK(records[1].message == "macro message");
	CHECK(records[2].level == LogLevel::Warning);
	CHECK(records[2].category == "Macro");
	CHECK(records[2].message == "value 42");
}

/// Verifies file output and record formatting.
void TestFileAndFormatting() {
	LoggerSettingsGuard guard;
	Logger& logger = Logger::Get();
	logger.SetMinimumLevel(LogLevel::Info);

	const std::filesystem::path path = std::filesystem::temp_directory_path() / "blacklight_logger_test.log";
	std::error_code error;
	std::filesystem::remove(path, error);

	CHECK(logger.SetLogFile(path));
	BL_LOG_ERROR("File") << "written to disk";
	logger.ClearLogFile();

	std::ifstream file(path, std::ios::binary);
	CHECK(file.is_open());
	const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	CHECK(contents.find("[ERROR] [File] written to disk") != std::string::npos);

	LogRecord record;
	record.level = LogLevel::Warning;
	record.category = "Render";
	record.message = "frame dropped";
	const std::string formatted = FormatLogRecord(record);
	CHECK(formatted.find("[WARN] [Render] frame dropped") != std::string::npos);
	CHECK(!formatted.empty() && formatted.back() == '\n');

	std::filesystem::remove(path, error);
}

} // namespace

int main() {
	TestLevelFiltering();
	TestStreamingAndSinks();
	TestFileAndFormatting();
	std::puts("Logger tests passed.");
	return 0;
}
