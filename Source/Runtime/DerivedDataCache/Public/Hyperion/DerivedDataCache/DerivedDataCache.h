#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace Hyperion
{
std::string DerivedDataDigest(std::string_view InBytes);

struct FDerivedDataCachePolicy
{
	std::uint64_t MaximumBytes = 8ull * 1024 * 1024 * 1024;
	std::size_t MaximumRecordBytes = 256 * 1024 * 1024;
	std::chrono::hours MaximumAge{24 * 30};
	std::size_t MaintenanceScanLimit = 4096;
};

struct FCacheMaintenanceResult
{
	std::size_t Scanned{};
	std::size_t Removed{};
	std::uint64_t ObservedBytes{};
	bool bComplete = true;
};

// Thread-safe immutable records. Semantic key ownership remains with the producer.
class FDerivedDataCache
{
public:
	FDerivedDataCache(std::filesystem::path InRoot, std::string InNamespace, FDerivedDataCachePolicy InPolicy = {});
	std::optional<std::string> Read(std::string_view InKey);
	bool Publish(std::string_view InKey, std::string_view InPayload);
	// Bounded best-effort sweep; repeated calls resume a large namespace. Budget is applied to observed records.
	FCacheMaintenanceResult Maintain();
	std::filesystem::path RecordPath(std::string_view InKey) const;

private:
	std::filesystem::path Directory;
	FDerivedDataCachePolicy Policy;
	std::atomic<bool> bReportedFailure{false};
	std::mutex MaintenanceMutex;
	std::filesystem::directory_iterator MaintenanceCursor;
	std::uint64_t MaintenanceBytes{};
	void ReportFailure(std::string_view InOperation, std::string_view InReason);
};
} // namespace Hyperion
