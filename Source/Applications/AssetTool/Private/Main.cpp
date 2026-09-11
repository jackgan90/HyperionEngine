#include "Adapters/ProcessStatistics.h"
#include "AssetCommands.h"
#include <chrono>
#include <iostream>

int main(int InCount, char** InArguments)
{
	using namespace Hyperion;
	try
	{
		std::vector<std::string_view> Arguments;
		for (int Index = 1; Index < InCount; ++Index)
		{
			Arguments.emplace_back(InArguments[Index]);
		}
		FTaskSystem Tasks(2, 1);
		FIOService IO(Tasks);
		const auto Started = std::chrono::steady_clock::now();
		RunAssetCommand(Arguments, IO, std::cout);
		const auto Milliseconds =
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Started).count();
		const auto& Stats = IO.Statistics();
		std::cout << "elapsed_ms=" << Milliseconds << " reads=" << Stats.Reads << " read_bytes=" << Stats.ReadBytes
		          << " peak_resident_bytes=" << AssetToolPeakResidentBytes() << " writes=" << Stats.Writes
		          << " written_bytes=" << Stats.WrittenBytes << std::endl;
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "AssetTool: " << Error.what() << '\n';
		return 1;
	}
}
