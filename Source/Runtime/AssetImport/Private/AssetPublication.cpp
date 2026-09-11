#include "AssetImportInternal.h"
#include "AssetPublicationInternal.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
TAsyncResult<FAssetImportResult> FAssetImportService::ImportAsync(std::filesystem::path InSource,
                                                                  std::filesystem::path InOutput,
                                                                  FAssetImportOptions InOptions)
{
	const auto Source = ImportPath(InSource);
	const auto Output = ImportPath(InOutput);
	if (Source == Output || ImportExtension(Output) != ".hasset")
	{
		throw std::invalid_argument("Import output must be a separate .hasset file");
	}
	std::lock_guard Lock(Impl->Mutex);
	if (Impl->bClosing)
	{
		throw std::logic_error("Import service is closing");
	}
	Impl->bStarted = true;
	Impl->Trim();
	if (Impl->Pending.size() >= 256)
	{
		throw std::runtime_error("Import publication in-flight limit exceeded");
	}
	std::vector<FTaskHandle> Prerequisites;
	if (const auto It = Impl->Publications.find(Output); It != Impl->Publications.end())
	{
		Prerequisites.push_back(It->second);
	}
	Impl->Pending.reserve(Impl->Pending.size() + 2);
	auto Result = DispatchAsync<FAssetImportResult>(
	    Impl->IO.TaskSystem(), {EDomain::Worker},
	    [State = Impl.get(), Source, Output, Options = std::move(InOptions), Prerequisites]
	    {
		    for (const auto& Task : Prerequisites)
		    {
			    try
			    {
				    State->IO.TaskSystem().Wait(Task);
			    }
			    catch (...)
			    {
			    }
		    }
		    return State->Publish(Source, Output, Options);
	    },
	    Impl->Cancellation);
	Impl->Pending.push_back(Result.Task());
	Impl->Publications[Output] = Result.Task();
	Impl->ScheduleTrim(Result.Task());
	return Result;
}

FAssetImportResult FAssetImportService::FImpl::Publish(const std::filesystem::path& InSource,
                                                       const std::filesystem::path& InOutput,
                                                       const FAssetImportOptions& InOptions)
{
	const auto Lease = IO.AcquireWriteLeaseAsync(InOutput, Cancellation).Get(IO.TaskSystem());
	FPublication Publication{IO,
	                         Cancellation,
	                         Importers,
	                         [this](const auto& InPath, auto InType)
	                         {
		                         return Convert(InPath, InType);
	                         },
	                         InSource,
	                         InOutput};
	Publication.Prepare(InOptions);
	if (!InOptions.bForce && Publication.IsCurrent())
	{
		return {Publication.Previous->Header, InOutput, 0, true};
	}
	auto Converted = Convert(InSource, Publication.SourceType);
	if (InOptions.bScene && Converted.Type->CppType == typeid(FModelAsset))
	{
		Publication.Converted.emplace(std::make_pair(InSource, Converted.Type->Id), Converted);
		FSceneManifest Scene;
		Scene.Assets.push_back({"model", {"", ImportPathString(InSource.filename()), Converted.Type->Id, ""}});
		Scene.Instances.push_back({"instance", "model", Identity(), true, InOptions.Name});
		Converted.Type = std::make_shared<const FRecordDescriptor>(RecordType<FSceneManifest>());
		Converted.Object = std::make_shared<const FSceneManifest>(std::move(Scene));
		Converted.NativeHeader.reset();
	}
	if (!InOptions.Name.empty() && Converted.Type->CppType == typeid(FModelAsset))
	{
		auto Model = std::make_shared<FModelAsset>(*std::static_pointer_cast<const FModelAsset>(Converted.Object));
		Model->Name = InOptions.Name;
		Converted.Object = std::move(Model);
	}
	Publication.Build(InSource, Converted, true);
	return {Publication.Root->Header, InOutput, Publication.Written, false};
}
} // namespace Hyperion
