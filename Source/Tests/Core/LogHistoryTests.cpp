#include "Hyperion/Core/Logging/LogHistory.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>
#include <thread>

namespace
{
void CheckHistory(const std::filesystem::path& InDirectory)
{
	using namespace Hyperion;
	const auto Journal = InDirectory / "History.bin";
	{
		FLogHistory History(Journal);
		History.Append(ELogLevel::Debug, "startup /Game/raw\nsecond line");
		HYP_CHECK(History.Count() == 2);
		std::vector<std::thread> Workers;
		for (unsigned Index = 0; Index < 4; ++Index)
		{
			Workers.emplace_back(
			    [&History, Index]
			    {
				    for (unsigned Row = 0; Row < 250; ++Row)
				    {
					    History.Append(ELogLevel::Info, std::to_string(Index) + ":" + std::to_string(Row));
				    }
			    });
		}
		for (auto& Worker : Workers)
		{
			Worker.join();
		}
		HYP_CHECK(History.Count() == 1002);
		const auto First = History.Read({0, 2});
		HYP_CHECK(First.Entries[0].Level == ELogLevel::Debug && First.Entries[0].Message == "startup /Game/raw");
		HYP_CHECK(First.Next == 2 && First.Total == 1002);
		std::uint64_t Cursor{};
		while (Cursor < History.Count())
		{
			for (const auto& Entry : History.Read({Cursor, 37}).Entries)
			{
				HYP_CHECK(Entry.Sequence == ++Cursor && !Entry.Time.empty());
			}
		}
		const std::string Long = std::string(8191, 'x') + "中文" + std::string(9000, 'y');
		History.Append(ELogLevel::Warning, Long);
		std::string Restored;
		for (const auto& Entry : History.Read({Cursor, 10}).Entries)
		{
			HYP_CHECK(Entry.Level == ELogLevel::Warning && Entry.Message.size() <= 8192);
			Restored += Entry.Message;
		}
		HYP_CHECK(Restored == Long);
		for (const FLogReadRequest Invalid : {FLogReadRequest{0, 0}, {0, 257}, {999999, 1}})
		{
			bool bRejected{};
			try
			{
				History.Read(Invalid);
			}
			catch (const std::invalid_argument&)
			{
				bRejected = true;
			}
			HYP_CHECK(bRejected);
		}
	}
	HYP_CHECK(!std::filesystem::exists(Journal));
	FLogHistory Next(Journal);
	HYP_CHECK(Next.Count() == 0);
}
} // namespace

int main()
{
	try
	{
		CheckHistory(std::filesystem::temp_directory_path() /
		             ("HyperionLogTests-" + std::to_string(Hyperion::ClockNanoseconds())));
		std::cout << "Log history concurrency, paging, UTF-8, early replay and isolation passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
