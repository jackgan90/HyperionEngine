#include "Hyperion/DerivedDataCache/DerivedDataCache.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/ApplicationPaths.h"
#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>
#include <charconv>

namespace Hyperion
{
FDerivedDataCache::FDerivedDataCache(std::filesystem::path InRoot, std::string InNamespace,
                                     FDerivedDataCachePolicy InPolicy)
    : Policy(InPolicy)
{
	ValidateStorageIdentity(InNamespace);
	if (InRoot.empty() || IsPackagePath(InRoot))
	{
		throw std::invalid_argument("Derived data requires an explicit local cache root");
	}
	Directory = NormalizeFilePath(InRoot) / InNamespace / "v1";
	if (!Policy.MaximumRecordBytes || !Policy.MaintenanceScanLimit || Policy.MaximumAge.count() < 0)
	{
		throw std::invalid_argument("Invalid derived-data cache policy");
	}
}

std::filesystem::path FDerivedDataCache::RecordPath(std::string_view InKey) const
{
	if (InKey.size() != 64 || !std::all_of(InKey.begin(), InKey.end(),
	                                       [](char InChar)
	                                       {
		                                       return (InChar >= '0' && InChar <= '9') ||
		                                              (InChar >= 'a' && InChar <= 'f');
	                                       }))
	{
		throw std::invalid_argument("Derived-data key must be a lowercase SHA-256 digest");
	}
	return Directory / (std::string(InKey) + ".ddc");
}

void FDerivedDataCache::ReportFailure(std::string_view InOperation, std::string_view InReason)
{
	if (!bReportedFailure.exchange(true))
	{
		try
		{
			Log(ELogLevel::Warning, "Derived-data cache degraded; operation='" + std::string(InOperation) +
			                            "'; directory='" + PathToUtf8(Directory) + "'; reason='" +
			                            std::string(InReason) + "'; generated results remain usable");
		}
		catch (...)
		{
		}
	}
}

std::optional<std::string> FDerivedDataCache::Read(std::string_view InKey)
{
	const auto Path = RecordPath(InKey);
	try
	{
		RequireUnlinkedPath(Path);
		FLocalFileSystem Files;
		if (!Files.Exists(Path))
		{
			return std::nullopt;
		}
		const auto Bytes = Files.Read(Path, Policy.MaximumRecordBytes + 256);
		std::string_view Record(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());
		const std::string Prefix = "HYP-DDC-1\n" + std::string(InKey) + "\n";
		if (!Record.starts_with(Prefix))
		{
			throw std::runtime_error("Unrecognized cache format or key");
		}
		Record.remove_prefix(Prefix.size());
		const auto Separator = Record.find('\n');
		std::size_t Length{};
		if (Separator == std::string_view::npos)
		{
			throw std::runtime_error("Truncated cache length");
		}
		const auto Parsed = std::from_chars(Record.data(), Record.data() + Separator, Length);
		if (Parsed.ec != std::errc{} || Parsed.ptr != Record.data() + Separator || Length > Policy.MaximumRecordBytes)
		{
			throw std::runtime_error("Invalid cache payload length");
		}
		Record.remove_prefix(Separator + 1);
		if (Record.size() != Length + 65 || Record[64] != '\n' ||
		    DerivedDataDigest(Record.substr(65)) != Record.substr(0, 64))
		{
			throw std::runtime_error("Cache payload integrity check failed");
		}
		return std::string(Record.substr(65));
	}
	catch (const std::exception& Error)
	{
		ReportFailure("read", Error.what());
		return std::nullopt;
	}
}

bool FDerivedDataCache::Publish(std::string_view InKey, std::string_view InPayload)
{
	const auto Path = RecordPath(InKey);
	try
	{
		RequireUnlinkedPath(Path);
		if (InPayload.size() > Policy.MaximumRecordBytes)
		{
			throw std::runtime_error("Generated payload exceeds cache record limit");
		}
		std::string Record = "HYP-DDC-1\n" + std::string(InKey) + "\n" + std::to_string(InPayload.size()) + "\n" +
		                     DerivedDataDigest(InPayload) + "\n";
		Record.append(InPayload);
		FLocalFileSystem Files;
		Files.WriteAtomic(Path, std::as_bytes(std::span(Record)));
		return true;
	}
	catch (const std::exception& Error)
	{
		ReportFailure("publish", Error.what());
		return false;
	}
}
} // namespace Hyperion
