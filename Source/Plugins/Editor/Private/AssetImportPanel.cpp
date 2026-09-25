#include "AssetImportPanel.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Platform/FileDialog.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
namespace
{
std::string Extension(const std::string& InPath)
{
	auto Result = PathToUtf8(PathFromUtf8(InPath).extension());
	std::transform(Result.begin(), Result.end(), Result.begin(),
	               [](unsigned char InValue)
	               {
		               return static_cast<char>(std::tolower(InValue));
	               });
	return Result;
}

bool IsImage(const std::string& InPath)
{
	const auto Ext = Extension(InPath);
	return Ext == ".png" || Ext == ".jpg" || Ext == ".jpeg";
}

void BakeSize(FGui& InGui, const char* InLabel, std::uint32_t& OutValue)
{
	std::uint64_t Value = OutValue;
	InGui.BeginPropertyRow(InLabel);
	if (InGui.InputInteger("##Value", Value))
	{
		OutValue = static_cast<std::uint32_t>(std::min<std::uint64_t>(Value, UINT32_MAX));
	}
	InGui.EndPropertyRow();
}

FVec4 TextRow(FGui& InGui, const char* InLabel, std::string& OutValue, bool bInPath = false)
{
	InGui.BeginPropertyRow(InLabel);
	InGui.InputText("##Value", OutValue, false, bInPath);
	const auto Bounds = InGui.LastItemBounds();
	InGui.EndPropertyRow();
	return Bounds;
}
} // namespace

FAssetImportPanel::FAssetImportPanel(FAssetImportWorkspace* InImports, FContentRootService& InRoots)
    : Imports(InImports), Roots(InRoots)
{
	Request.Library = "/Game";
	Request.Generation = Roots.Info().Generation;
}

void FAssetImportPanel::Show()
{
	bOpen = bFocus = true;
}

void FAssetImportPanel::UpdateSource()
{
	if (PreviousSource == Request.Source)
	{
		return;
	}
	const auto Suggested = "/Game/" + PathToUtf8(PathFromUtf8(Request.Source).stem()) + ".hasset";
	if (Request.Output.empty() || Request.Output == SuggestedOutput)
	{
		Request.Output = Suggested;
	}
	SuggestedOutput = Suggested;
	PreviousSource = Request.Source;
	Message.clear();
	bAutoPrepare = true;
}

FImportRequest FAssetImportPanel::Snapshot() const
{
	auto Result = Request;
	const auto Formats = FAssetImportWorkspace::Capabilities().Formats;
	Result.Type = TypeIndex ? Formats.at(TypeIndex - 1).Type : "";
	const auto Ext = Extension(Result.Source);
	const bool bModel = TypeIndex == 1 || (TypeIndex == 0 && (Ext == ".gltf" || Ext == ".glb"));
	Result.bScene = bModel && Request.bScene;
	if (!bModel && !IsImage(Result.Source))
	{
		Result.Name.clear();
	}
	Result.TextureEncoding =
	    IsImage(Result.Source) ? std::optional(static_cast<EMaterialTextureEncoding>(EncodingIndex)) : std::nullopt;
	Result.Sky = bBakeSettings ? std::optional(Bake) : std::nullopt;
	return Result;
}

void FAssetImportPanel::Process(FNativeSurface InOwner)
{
	const auto Root = Roots.Info();
	if (Request.Generation != Root.Generation)
	{
		Request.Generation = Root.Generation;
		Message.clear();
		DraftId.clear();
		PreparedKey.clear();
		PreviewInfo = {};
		PreviewOffset = 0;
		SelectedNode = SelectedPrimitive = 0;
		bRefreshDraft = false;
		bAutoPrepare = !Request.Source.empty();
	}
	if (!std::exchange(bBrowse, false))
	{
		ProcessDraft();
		return;
	}
	try
	{
		const std::array Filters{
		    FFileDialogFilter{"Supported assets", "*.gltf;*.glb;*.png;*.jpg;*.jpeg;*.hdr;*.exr;*.json;*.hasset"},
		    FFileDialogFilter{"Models", "*.gltf;*.glb"}, FFileDialogFilter{"Textures", "*.png;*.jpg;*.jpeg"},
		    FFileDialogFilter{"Sky panoramas", "*.hdr;*.exr"}, FFileDialogFilter{"JSON / Native", "*.json;*.hasset"}};
		if (const auto Selected = SelectFile(InOwner, PathFromUtf8(Request.Source), Filters))
		{
			Request.Source = PathToUtf8(*Selected);
			UpdateSource();
		}
	}
	catch (const std::exception& Failure)
	{
		Message = Failure.what();
	}
	bSourceEditing = false;
	ProcessDraft();
}

void FAssetImportPanel::DrawSource(FGui& InGui)
{
	InGui.Text("Source");
	const std::array<std::string, 6> Types{
	    "Auto detect", "Model", "Texture", "Sky", "Material (JSON / Native)", "Scene (JSON / Native)"};
	InGui.BeginPropertyRow("Asset type");
	if (InGui.Combo("##Type", Types, TypeIndex))
	{
		Request.bScene = false;
		bBakeSettings = false;
	}
	InGui.EndPropertyRow();
	InGui.BeginLiveEdit();
	SourceBounds = TextRow(InGui, "Source file", Request.Source, true);
	bSourceEditing = InGui.EndLiveEdit().ActiveInteraction != 0;
	UpdateSource();
	if (InGui.Button("Browse..."))
	{
		bBrowse = true;
	}
	InGui.TextWrapped(
	    "glTF / GLB models, PNG / JPEG textures, 2:1 HDR / EXR skies, typed JSON / sky recipes and native upgrades.");
}

void FAssetImportPanel::DrawSettings(FGui& InGui)
{
	InGui.Separator();
	InGui.Text("Conversion settings");
	const auto Ext = Extension(Request.Source);
	const bool bImage = IsImage(Request.Source);
	const bool bModel = TypeIndex == 1 || (TypeIndex == 0 && (Ext == ".gltf" || Ext == ".glb"));
	const bool bSky = TypeIndex == 3 || (TypeIndex == 0 && (Ext == ".hdr" || Ext == ".exr"));
	if (bModel)
	{
		InGui.Checkbox("Create scene containing the model", Request.bScene);
		if (Request.bScene)
		{
			TextRow(InGui, "Scene model node name", Request.Name);
		}
	}
	if (bImage)
	{
		const std::array<std::string, 2> Encodings{"Linear (data / normal maps)", "sRGB (color images)"};
		InGui.BeginPropertyRow("Color encoding");
		InGui.Combo("##Encoding", Encodings, EncodingIndex);
		InGui.EndPropertyRow();
		InGui.Text("RGBA8 / Texture2D / full mip chain");
	}
	if (bSky && Ext != ".hasset")
	{
		InGui.Checkbox("Override sky bake settings", bBakeSettings);
		if (bBakeSettings)
		{
			BakeSize(InGui, "Radiance face size", Bake.RadianceSize);
			BakeSize(InGui, "Specular face size", Bake.SpecularSize);
			BakeSize(InGui, "Samples", Bake.Samples);
			InGui.TextWrapped(
			    "Power-of-two sizes: radiance 1-1024; specular 1-256, no larger than radiance. Samples 1-1024.");
		}
		else
		{
			InGui.TextWrapped("Use source recipe settings, or defaults: radiance 256, specular 64, samples 256.");
		}
	}
	else
	{
		bBakeSettings = false;
	}
	if (!bModel && !bImage && !bSky)
	{
		InGui.TextWrapped("Keep source properties. Select Sky for a JSON sky recipe with custom bake settings.");
	}
}

void FAssetImportPanel::DrawOutput(FGui& InGui)
{
	InGui.Separator();
	InGui.Text("Output");
	OutputBounds = TextRow(InGui, "Asset path (.hasset)", Request.Output, true);
	TextRow(InGui, "Dependency library", Request.Library, true);
	const auto Root = Roots.Info();
	InGui.TextWrapped("Game directory: " + (Root.Directory.empty() ? std::string("not selected") : Root.Directory));
	InGui.Checkbox("Advanced options", bAdvanced);
	if (bAdvanced)
	{
		InGui.Checkbox("Force reimport (skip freshness check)", Request.bForce);
		TextRow(InGui, "Source root (optional pair)", Request.SourceRoot, true);
		TextRow(InGui, "Logical source ID", Request.SourceId);
		TextRow(InGui, "Root ID (reconstruction)", Request.RootId);
		InGui.TextWrapped("Source root and ID are a pair. Keep root ID empty for normal import. Existing identity and "
		                  "write checks always apply.");
	}
	const bool bWritable = !Root.Directory.empty() && !Root.bReadOnly && Request.Output.starts_with("/Game/") &&
	                       (Request.Library == "/Game" || Request.Library.starts_with("/Game/"));
	if (!bWritable)
	{
		InGui.TextWrapped(
		    "Select a writable Game root through File > Open..., and use /Game paths for output and dependencies.");
	}
	InGui.BeginDisabled(!bWritable || !Imports);
	try
	{
		if (InGui.Button("Validate"))
		{
			(void)Imports->Validate(Snapshot());
			Message = "Input valid. Full conversion and dependency checks run when importing.";
		}
		InGui.SameLine();
		const bool bReady = !DraftId.empty() && PreviewInfo.Status == "ready" && PreparedKey == RequestKey();
		InGui.BeginDisabled(!bReady);
		const bool bSubmit = InGui.Button("Import");
		ImportBounds = InGui.LastItemBounds();
		InGui.EndDisabled();
		if (bSubmit)
		{
			(void)Imports->SubmitDraft({DraftId, PreviewInfo.Generation});
			Message = "Import started. See the result below.";
		}
		InGui.SameLine();
		if (InGui.Button(PreviewInfo.bDirty ? "Discard changes and refresh" : "Update preview"))
		{
			bRefreshDraft = true;
		}
	}
	catch (const std::exception& Failure)
	{
		Message = Failure.what();
	}
	InGui.EndDisabled();
	InGui.SameLine();
	if (InGui.Button("Close panel"))
	{
		bOpen = false;
	}
	CloseBounds = InGui.LastItemBounds();
	InGui.TextWrapped("Accepted imports continue when this panel closes. Published files are not part of scene Undo.");
	InGui.TextWrapped(Message);
	if (!DraftId.empty() && PreparedKey != RequestKey())
	{
		InGui.TextWrapped("Preview is stale: source or settings changed. Update preview before importing.");
	}
	if (!DraftId.empty() && InGui.Button("Show property preview"))
	{
		bPreviewOpen = bPreviewFocus = true;
	}
}

void FAssetImportPanel::DrawTasks(FGui& InGui)
{
	InGui.Separator();
	InGui.Text("Recent imports (GUI and automation)");
	if (!Imports)
	{
		InGui.TextWrapped("Import service unavailable.");
		return;
	}
	for (const auto& Task : Imports->List().Tasks)
	{
		InGui.TextWrapped(Task.Status + " | " + Task.Output);
		if (Task.Result)
		{
			InGui.TextWrapped((Task.Result->bUpToDate ? std::string("Up to date") : std::string("Published")) +
			                  " | written assets: " + std::to_string(Task.Result->WrittenAssets));
			if (!Task.Result->Warning.empty())
			{
				InGui.TextWrapped(Task.Result->Warning);
			}
		}
		if (!Task.Error.empty())
		{
			InGui.TextWrapped(Task.Error);
		}
	}
}

void FAssetImportPanel::Draw(FGui& InGui)
{
	if (bOpen)
	{
		if (InGui.BeginWindow("Import Asset", bOpen, {660, 720}))
		{
			// Import fields distinguish external paths and package destinations; show both in full.
			InGui.SetPathDisplayRoot({});
			if (std::exchange(bFocus, false))
			{
				InGui.FocusWindow("Import Asset");
			}
			DrawSource(InGui);
			DrawSettings(InGui);
			DrawOutput(InGui);
			DrawTasks(InGui);
			InGui.SetPathDisplayRoot("/Game");
		}
		InGui.EndWindow();
	}
	DrawPreview(InGui);
}
} // namespace Hyperion
