#include "Adapters/ProcessStatistics.h"
#include "AssetCommands.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include <chrono>
#include <iostream>

int main(int InCount, char** InArguments)
{
	using namespace Hyperion;
	try
	{
		std::vector<std::string_view> Arguments;
		std::filesystem::path MountConfiguration;
		bool bAuthoring = false;
		for (int Index = 1; Index < InCount; ++Index)
		{
			if (std::string_view(InArguments[Index]) == "--mounts" && Index + 1 < InCount)
			{
				MountConfiguration = PathFromUtf8(InArguments[++Index]);
				continue;
			}
			if (std::string_view(InArguments[Index]) == "--authoring")
			{
				bAuthoring = true;
				continue;
			}
			Arguments.emplace_back(InArguments[Index]);
		}
		FTaskSystem Tasks(2, 1);
		std::shared_ptr<IFileSystem> Files =
		    MountConfiguration.empty() ? std::static_pointer_cast<IFileSystem>(std::make_shared<FLocalFileSystem>())
		                               : LoadContentMounts(MountConfiguration, bAuthoring);
		FIOService IO(Tasks, Files);
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
