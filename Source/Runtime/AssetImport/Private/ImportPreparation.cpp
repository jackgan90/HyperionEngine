#include "AssetPublicationInternal.h"
#include "Hyperion/AssetImport/ImageImport.h"
#include "Hyperion/Scene/Model.h"
#include "ImportRules.h"

namespace Hyperion
{
TAsyncResult<FPreparedImport> FAssetImportService::PrepareAsync(std::filesystem::path InSource,
                                                                std::filesystem::path InOutput,
                                                                FAssetImportOptions InOptions)
{
	const auto Source = ImportPath(InSource);
	const auto Output = Impl->IO.FileSystem()->Normalize(InOutput);
	InOptions.Library = NormalizeImportLibrary(*Impl->IO.FileSystem(), Output, InOptions.Library);
	std::lock_guard Lock(Impl->Mutex);
	if (Impl->bClosing)
	{
		throw std::logic_error("Import service is closing");
	}
	Impl->bStarted = true;
	Impl->Trim();
	if (Impl->Pending.size() >= 256)
	{
		throw std::runtime_error("Import preparation capacity exceeded");
	}
	Impl->Pending.reserve(Impl->Pending.size() + 2);
	auto Result = DispatchAsync<FPreparedImport>(
	    Impl->IO.TaskSystem(), {EDomain::Worker},
	    [State = Impl.get(), Source, Output, Options = std::move(InOptions)]
	    {
		    return State->Prepare(Source, Output, Options);
	    },
	    Impl->Cancellation);
	Impl->Pending.push_back(Result.Task());
	Impl->ScheduleTrim(Result.Task());
	return Result;
}

FPreparedImport FAssetImportService::FImpl::Prepare(const std::filesystem::path& InSource,
                                                    const std::filesystem::path& InOutput,
                                                    const FAssetImportOptions& InOptions)
{
	if (InOptions.bCreateFolder)
	{
		const auto Output = SelectImportFolderOutput(IO, InSource, InOutput);
		auto Options = InOptions;
		Options.bCreateFolder = false;
		Options.Library = Output.parent_path();
		return Prepare(InSource, Output, Options);
	}
	FPreparedImport Result;
	FPublication Publication{IO,
	                         Cancellation,
	                         Importers,
	                         [this, &Result](const auto& InPath, auto InType)
	                         {
		                         auto Asset = Convert(InPath, InType);
		                         Result.Dependencies.emplace(std::make_pair(InPath, std::string(InType)), Asset);
		                         return Asset;
	                         },
	                         InSource,
	                         InOutput};
	Publication.Library = InOptions.Library;
	Publication.SourceRoot = InOptions.SourceRoot.empty() ? std::filesystem::path{} : ImportPath(InOptions.SourceRoot);
	Publication.SourceId = InOptions.SourceId;
	Publication.Prepare(InOptions);
	Publication.LoadLibrary();
	Result.Root = Convert(InSource, Publication.SourceType, InOptions.Conversion);
	if (!InOptions.Name.empty() && ((!InOptions.bScene && Result.Root.Type->CppType == typeid(FModelAsset)) ||
	                                Result.Root.Importer == ImageImporterId))
	{
		auto Record = WriteRecord(*Result.Root.Type, Result.Root.Object.get());
		auto& Fields = RecordFields(Record);
		Fields.at("name") = WriteValue(InOptions.Name);
		Result.Root.Object = ReadRecord(*Result.Root.Type, Record);
	}
	// Dry traversal exercises the real dependency rules and fingerprints, but never commits staged bytes.
	Publication.Build(InSource, Result.Root, true);
	Publication.CheckSources();
	Result.Sources = std::move(Publication.Sources);
	return Result;
}
} // namespace Hyperion
