#include "AssetImportInternal.h"
#include "AssetPublicationInternal.h"
#include "Hyperion/AssetImport/ImageImport.h"
#include "Hyperion/Assets/AssetEntryNames.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "ImportRules.h"

namespace Hyperion
{
TAsyncResult<FAssetImportResult> FAssetImportService::ImportAsync(std::filesystem::path InSource,
                                                                  std::filesystem::path InOutput,
                                                                  FAssetImportOptions InOptions)
{
	const auto Source = ImportPath(InSource);
	ValidateImportSettings(InOptions.Conversion, ImportExtension(Source), InOptions.TypeId);
	const auto Output = Impl->IO.FileSystem()->Normalize(InOutput);
	if (InOptions.bCreateFolder && !InOptions.Library.empty() &&
	    Impl->IO.FileSystem()->Normalize(InOptions.Library) != Output.parent_path())
	{
		throw std::invalid_argument("Grouped imports cannot use a separate dependency library");
	}
	if (!IsSeparateImportOutput(*Impl->IO.FileSystem(), Source, Output))
	{
		throw std::invalid_argument("Import output must be a separate .hasset file");
	}
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
		throw std::runtime_error("Import publication in-flight limit exceeded");
	}
	std::vector<FTaskHandle> Prerequisites;
	if (const auto It = Impl->Publications.find(InOptions.Library); It != Impl->Publications.end())
	{
		Prerequisites.push_back(It->second);
	}
	Impl->Pending.reserve(Impl->Pending.size() + 2);
	const auto PublicationKey = InOptions.Library;
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
	Impl->Publications[PublicationKey] = Result.Task();
	Impl->ScheduleTrim(Result.Task());
	return Result;
}

FAssetImportResult FAssetImportService::FImpl::Publish(const std::filesystem::path& InSource,
                                                       const std::filesystem::path& InOutput,
                                                       const FAssetImportOptions& InOptions)
{
	if (InOptions.bCreateFolder)
	{
		return PublishGrouped(InSource, InOutput, InOptions);
	}
	const auto Lease = IO.AcquireWriteLeaseAsync(InOutput, Cancellation).Get(IO.TaskSystem());
	FPublication Publication{IO,
	                         Cancellation,
	                         Importers,
	                         [this, &InOptions](const auto& InPath, auto InType)
	                         {
		                         if (InOptions.Prepared)
		                         {
			                         return InOptions.Prepared->Dependencies.at({InPath, std::string(InType)});
		                         }
		                         return Convert(InPath, InType);
	                         },
	                         InSource,
	                         InOutput};
	Publication.Library = InOptions.Library;
	Publication.FolderKey = InOptions.FolderKey;
	Publication.SourceRoot = InOptions.SourceRoot.empty() ? std::filesystem::path{} : ImportPath(InOptions.SourceRoot);
	Publication.SourceId = InOptions.SourceId;
	const auto IdentityError = CheckImportSourceIdentity(!Publication.SourceRoot.empty(), Publication.SourceId);
	if (IdentityError == EImportSourceIdentityError::Incomplete)
	{
		throw std::invalid_argument("Source root and source ID must be supplied together");
	}
	if (IdentityError == EImportSourceIdentityError::NonPortable)
	{
		throw std::invalid_argument("Source ID must be a portable logical name");
	}
	const auto LibraryLease =
	    IO.AcquireWriteLeaseAsync(Publication.Library / AssetPublicationLeaseName(), Cancellation).Get(IO.TaskSystem());
	Publication.Prepare(InOptions);
	if (InOptions.Prepared)
	{
		Publication.Sources = InOptions.Prepared->Sources;
		Publication.CheckSources();
	}
	Publication.LoadLibrary();
	if (!InOptions.bForce && Publication.IsCurrent())
	{
		return {Publication.Previous->Header, InOutput, 0, true};
	}
	auto Converted =
	    InOptions.Prepared ? InOptions.Prepared->Root : Convert(InSource, Publication.SourceType, InOptions.Conversion);
	if (InOptions.bScene && Converted.Type->CppType == typeid(FModelAsset))
	{
		Publication.Converted.emplace(std::make_pair(InSource, Converted.Type->Id), Converted);
		FSceneManifest Scene;
		Scene.Assets.push_back({"model", {"", ImportPathString(InSource.filename()), Converted.Type->Id, ""}});
		FSceneNodeEntry Model;
		Model.Id = "instance";
		Model.Name = InOptions.Name.empty() ? "Model" : InOptions.Name;
		Model.Model = FSceneNodeModel{"model"};
		Scene.Nodes.push_back(std::move(Model));
		Scene.Nodes.push_back(SceneEntryFromNode(MakeSceneDirectionalLightNode("light-main")));
		Scene.Nodes.push_back(SceneEntryFromNode(MakeSceneEnvironmentLightNode("light-environment")));
		Converted.Products.clear();
		Converted.Type = std::make_shared<const FRecordDescriptor>(RecordType<FSceneManifest>());
		Converted.Object = std::make_shared<const FSceneManifest>(std::move(Scene));
	}
	if (!InOptions.Prepared && !InOptions.Name.empty() && Converted.Type->CppType == typeid(FModelAsset))
	{
		auto Model = std::make_shared<FModelAsset>(*std::static_pointer_cast<const FModelAsset>(Converted.Object));
		Model->Name = InOptions.Name;
		Converted.Object = std::move(Model);
	}
	if (!InOptions.Prepared && !InOptions.Name.empty() && Converted.Importer == ImageImporterId)
	{
		auto Texture =
		    std::make_shared<FTextureAsset>(*std::static_pointer_cast<const FTextureAsset>(Converted.Object));
		Texture->Name = InOptions.Name;
		Converted.Object = std::move(Texture);
	}
	Publication.Build(InSource, Converted, true);
	Publication.Commit();
	return {Publication.Root->Header, InOutput, Publication.Written, false};
}
} // namespace Hyperion
