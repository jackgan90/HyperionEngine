#include "AssetImportPanel.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Platform/FileDialog.h"
#include "ImportChoices.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
namespace
{
std::optional<std::filesystem::path> RelativeOutputDirectory(const std::filesystem::path& InDirectory,
                                                             const std::filesystem::path& InRoot)
{
	std::error_code Error;
	if (InDirectory.empty() || InRoot.empty() || !std::filesystem::is_directory(InDirectory, Error))
	{
		return {};
	}
	const auto Relative = std::filesystem::relative(InDirectory, InRoot, Error);
	if (Error || Relative.empty() || Relative.is_absolute())
	{
		return {};
	}
	for (const auto& Part : Relative)
	{
		if (Part == "..")
		{
			return {};
		}
	}
	return Relative;
}

void BakeSize(FGui& InGui, const char* InLabel, std::uint32_t& OutValue, const char* InTooltip)
{
	std::uint64_t Value = OutValue;
	InGui.BeginPropertyRow(InLabel, nullptr, InTooltip);
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

FAssetImportPanel::FAssetImportPanel(FAssetImportWorkspace* InImports, FContentRootService& InRoots,
                                     FEditorPreferences& InPreferences, std::function<void()> InSavePreferences)
    : Imports(InImports), Roots(InRoots), Preferences(InPreferences), SavePreferences(std::move(InSavePreferences))
{
	Request.Generation = Roots.Info().Generation;
}

void FAssetImportPanel::Show()
{
	bOpen = bFocus = true;
	ValidatedOutputKey.clear();
	bAutoPrepare |= PreviewInfo.Status == EImportDraftState::Failed || !PreviewInfo.Error.empty() || !Message.empty() ||
	                !OutputError.empty();
}

void FAssetImportPanel::UpdateSource()
{
	if (PreviousSource == Request.Source)
	{
		return;
	}
	const auto Directory =
	    RelativeOutputDirectory(Preferences.ImportOutputDirectory, PathFromUtf8(Roots.Info().Directory));
	const auto Suggested = PathToUtf8((PathFromUtf8("/Game") / Directory.value_or(std::filesystem::path{}) /
	                                   (PathToUtf8(PathFromUtf8(Request.Source).stem()) + ".hasset"))
	                                      .lexically_normal());
	Request.Output = Suggested;
	PreviousSource = Request.Source;
	Message.clear();
	bAutoPrepare = true;
}

FImportRequest FAssetImportPanel::Snapshot() const
{
	auto Result = ImportSettingsSnapshot(Request, ImportTypeChoices(FAssetImportWorkspace::Capabilities()),
	                                     static_cast<EMaterialTextureEncoding>(EncodingIndex), Bake);
	Result.bCreateFolder = true;
	return Result;
}

void FAssetImportPanel::Process(FNativeSurface InOwner)
{
	ProcessImportResult();
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
	if (std::exchange(bBrowseOutput, false))
	{
		try
		{
			BrowseOutput(InOwner);
		}
		catch (const std::exception& Failure)
		{
			Message = Failure.what();
		}
	}
	if (!std::exchange(bBrowse, false))
	{
		ProcessDraft();
		return;
	}
	try
	{
		const auto Filters =
		    ImportSourceFilters(ImportTypeChoices(FAssetImportWorkspace::Capabilities()), Request.Type);
		if (const auto Selected = SelectFile(InOwner, PathFromUtf8(Request.Source), Filters))
		{
			Request.Source = PathToUtf8(*Selected);
			UpdateSource();
			// Explicit selection also retries the same path after an external edit or repair.
			ValidatedOutputKey.clear();
			bAutoPrepare = true;
		}
	}
	catch (const std::exception& Failure)
	{
		Message = Failure.what();
	}
	bSourceEditing = false;
	ProcessDraft();
}

void FAssetImportPanel::BrowseOutput(FNativeSurface InOwner)
{
	const auto Root = Roots.Info();
	if (Root.Directory.empty() || Root.bReadOnly)
	{
		return;
	}
	const auto RootPath = PathFromUtf8(Root.Directory);
	const auto Previous = RelativeOutputDirectory(Preferences.ImportOutputDirectory, RootPath);
	const auto Selected =
	    SelectFolder(InOwner, Previous ? Preferences.ImportOutputDirectory : RootPath, "Select asset output directory");
	if (!Selected)
	{
		return;
	}
	SetOutputDirectory(*Selected);
}

void FAssetImportPanel::SetOutputDirectory(const std::filesystem::path& InDirectory)
{
	const auto Root = Roots.Info();
	if (Root.Directory.empty() || Root.bReadOnly)
	{
		throw std::invalid_argument("Select a writable Game asset root before choosing an output directory");
	}
	const auto Relative = RelativeOutputDirectory(InDirectory, PathFromUtf8(Root.Directory));
	if (!Relative)
	{
		throw std::invalid_argument("Choose an output directory inside /Game: " + Root.Directory);
	}
	auto Filename = PathFromUtf8(Request.Output).filename();
	if (Filename.empty())
	{
		Filename = "Untitled.hasset";
	}
	Request.Output = PathToUtf8((PathFromUtf8("/Game") / *Relative / Filename).lexically_normal());
	Preferences.ImportOutputDirectory = InDirectory;
	SavePreferences();
	Message.clear();
	bAutoPrepare = true;
}

void FAssetImportPanel::DrawSource(FGui& InGui)
{
	const auto Choices = ImportTypeChoices(FAssetImportWorkspace::Capabilities());
	std::vector<std::string> Types{"Auto detect"};
	std::size_t TypeIndex{};
	for (const auto& Choice : Choices)
	{
		Types.push_back(Choice.Label);
		if (Choice.Type == Request.Type)
		{
			TypeIndex = Types.size() - 1;
		}
	}
	InGui.BeginPropertyRow("Asset type");
	if (InGui.Combo("##Type", Types, TypeIndex))
	{
		Request.Type = TypeIndex ? Choices.at(TypeIndex - 1).Type : "";
		Request.bScene = false;
	}
	InGui.EndPropertyRow();
	InGui.BeginPropertyRow("Source file", nullptr, "glTF / GLB models, PNG / JPEG textures, and 2:1 HDR / EXR skies.");
	InGui.SetNextItemWidth(std::max(1.f, InGui.AvailableWidth() - 80.f));
	InGui.BeginLiveEdit();
	InGui.InputText("##Value", Request.Source, false, true);
	SourceBounds = InGui.LastItemBounds();
	bSourceEditing = InGui.EndLiveEdit().ActiveInteraction != 0;
	UpdateSource();
	InGui.SameLine();
	if (InGui.Button("Browse..."))
	{
		bBrowse = true;
	}
	InGui.EndPropertyRow();
}

void FAssetImportPanel::DrawSettings(FGui& InGui)
{
	const auto Settings = Snapshot();
	const auto Choices = ImportTypeChoices(FAssetImportWorkspace::Capabilities());
	const auto* Choice = ResolveImportChoice(Choices, Request.Type, Request.Source);
	const bool bImage = Settings.TextureEncoding.has_value();
	const bool bModel = Choice && Choice->Type == RecordType<FModelAsset>().Id;
	const bool bSky = Settings.Sky.has_value();
	if (!bModel && !bImage && !bSky)
	{
		return;
	}
	if (!InGui.Section("Conversion settings"))
	{
		return;
	}
	InGui.Indent();
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
		InGui.BeginDisabled(true);
		std::string MipGeneration = "Full chain";
		TextRow(InGui, "Generate mipmaps", MipGeneration);
		InGui.EndDisabled();
	}
	if (bSky)
	{
		BakeSize(InGui, "Radiance size", Bake.RadianceSize,
		         "Resolution of each sky cubemap face, generated from a 2:1 HDR/EXR panorama. "
		         "Also used to generate diffuse lighting. Must be a power of two from 1 to 1024.");
		BakeSize(InGui, "Specular size", Bake.SpecularSize,
		         "Resolution of each reflection cubemap face, filtered for different surface roughness levels. "
		         "Must be a power of two from 1 to 256, no larger than Radiance size.");
		BakeSize(InGui, "Sample count", Bake.Samples,
		         "Number of samples used to filter reflections. More samples can reduce noise but take longer "
		         "to generate. Must be between 1 and 1024.");
	}
	InGui.Unindent();
}

void FAssetImportPanel::DrawOutputPath(FGui& InGui)
{
	const bool bExpanded = InGui.Section("Output");
	const auto Root = Roots.Info();
	if (bExpanded)
	{
		InGui.Indent();
		const auto OutputTooltip = "/Game -> " +
		                           (Root.Directory.empty() ? std::string("not selected") : Root.Directory) +
		                           "\nDestination folder for all imported assets. Browse selects its parent directory. "
		                           "All imported assets go inside it. Reimport uses the same folder; other name "
		                           "conflicts receive a numeric suffix.";
		InGui.BeginPropertyRow("Save as", nullptr, OutputTooltip.c_str());
		InGui.SetNextItemWidth(std::max(1.f, InGui.AvailableWidth() - 80.f));
		InGui.BeginDisabled(true);
		InGui.InputText("##Value", OutputFolder, false, true);
		InGui.EndDisabled();
		OutputBounds = InGui.LastItemBounds();
		InGui.SameLine();
		InGui.BeginDisabled(Root.Directory.empty() || Root.bReadOnly);
		if (InGui.Button("Browse..."))
		{
			bBrowseOutput = true;
		}
		InGui.EndDisabled();
		InGui.EndPropertyRow();
	}
	const auto Key = RequestKey();
	if (!bSourceEditing && Imports && Key != ValidatedOutputKey)
	{
		ValidatedOutputKey = Key;
		OutputError.clear();
		OutputFolder.clear();
		try
		{
			OutputFolder = Imports->Validate(Snapshot()).Folder;
		}
		catch (const std::exception& Failure)
		{
			OutputError = Failure.what();
		}
	}
	if (bExpanded)
	{
		InGui.Unindent();
	}
}

void FAssetImportPanel::DrawOutput(FGui& InGui)
{
	DrawOutputPath(InGui);
	DrawImportProperties(InGui);
	if (!InGui.Section("Advanced options", false))
	{
		return;
	}
	InGui.Indent();
	InGui.BeginLiveEdit();
	InGui.Checkbox("Force reimport (skip freshness check)", Request.bForce);
	bSettingsEditing |= InGui.EndLiveEdit().ActiveInteraction != 0;
	InGui.Unindent();
}

std::string FAssetImportPanel::ImportMessage() const
{
	if (!Message.empty())
	{
		return Message;
	}
	if (!Request.Output.empty() && !OutputError.empty())
	{
		return OutputError;
	}
	if (!bSourceEditing && PreparedKey == RequestKey() && PreviewInfo.Status == EImportDraftState::Failed)
	{
		return PreviewInfo.Error;
	}
	return {};
}

void FAssetImportPanel::DrawImportAction(FGui& InGui, const std::string& InMessage)
{
	const bool bWritable = OutputError.empty() && ValidatedOutputKey == RequestKey();
	const bool bReady = !DraftId.empty() && PreviewInfo.Status == EImportDraftState::Ready &&
	                    PreparedKey == RequestKey() && ImportTask.empty() && !bImportResultOpen;
	const bool bSubmit = InGui.EndActionLayout("Import", InMessage, bWritable && Imports && bReady);
	ImportBounds = InGui.LastItemBounds();
	if (bSubmit)
	{
		SubmitImport();
	}
}

void FAssetImportPanel::Draw(FGui& InGui)
{
	if (bOpen)
	{
		if (InGui.BeginWindow("Import Asset", bOpen, {660, 720}))
		{
			bSettingsEditing = false;
			CloseBounds = InGui.LastItemBounds();
			CloseBounds.X = CloseBounds.Z - (CloseBounds.W - CloseBounds.Y);
			// Import fields distinguish external paths and package destinations; show both in full.
			InGui.SetPathDisplayRoot({});
			if (std::exchange(bFocus, false))
			{
				InGui.FocusWindow("Import Asset");
			}
			const auto FooterMessage = ImportMessage();
			InGui.BeginActionLayout("ImportLayout", FooterMessage);
			if (InGui.Section("Source"))
			{
				InGui.Indent();
				DrawSource(InGui);
				InGui.Unindent();
			}
			else
			{
				bSourceEditing = false;
			}
			InGui.BeginLiveEdit();
			DrawSettings(InGui);
			bSettingsEditing = InGui.EndLiveEdit().ActiveInteraction != 0;
			DrawOutput(InGui);
			DrawImportAction(InGui, FooterMessage);
			DrawRefreshConfirmation(InGui);
			InGui.SetPathDisplayRoot("/Game");
		}
		InGui.EndWindow();
	}
	DrawImportResult(InGui);
}
} // namespace Hyperion
