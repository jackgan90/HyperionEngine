#pragma once
#include "Hyperion/Core/Core.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Hyperion
{
struct FLogEntry
{
	std::uint64_t Sequence{};
	std::string Time;
	std::uint64_t Thread{};
	ELogLevel Level = ELogLevel::Info;
	std::string Source;
	std::string Message;
};

struct FLogReadRequest
{
	std::uint64_t After{};
	std::uint32_t Limit = 100;
};

struct FLogPage
{
	std::vector<FLogEntry> Entries;
	std::uint64_t Next{};
	std::uint64_t Total{};
};

// Thread-safe current-run history. Text lives in a temporary journal; only row offsets stay resident.
class FLogHistory
{
public:
	explicit FLogHistory(const std::filesystem::path& InJournal);
	~FLogHistory();
	void Append(ELogLevel InLevel, std::string_view InMessage, std::string_view InSource = "engine");
	FLogPage Read(const FLogReadRequest& InRequest) const;
	std::uint64_t Count() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

// Receives formatted engine output, bypassing captured stdout/stderr to prevent duplicate history.
using FLogOutput = std::function<void(std::string_view, bool)>;
void InitializeEditorLog(const std::filesystem::path& InFile, std::shared_ptr<FLogHistory> InHistory,
                         FLogOutput InOutput);
void LogStandardOutput(bool bInError, std::string_view InMessage);
const char* LogLevelName(ELogLevel InLevel);
} // namespace Hyperion
