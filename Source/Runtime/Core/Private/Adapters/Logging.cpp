#include "Hyperion/Core/Logging/LogHistory.h"
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Hyperion
{
namespace
{
std::mutex LogMutex;
std::shared_ptr<FLogHistory> History;
FLogOutput Output;

spdlog::level::level_enum Severity(ELogLevel InLevel)
{
	switch (InLevel)
	{
		case ELogLevel::Error:
			return spdlog::level::err;
		case ELogLevel::Warning:
			return spdlog::level::warn;
		case ELogLevel::Debug:
			return spdlog::level::debug;
		default:
			return spdlog::level::info;
	}
}

void Configure(std::vector<spdlog::sink_ptr> InSinks, bool bInDebug)
{
	auto Logger = std::make_shared<spdlog::logger>("Hyperion", InSinks.begin(), InSinks.end());
	Logger->set_pattern("[%H:%M:%S.%e] [%t] [%l] %v");
	Logger->set_level(bInDebug ? spdlog::level::debug : spdlog::level::info);
	Logger->flush_on(bInDebug ? spdlog::level::debug : spdlog::level::info);
	spdlog::set_default_logger(std::move(Logger));
}

spdlog::sink_ptr FileSink(const std::filesystem::path& InFile)
{
	if (!InFile.parent_path().empty())
	{
		std::filesystem::create_directories(InFile.parent_path());
	}
	return std::make_shared<spdlog::sinks::basic_file_sink_mt>(InFile.string(), false);
}
} // namespace

const char* LogLevelName(ELogLevel InLevel)
{
	switch (InLevel)
	{
		case ELogLevel::Error:
			return "error";
		case ELogLevel::Warning:
			return "warning";
		case ELogLevel::Debug:
			return "debug";
		default:
			return "info";
	}
}

void InitializeLog(const std::filesystem::path& InFile)
{
	std::lock_guard Lock(LogMutex);
	Configure({std::make_shared<spdlog::sinks::stdout_color_sink_mt>(), FileSink(InFile)}, false);
	History.reset();
	Output = {};
}

void InitializeStderrLog()
{
	std::lock_guard Lock(LogMutex);
	Configure({std::make_shared<spdlog::sinks::stderr_color_sink_mt>()}, false);
	spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
	History.reset();
	Output = {};
}

void InitializeEditorLog(const std::filesystem::path& InFile, std::shared_ptr<FLogHistory> InHistory,
                         FLogOutput InOutput)
{
	std::lock_guard Lock(LogMutex);
	Configure({FileSink(InFile)}, true);
	History = std::move(InHistory);
	Output = std::move(InOutput);
}

void Log(ELogLevel InLevel, std::string_view InMessage)
{
	std::lock_guard Lock(LogMutex);
	spdlog::log(Severity(InLevel), "{}", InMessage);
	if (Output)
	{
		Output("[" + std::string(LogLevelName(InLevel)) + "] " + std::string(InMessage) + "\n",
		       InLevel == ELogLevel::Error);
	}
	if (History)
	{
		History->Append(InLevel, InMessage);
	}
}

void LogStandardOutput(bool bInError, std::string_view InMessage)
{
	std::lock_guard Lock(LogMutex);
	const auto Level = bInError ? ELogLevel::Error : ELogLevel::Info;
	spdlog::log(Severity(Level), "{}", InMessage);
	if (History)
	{
		History->Append(Level, InMessage, bInError ? "stderr" : "stdout");
	}
}

void ShutdownLog()
{
	std::lock_guard Lock(LogMutex);
	spdlog::shutdown();
	Output = {};
	History.reset();
}
} // namespace Hyperion
