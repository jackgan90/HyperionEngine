#include "AssetImportInternal.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Content/ContentPaths.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/Model.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "ImportRules.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
void CheckWritePath(FMountedFileSystem& InFiles, const std::filesystem::path& InPath, const FContentRootInfo& InRoot)
{
	const auto Text = PathToUtf8(InPath);
	if (!IsGameContentPath(Text))
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments,
		                        "Save the asset and its dependencies inside the current /Game root");
	}
	const auto Physical = std::filesystem::weakly_canonical(InFiles.Resolve(InPath, true));
	const auto Relative = std::filesystem::relative(Physical, PathFromUtf8(InRoot.Directory));
	if (Relative.empty() || Relative.is_absolute() ||
	    std::find(Relative.begin(), Relative.end(), std::filesystem::path("..")) != Relative.end())
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments,
		                        "Output directory resolves outside the current /Game root");
	}
}
} // namespace

void FAssetImportWorkspace::ValidateOutput(const FImportRequest& InRequest) const
{
	RequireMain();
	const auto Root = Roots.Info();
	if (InRequest.Generation != Root.Generation)
	{
		throw FAssetImportError(AssetImportErrors::StaleRevision, "Content root changed before import");
	}
	try
	{
		RequireGameContentRoot(Root, "Select a Game asset root through File > Open before choosing an output");
	}
	catch (const FContentRootError& Failure)
	{
		throw FAssetImportError(Failure.Code, Failure.what());
	}
	if (Root.bReadOnly)
	{
		throw FAssetImportError(AssetImportErrors::ReadOnly, "Game content is read-only");
	}
	for (const auto* Text : {&InRequest.Output, &InRequest.Library})
	{
		if (Text->size() > 4096 || Text->find('\0') != std::string::npos)
		{
			throw FAssetImportError(AssetImportErrors::InvalidArguments,
			                        "Output paths must be at most 4096 bytes without NUL");
		}
	}
	if (InRequest.Output.empty())
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments, "Enter an output .hasset path under /Game");
	}
	try
	{
		auto& Files = *IO.FileSystem();
		const auto Output = Files.Normalize(PathFromUtf8(InRequest.Output));
		const auto Library = NormalizeImportLibrary(Files, Output, PathFromUtf8(InRequest.Library));
		if (!IsImportAssetOutput(Output))
		{
			throw FAssetImportError(AssetImportErrors::InvalidArguments, "Output filename must end with .hasset");
		}
		if (auto* Mounted = dynamic_cast<FMountedFileSystem*>(&Files))
		{
			CheckWritePath(*Mounted, Output, Root);
			CheckWritePath(*Mounted, Library, Root);
		}
	}
	catch (const FAssetImportError&)
	{
		throw;
	}
	catch (const std::exception& Failure)
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments, Failure.what());
	}
}

FImportValidation FAssetImportWorkspace::Validate(const FImportRequest& InRequest) const
{
	ValidateOutput(InRequest);
	if (InRequest.bCreateFolder && !InRequest.Library.empty())
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments,
		                        "Grouped imports cannot specify a dependency library");
	}
	for (const auto* Text : {&InRequest.Source, &InRequest.Output, &InRequest.Library, &InRequest.Name, &InRequest.Type,
	                         &InRequest.SourceRoot, &InRequest.SourceId, &InRequest.RootId})
	{
		if (Text->size() > 4096 || Text->find('\0') != std::string::npos)
		{
			throw FAssetImportError(AssetImportErrors::InvalidArguments,
			                        "Import strings must be at most 4096 bytes without NUL");
		}
	}
	if (InRequest.Source.empty() || InRequest.Output.empty())
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments, "Source and output are required");
	}
	auto& Files = *IO.FileSystem();
	const auto Source = ImportPath(PathFromUtf8(InRequest.Source));
	const auto Output = Files.Normalize(PathFromUtf8(InRequest.Output));
	const auto Library = NormalizeImportLibrary(Files, Output, PathFromUtf8(InRequest.Library));
	if (!IsSeparateImportOutput(Files, Source, Output))
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments, "Output must be a separate .hasset file");
	}
	const auto Extension = ImportExtension(Source);
	const auto Importers = Imports.ImporterDescriptors();
	const auto* Importer = FindAssetImporter(Importers, Extension, InRequest.Type, true);
	if (!Importer)
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments,
		                        "No matching source importer for the selected type and extension");
	}
	const auto Type = Importer->Type->Id;
	ValidateImportSettings({InRequest.TextureEncoding, InRequest.Sky}, Importer);
	if (InRequest.bScene && !Type.empty() && Type != RecordType<FModelAsset>().Id &&
	    Type != RecordType<FSceneManifest>().Id)
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments, "Scene wrapping requires a model or scene source");
	}
	if (CheckImportSourceIdentity(!InRequest.SourceRoot.empty(), InRequest.SourceId) !=
	    EImportSourceIdentityError::None)
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments,
		                        "Supply sourceRoot and a portable sourceId together");
	}
	if (!InRequest.RootId.empty() && !IsAssetIdentifier(InRequest.RootId))
	{
		throw FAssetImportError(AssetImportErrors::InvalidArguments,
		                        "Root ID must be 32 lowercase hexadecimal characters");
	}
	if (!Files.Exists(Source))
	{
		throw FAssetImportError(AssetImportErrors::NotFound, "Import source does not exist");
	}
	const auto Destination = InRequest.bCreateFolder ? SelectImportFolderOutput(IO, Source, Output) : Output;
	return {PathToUtf8(Source), PathToUtf8(Output), PathToUtf8(Library), Type, PathToUtf8(Destination.parent_path())};
}
} // namespace Hyperion
