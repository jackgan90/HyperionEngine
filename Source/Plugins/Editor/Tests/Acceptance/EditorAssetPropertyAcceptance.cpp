#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <bit>

namespace Hyperion
{
namespace
{
void RequireAsset(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

const FMaterialAssetValue& AssetValue(const FMaterialAsset& InMaterial, std::string_view InName)
{
	const auto It = std::find_if(InMaterial.Values.begin(), InMaterial.Values.end(),
	                             [&](const auto& InEntry)
	                             {
		                             return InEntry.Name == InName;
	                             });
	if (It == InMaterial.Values.end())
	{
		throw std::runtime_error("Missing acceptance material value");
	}
	return It->Value;
}

float Roughness(const FMaterialAsset& InMaterial)
{
	return std::bit_cast<float>(AssetValue(InMaterial, "Pbr.RoughnessFactor").Words.at(0));
}

void AssetKey(std::vector<FInputEvent>& InEvents, EKey InKey, bool bInDown, unsigned InModifiers = 0)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = InKey;
	Key.bDown = bInDown;
	Key.Modifiers = InModifiers;
	InEvents.push_back(Key);
}

void AssetShortcut(std::vector<FInputEvent>& InEvents, EKey InKey)
{
	AssetKey(InEvents, InKey, true, 1);
	AssetKey(InEvents, InKey, false);
}
} // namespace

void FEditorAcceptanceHarness::ExerciseAssetPropertyInput(std::vector<FInputEvent>& InEvents)
{
	const auto Stage = DescribeAssetAcceptance(Scenario.Asset.Progress.GetState()).Stage;
	if (Scenario.AssetLoggedState != Scenario.Asset.Progress.GetState())
	{
		Scenario.AssetLoggedState = Scenario.Asset.Progress.GetState();
		Log(ELogLevel::Info, "Asset property acceptance step " + Scenario.Asset.Progress.Name());
	}
	if ((Stage == EAssetAcceptanceStage::CleanTab || Stage == EAssetAcceptanceStage::DiscardTab ||
	     Stage == EAssetAcceptanceStage::FloatingPanel))
	{
		ExerciseAssetWorkspaceInput(InEvents);
		return;
	}
	if ((Stage == EAssetAcceptanceStage::Texture || Stage == EAssetAcceptanceStage::CustomMaterial ||
	     Stage == EAssetAcceptanceStage::CustomMaterialReset))
	{
		if ((Stage == EAssetAcceptanceStage::CustomMaterial || Stage == EAssetAcceptanceStage::CustomMaterialReset))
		{
			ExerciseCustomMaterialInput(InEvents);
			return;
		}
		ExerciseAssetTextureInput(InEvents);
		return;
	}
	if ((Stage == EAssetAcceptanceStage::References))
	{
		ExerciseAssetReferences(InEvents);
		return;
	}
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	const auto& Model = Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Model();
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::OpenMaterial:
			Editor.AssetWorkspace->Open("/Game/Material.hasset");
			Scenario.AssetExerciseModel = Model->Data;
			Scenario.AssetExerciseRoughness = Roughness(*Model->Data->Materials.front()->Asset);
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitMaterialAndRevealRoughness);
			break;
		case EAssetState::AwaitMaterialAndRevealRoughness:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Editor.AssetWorkspace->RevealProperty("value/Pbr.RoughnessFactor/0");
				Scenario.Asset.Progress.TransitionTo(EAssetState::FocusRoughness);
			}
			break;
		case EAssetState::FocusRoughness:
			AssetKey(InEvents, EKey::None, true, 1);
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("value/Pbr.RoughnessFactor/0"),
			                  Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::TypeRoughness);
			}
			break;
		case EAssetState::TypeRoughness:
			if (ExerciseTextInput(InEvents, Scenario.Asset.RoughnessInput, "0.31"))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::UndoRoughness);
			}
			break;
		case EAssetState::UndoRoughness:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == .31f,
			             "Material numeric input failed");
			RequireAsset(Roughness(*Model->Data->Materials.front()->Asset) == Scenario.AssetExerciseRoughness,
			             "Draft leaked into scene");
			Editor.Gui->FinishEditing();
			AssetShortcut(InEvents, EKey::Z);
			Scenario.Asset.Progress.TransitionTo(EAssetState::RedoRoughness);
			break;
		case EAssetState::RedoRoughness:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == Scenario.AssetExerciseRoughness,
			             "Material numeric Undo failed");
			AssetShortcut(InEvents, EKey::Y);
			Scenario.Asset.Progress.TransitionTo(EAssetState::SaveRoughness);
			break;
		case EAssetState::SaveRoughness:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == .31f,
			             "Material numeric Redo failed");
			AssetShortcut(InEvents, EKey::S);
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitMaterialRefresh);
			break;
		case EAssetState::AwaitMaterialRefresh:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    Roughness(*Model->Data->Materials.front()->Asset) == .31f)
			{
				RequireAsset(Model->Data->Asset == Scenario.AssetExerciseModel->Asset &&
				                 Model->Data->QueryGeometry == Scenario.AssetExerciseModel->QueryGeometry,
				             "Material refresh rebuilt geometry");
				RequireAsset(Model->Material.Roughness == .73f && Model->Surface.Reference &&
				                 !Model->Surface.Overrides.empty(),
				             "Material refresh discarded scene overrides");
				Scenario.Asset.Progress.TransitionTo(EAssetState::RevealTextureReference);
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetReferences(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	const auto& Model = Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Model();
	const std::string ReferenceControl = "Texture##BaseColorTexture";
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::RevealTextureReference:
			Editor.AssetWorkspace->RevealProperty(ReferenceControl);
			Scenario.Asset.Progress.TransitionTo(EAssetState::OpenTextureReference);
			break;
		case EAssetState::OpenTextureReference:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds(ReferenceControl), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::RevealTextureChoice);
			}
			break;
		case EAssetState::RevealTextureChoice:
			Editor.AssetWorkspace->RevealProperty("choice/" + ReferenceControl + "/Game/SecondTexture.hasset");
			Scenario.Asset.Progress.TransitionTo(EAssetState::SelectTextureChoice);
			break;
		case EAssetState::SelectTextureChoice:
			if (ExerciseClick(
			        InEvents,
			        Editor.AssetWorkspace->ObservedBounds("choice/" + ReferenceControl + "/Game/SecondTexture.hasset"),
			        Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::SaveTextureReference);
			}
			break;
		case EAssetState::SaveTextureReference:
			if (AssetValue(ReadValue<FMaterialAsset>(Document->Snapshot()), "BaseColorTexture").Texture->Path ==
			    "/Game/SecondTexture.hasset")
			{
				RequireAsset(AssetValue(*Model->Data->Materials.front()->Asset, "BaseColorTexture").Texture->Path !=
				                 "/Game/SecondTexture.hasset",
				             "Draft reference leaked into scene");
				AssetShortcut(InEvents, EKey::S);
				Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitTextureRefresh);
			}
			break;
		case EAssetState::AwaitTextureRefresh:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    AssetValue(*Model->Data->Materials.front()->Asset, "BaseColorTexture").Texture->Path ==
			        "/Game/SecondTexture.hasset")
			{
				Editor.AssetWorkspace->Open("/Game/Model.hasset");
				Scenario.Asset.Progress.TransitionTo(EAssetState::ExerciseModelPreview);
			}
			break;
		case EAssetState::ExerciseModelPreview:
			if (Document && Document->Loaded().Path == "/Game/Model.hasset" &&
			    (!Scenario.AssetPreviewExercise.Progress.Is(EAssetPreviewState::RevealPosition) ||
			     Editor.AssetWorkspace->IsPreviewReady()))
			{
				if (!ExerciseAssetPreviewInput(InEvents))
				{
					break;
				}
				Editor.AssetWorkspace->RevealProperty("Slot 0");
				Scenario.Asset.Progress.TransitionTo(EAssetState::OpenMaterialSlot);
			}
			break;
		case EAssetState::OpenMaterialSlot:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("Slot 0"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::RevealMaterialChoice);
			}
			break;
		case EAssetState::RevealMaterialChoice:
			Editor.AssetWorkspace->RevealProperty("choice/Slot 0/Game/CustomMaterial.hasset");
			Scenario.Asset.Progress.TransitionTo(EAssetState::SelectMaterialChoice);
			break;
		case EAssetState::SelectMaterialChoice:
			if (ExerciseClick(InEvents,
			                  Editor.AssetWorkspace->ObservedBounds("choice/Slot 0/Game/CustomMaterial.hasset"),
			                  Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::SaveMaterialSlot);
			}
			break;
		case EAssetState::SaveMaterialSlot:
			if (ReadValue<std::vector<FAssetRef>>(Document->Get("materialSlots")).front().Path ==
			    "/Game/CustomMaterial.hasset")
			{
				AssetShortcut(InEvents, EKey::S);
				Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitMaterialSlotRefresh);
			}
			break;
		case EAssetState::AwaitMaterialSlotRefresh:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    Model->Data->Materials.front()->Asset->Name == "Edited CustomMaterial")
			{
				Editor.AssetWorkspace->Open("/Game/Texture.hasset");
				Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitTextureAndRevealEncoding);
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetTextureInput(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::AwaitTextureAndRevealEncoding:
			if (Document && Document->Loaded().Path == "/Game/Texture.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Editor.AssetWorkspace->RevealProperty("encoding");
				Scenario.Asset.Progress.TransitionTo(EAssetState::OpenEncoding);
			}
			break;
		case EAssetState::OpenEncoding:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("encoding"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::SelectSrgbEncoding);
			}
			break;
		case EAssetState::SelectSrgbEncoding:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("encoding/sRGB"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::SaveEncoding);
			}
			break;
		case EAssetState::SaveEncoding:
			if (ReadValue<EMaterialTextureEncoding>(Document->Get("encoding")) == EMaterialTextureEncoding::Srgb)
			{
				RequireAsset(Scenario.bPendingAssetEditChecked, "Pending encoding protection was not exercised");
				AssetShortcut(InEvents, EKey::S);
				Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitEncodingSave);
			}
			break;
		case EAssetState::AwaitEncodingSave:
			if (!Document->IsSaving() && !Document->IsDirty() && Editor.AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Editor.Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(Editor.Tasks);
				RequireAsset(Saved->Encoding == EMaterialTextureEncoding::Srgb &&
				                 Saved->Mips.front() == Document->Loaded().As<FTextureAsset>()->Mips.front(),
				             "Encoding save changed mip zero");
				Editor.AssetWorkspace->Open("/Game/Radiance.hasset");
				Scenario.Asset.Progress.TransitionTo(EAssetState::CaptureCubePreview);
			}
			break;
		case EAssetState::CaptureCubePreview:
			if (Document && Document->Loaded().Path == "/Game/Radiance.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseAssets.parent_path() / "AssetEditor-Cube.png";
				Editor.AssetWorkspace->Open("/Game/Model.hasset");
				Scenario.Asset.Progress.TransitionTo(EAssetState::UndoModelForClose);
			}
			break;
		case EAssetState::UndoModelForClose:
			if (Document && Document->Loaded().Path == "/Game/Model.hasset" && Editor.AssetWorkspace->IsPreviewReady())
			{
				AssetShortcut(InEvents, EKey::Z);
				Scenario.Asset.Progress.TransitionTo(EAssetState::RequestModelClose);
			}
			break;
		case EAssetState::RequestModelClose:
		case EAssetState::RetryModelClose:
		{
			RequireAsset(Document && Document->IsDirty(), "Saved model undo did not remain local and dirty");
			auto Bounds = Editor.AssetWorkspace->ObservedBounds("tab//Game/Model.hasset");
			Bounds.X = Bounds.Z - Editor.Gui->Scale(28);
			FInputEvent Pointer;
			Pointer.Type = EEventType::MouseMove;
			Pointer.X = (Bounds.X + Bounds.Z) * .5f;
			Pointer.Y = (Bounds.Y + Bounds.W) * .5f;
			InEvents.push_back(Pointer);
			// Settle scrolling and hover before pressing the overlapping tab close control.
			if (Scenario.Asset.ModelTabClose.Hover.ConsumeFrame())
			{
				break;
			}
			if (ExerciseClick(InEvents, Bounds, Scenario.Asset.ModelTabClose.Click, EAcceptanceClickDelay::Immediate))
			{
				Scenario.Asset.ModelTabClose.Hover.Restart();
				Scenario.Asset.Progress.TransitionTo(Scenario.Asset.Progress.Is(EAssetState::RequestModelClose)
				                                         ? EAssetState::CancelModelClose
				                                         : EAssetState::SaveModelOnClose);
			}
			break;
		}
		case EAssetState::CancelModelClose:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("close/cancel"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::VerifyModelCloseCancellation);
			}
			break;
		case EAssetState::VerifyModelCloseCancellation:
			RequireAsset(Document && Document->IsDirty(), "Cancel asset close discarded draft");
			Scenario.Asset.Progress.TransitionTo(EAssetState::RetryModelClose);
			break;
		case EAssetState::SaveModelOnClose:
			if (Scenario.Asset.ModelCloseRequestObservation.IsAt(FAssetScenarioContext::CloseDiagnosticFrames))
			{
				Scenario.PlacementCapture =
				    Editor.Options.ExerciseAssets.parent_path() / "AssetEditor-CloseRequest.png";
			}
			Scenario.Asset.ModelCloseRequestObservation.Advance();
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("close/save"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitModelClose);
			}
			break;
		case EAssetState::AwaitModelClose:
			Scenario.Asset.ModelCloseObservation.Advance();
			if (Scenario.Asset.ModelCloseObservation.IsAt(FAssetScenarioContext::CloseDiagnosticFrames))
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseAssets.parent_path() / "AssetEditor-Close.png";
				Log(ELogLevel::Info, "Waiting for close: " + Editor.AssetWorkspace->ActiveStatus());
			}
			if (!Document || Document->Loaded().Path != "/Game/Model.hasset")
			{
				Scenario.Asset.ModelCloseObservation.Restart();
				Scenario.Asset.Progress.TransitionTo(EAssetState::OpenCustomMaterial);
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseCustomMaterialInput(std::vector<FInputEvent>& InEvents)
{
	const auto Stage = DescribeAssetAcceptance(Scenario.Asset.Progress.GetState()).Stage;
	if ((Stage == EAssetAcceptanceStage::CustomMaterialReset))
	{
		ExerciseCustomMaterialReset(InEvents);
		return;
	}
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::OpenCustomMaterial:
			Editor.AssetWorkspace->Open("/Game/CustomMaterial.hasset");
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitCustomMaterialAndRevealTint);
			break;
		case EAssetState::AwaitCustomMaterialAndRevealTint:
			if (Document && Document->Loaded().Path == "/Game/CustomMaterial.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Editor.AssetWorkspace->RevealProperty("value/Tint/3");
				Scenario.Asset.Progress.TransitionTo(EAssetState::FocusTint);
			}
			break;
		case EAssetState::FocusTint:
			AssetKey(InEvents, EKey::None, true, 1);
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("value/Tint/3"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::TypeTint);
			}
			break;
		case EAssetState::TypeTint:
			if (ExerciseTextInput(InEvents, Scenario.Asset.TintInput, "0.37"))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::SaveTint);
			}
			break;
		case EAssetState::SaveTint:
			RequireAsset(std::bit_cast<float>(
			                 AssetValue(ReadValue<FMaterialAsset>(Document->Snapshot()), "Tint").Words[3]) == .37f,
			             "Custom vector parameter edit failed");
			AssetShortcut(InEvents, EKey::S);
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitTintSave);
			break;
		case EAssetState::AwaitTintSave:
			if (!Document->IsSaving() && !Document->IsDirty() && Editor.AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Editor.Assets.LoadAsync<FMaterialAsset>(Document->Loaded().Path).Get(Editor.Tasks);
				RequireAsset(std::bit_cast<float>(AssetValue(*Saved, "Tint").Words[3]) == .37f,
				             "Custom vector parameter save failed");
				Editor.AssetWorkspace->RevealProperty("reset/Tint");
				Scenario.Asset.Progress.TransitionTo(EAssetState::ResetTint);
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseCustomMaterialReset(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	const auto Material = ReadValue<FMaterialAsset>(Document->Snapshot());
	const bool bOverridden = std::any_of(Material.Values.begin(), Material.Values.end(),
	                                     [](const auto& InValue)
	                                     {
		                                     return InValue.Name == "Tint";
	                                     });
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::ResetTint:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("reset/Tint"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::UndoTintReset);
			}
			break;
		case EAssetState::UndoTintReset:
			RequireAsset(!bOverridden && Document->IsDirty(), "Reset did not restore declared default");
			AssetShortcut(InEvents, EKey::Z);
			Scenario.Asset.Progress.TransitionTo(EAssetState::RedoTintReset);
			break;
		case EAssetState::RedoTintReset:
			RequireAsset(bOverridden && !Document->IsDirty(), "Custom reset undo did not restore saved baseline");
			AssetShortcut(InEvents, EKey::Y);
			Scenario.Asset.Progress.TransitionTo(EAssetState::SaveTintReset);
			break;
		case EAssetState::SaveTintReset:
			RequireAsset(!bOverridden && Document->IsDirty(), "Custom reset redo failed");
			AssetShortcut(InEvents, EKey::S);
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitTintResetSave);
			break;
		case EAssetState::AwaitTintResetSave:
			if (!Document->IsSaving() && !Document->IsDirty() && Editor.AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Editor.Assets.LoadAsync<FMaterialAsset>(Document->Loaded().Path).Get(Editor.Tasks);
				RequireAsset(std::none_of(Saved->Values.begin(), Saved->Values.end(),
				                          [](const auto& InValue)
				                          {
					                          return InValue.Name == "Tint";
				                          }),
				             "Reset was not persisted");
				Scenario.AssetExerciseIndex = 0;
				Scenario.Asset.Progress.TransitionTo(EAssetState::OpenCleanTab);
			}
			break;
	}
}
} // namespace Hyperion
