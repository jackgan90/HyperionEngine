#include "Adapters/ProcessStatistics.h"
#include "AssetCommands.h"
#include "Hyperion/Content/ContentRootService.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <chrono>
#include <iostream>

int main(int InCount, char** InArguments)
{
	using namespace Hyperion;
	try
	{
		std::vector<std::string_view> Arguments;
		std::filesystem::path EngineContent = std::filesystem::path(HYP_SOURCE_DIR) / "Content";
		std::optional<std::filesystem::path> AssetRoot;
		bool bAuthoring = false;
		bool bReadOnly = false;
		for (int Index = 1; Index < InCount; ++Index)
		{
			const std::string_view Argument = InArguments[Index];
			if (Argument == "--asset-root" || Argument == "--engine-content")
			{
				if (++Index >= InCount || std::string_view(InArguments[Index]).empty())
				{
					throw std::invalid_argument("Missing directory for " + std::string(Argument));
				}
				const auto Directory = PathFromUtf8(InArguments[Index]);
				if (Argument == "--asset-root")
				{
					AssetRoot = Directory;
				}
				else
				{
					EngineContent = Directory;
				}
				continue;
			}
			if (Argument == "--read-only")
			{
				bReadOnly = true;
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
		auto Files = CreateContentFileSystem(EngineContent, bAuthoring);
		FIOService IO(Tasks, Files);
		FAssetService Assets(IO);
		RegisterSceneAssetTypes(Assets.Types());
		FContentRootService Content(Tasks, *Files, Assets);
		if (AssetRoot)
		{
			Content.Change(*AssetRoot, bReadOnly);
		}
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
