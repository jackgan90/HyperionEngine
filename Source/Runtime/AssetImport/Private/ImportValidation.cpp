#include "AssetImportInternal.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/Model.h"
#include "Hyperion/Scene/ModelSource.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
std::string ImportType(const FImportRequest& InRequest, const std::string& InExtension)
{
	const auto Capabilities = FAssetImportWorkspace::Capabilities();
	for (const auto& Format : Capabilities.Formats)
	{
		if ((InRequest.Type.empty() || Format.Type == InRequest.Type) &&
		    std::find(Format.Extensions.begin(), Format.Extensions.end(), InExtension) != Format.Extensions.end())
		{
			return Format.Type;
		}
	}
	// Retain the existing explicit tooling-only glTF source record path.
	if (InRequest.Type == RecordType<FModelSource>().Id && (InExtension == ".gltf" || InExtension == ".glb"))
	{
		return InRequest.Type;
	}
	throw FAssetImportError("invalid_arguments", "No matching source importer for the selected type and extension");
}

void CheckWritePath(FMountedFileSystem& InFiles, const std::filesystem::path& InPath, const FContentRootInfo& InRoot)
{
	const auto Text = PathToUtf8(InPath);
	if (Text != "/Game" && !Text.starts_with("/Game/"))
	{
		throw FAssetImportError("invalid_arguments",
		                        "Save the asset and its dependencies inside the current /Game root");
	}
	const auto Physical = std::filesystem::weakly_canonical(InFiles.Resolve(InPath, true));
	const auto Relative = std::filesystem::relative(Physical, PathFromUtf8(InRoot.Directory));
	if (Relative.empty() || Relative.is_absolute() ||
	    std::find(Relative.begin(), Relative.end(), std::filesystem::path("..")) != Relative.end())
	{
		throw FAssetImportError("invalid_arguments", "Output directory resolves outside the current /Game root");
	}
}
} // namespace

void FAssetImportWorkspace::ValidateOutput(const FImportRequest& InRequest) const
{
	RequireMain();
	const auto Root = Roots.Info();
	if (InRequest.Generation != Root.Generation)
	{
		throw FAssetImportError("stale_revision", "Content root changed before import");
	}
	if (Root.Directory.empty())
	{
		throw FAssetImportError("root_unset", "Select a Game asset root through File > Open before choosing an output");
	}
	if (Root.bReadOnly)
	{
		throw FAssetImportError("read_only", "Game content is read-only");
	}
	for (const auto* Text : {&InRequest.Output, &InRequest.Library})
	{
		if (Text->size() > 4096 || Text->find('\0') != std::string::npos)
		{
			throw FAssetImportError("invalid_arguments", "Output paths must be at most 4096 bytes without NUL");
		}
	}
	if (InRequest.Output.empty())
	{
		throw FAssetImportError("invalid_arguments", "Enter an output .hasset path under /Game");
	}
	try
	{
		auto& Files = *IO.FileSystem();
		const auto Output = Files.Normalize(PathFromUtf8(InRequest.Output));
		const auto Library =
		    Files.Normalize(InRequest.Library.empty() ? Output.parent_path() : PathFromUtf8(InRequest.Library));
		if (ImportExtension(Output) != ".hasset")
		{
			throw FAssetImportError("invalid_arguments", "Output filename must end with .hasset");
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
		throw FAssetImportError("invalid_arguments", Failure.what());
	}
}

FImportValidation FAssetImportWorkspace::Validate(const FImportRequest& InRequest) const
{
	ValidateOutput(InRequest);
	if (InRequest.bCreateFolder && !InRequest.Library.empty())
	{
		throw FAssetImportError("invalid_arguments", "Grouped imports cannot specify a dependency library");
	}
	for (const auto* Text : {&InRequest.Source, &InRequest.Output, &InRequest.Library, &InRequest.Name, &InRequest.Type,
	                         &InRequest.SourceRoot, &InRequest.SourceId, &InRequest.RootId})
	{
		if (Text->size() > 4096 || Text->find('\0') != std::string::npos)
		{
			throw FAssetImportError("invalid_arguments", "Import strings must be at most 4096 bytes without NUL");
		}
	}
	if (InRequest.Source.empty() || InRequest.Output.empty())
	{
		throw FAssetImportError("invalid_arguments", "Source and output are required");
	}
	auto& Files = *IO.FileSystem();
	const auto Source = ImportPath(PathFromUtf8(InRequest.Source));
	const auto Output = Files.Normalize(PathFromUtf8(InRequest.Output));
	const auto Library =
	    Files.Normalize(InRequest.Library.empty() ? Output.parent_path() : PathFromUtf8(InRequest.Library));
	if (Files.Normalize(Source) == Output || ImportExtension(Output) != ".hasset")
	{
		throw FAssetImportError("invalid_arguments", "Output must be a separate .hasset file");
	}
	const auto Extension = ImportExtension(Source);
	const auto Type = ImportType(InRequest, Extension);
	ValidateImportSettings({InRequest.TextureEncoding, InRequest.Sky}, Extension, Type);
	if (InRequest.bScene && !Type.empty() && Type != RecordType<FModelAsset>().Id &&
	    Type != RecordType<FSceneManifest>().Id)
	{
		throw FAssetImportError("invalid_arguments", "Scene wrapping requires a model or scene source");
	}
	if (InRequest.SourceRoot.empty() != InRequest.SourceId.empty() ||
	    InRequest.SourceId.find_first_of(":\\") != std::string::npos || InRequest.SourceId.starts_with('/') ||
	    InRequest.SourceId.find("..") != std::string::npos)
	{
		throw FAssetImportError("invalid_arguments", "Supply sourceRoot and a portable sourceId together");
	}
	if (!InRequest.RootId.empty() && !IsAssetIdentifier(InRequest.RootId))
	{
		throw FAssetImportError("invalid_arguments", "Root ID must be 32 lowercase hexadecimal characters");
	}
	if (!Files.Exists(Source))
	{
		throw FAssetImportError("not_found", "Import source does not exist");
	}
	const auto Destination = InRequest.bCreateFolder ? SelectImportFolderOutput(IO, Source, Output) : Output;
	return {PathToUtf8(Source), PathToUtf8(Output), PathToUtf8(Library), Type, PathToUtf8(Destination.parent_path())};
}
} // namespace Hyperion
