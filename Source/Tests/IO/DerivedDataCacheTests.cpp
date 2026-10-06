#include "Hyperion/Core/Core.h"
#include "Hyperion/DerivedDataCache/DerivedDataCache.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace Hyperion;

namespace
{
void CheckRecords(const std::filesystem::path& InRoot)
{
	FDerivedDataCache Store(InRoot, "Test");
	const auto Key = DerivedDataDigest("input-v1");
	HYP_CHECK(!Store.Read(Key));
	HYP_CHECK(Store.Publish(Key, "payload"));
	HYP_CHECK(Store.Read(Key) == "payload");
	std::ofstream(Store.RecordPath(Key), std::ios::binary | std::ios::trunc) << "broken";
	HYP_CHECK(!Store.Read(Key));
	HYP_CHECK(Store.Publish(Key, "rebuilt"));
	HYP_CHECK(Store.Read(Key) == "rebuilt");
	const auto Other = DerivedDataDigest("other-input");
	std::filesystem::copy_file(Store.RecordPath(Key), Store.RecordPath(Other));
	HYP_CHECK(!Store.Read(Other));
	const auto Blocked = InRoot / "Blocked";
	std::ofstream(Blocked) << "file-instead-of-directory";
	FDerivedDataCache Unwritable(Blocked, "Test");
	HYP_CHECK(!Unwritable.Publish(Key, "usable-generated-data"));
	HYP_CHECK(!Unwritable.Read(Key));
	bool bRejected = false;
	try
	{
		(void)Store.RecordPath("../../escape");
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckConcurrentRecords(const std::filesystem::path& InRoot)
{
	FDerivedDataCache Store(InRoot, "Concurrent");
	const auto Key = DerivedDataDigest("shared-input");
	const std::string Payload(32768, 'x');
	std::atomic<bool> bWrong{false};
	std::atomic<unsigned> Published{};
	std::vector<std::thread> Workers;
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		Workers.emplace_back(
		    [&]
		    {
			    FDerivedDataCache Other(InRoot, "Concurrent");
			    for (unsigned Attempt = 0; Attempt < 30; ++Attempt)
			    {
				    if (Other.Publish(Key, Payload))
				    {
					    ++Published;
				    }
				    if (const auto Read = Store.Read(Key); Read && *Read != Payload)
				    {
					    bWrong = true;
				    }
			    }
		    });
	}
	for (auto& Worker : Workers)
	{
		Worker.join();
	}
	HYP_CHECK(Published > 0 && !bWrong && Store.Read(Key) == Payload);
}

void CheckMaintenance(const std::filesystem::path& InRoot)
{
	FDerivedDataCachePolicy Policy;
	Policy.MaximumBytes = 1;
	Policy.MaintenanceScanLimit = 2;
	FDerivedDataCache Store(InRoot, "Maintenance", Policy);
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		HYP_CHECK(Store.Publish(DerivedDataDigest(std::to_string(Index)), "discardable"));
	}
	const auto Foreign = Store.RecordPath(DerivedDataDigest("probe")).parent_path() / "User.txt";
	std::ofstream(Foreign) << "keep";
	for (unsigned Index = 0; Index < 10; ++Index)
	{
		const auto Result = Store.Maintain();
		HYP_CHECK(Result.Scanned <= 2);
	}
	HYP_CHECK(std::filesystem::exists(Foreign));
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		HYP_CHECK(!Store.Read(DerivedDataDigest(std::to_string(Index))));
	}
}

void CheckBatchedBudget(const std::filesystem::path& InRoot)
{
	FDerivedDataCachePolicy Policy;
	Policy.MaximumBytes = 500;
	Policy.MaintenanceScanLimit = 2;
	FDerivedDataCache Store(InRoot, "Budget", Policy);
	for (unsigned Index = 0; Index < 10; ++Index)
	{
		HYP_CHECK(Store.Publish(DerivedDataDigest(std::to_string(Index)), "payload"));
	}
	for (unsigned Index = 0; Index < 10; ++Index)
	{
		HYP_CHECK(Store.Maintain().Scanned <= 2);
	}
	std::uint64_t Bytes{};
	for (const auto& Entry :
	     std::filesystem::directory_iterator(Store.RecordPath(DerivedDataDigest("0")).parent_path()))
	{
		Bytes += Entry.file_size();
	}
	HYP_CHECK(Bytes <= Policy.MaximumBytes);
}
} // namespace

int main(int InCount, char** InValues)
{
	try
	{
		if (InCount == 3 && std::string_view(InValues[1]) == "--concurrent")
		{
			CheckConcurrentRecords(InValues[2]);
			return 0;
		}
		const auto Root = std::filesystem::absolute("derived-data-tests") / std::to_string(ClockNanoseconds());
		CheckRecords(Root / "Records");
		CheckConcurrentRecords(Root / "Concurrent");
		CheckMaintenance(Root / "Maintenance");
		CheckBatchedBudget(Root / "Budget");
		HYP_CHECK(DerivedDataDigest("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
		std::cout << "Derived-data cache tests passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
