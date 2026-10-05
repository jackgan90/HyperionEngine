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
	if (Scenario.AssetExerciseLoggedStep != static_cast<int>(Scenario.ExerciseStep))
	{
		Scenario.AssetExerciseLoggedStep = static_cast<int>(Scenario.ExerciseStep);
		Log(ELogLevel::Info, "Asset property acceptance step " + std::to_string(Scenario.ExerciseStep));
	}
	if (Scenario.ExerciseStep >= 161)
	{
		ExerciseAssetWorkspaceInput(InEvents);
		return;
	}
	if (Scenario.ExerciseStep >= 130)
	{
		if (Scenario.ExerciseStep >= 150)
		{
			ExerciseCustomMaterialInput(InEvents);
			return;
		}
		ExerciseAssetTextureInput(InEvents);
		return;
	}
	if (Scenario.ExerciseStep >= 111)
	{
		ExerciseAssetReferences(InEvents);
		return;
	}
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	const auto& Model = Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Model();
	switch (Scenario.ExerciseStep)
	{
		case 100:
			Editor.AssetWorkspace->Open("/Game/Material.hasset");
			Scenario.AssetExerciseModel = Model->Data;
			Scenario.AssetExerciseRoughness = Roughness(*Model->Data->Materials.front()->Asset);
			++Scenario.ExerciseStep;
			break;
		case 101:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Editor.AssetWorkspace->RevealProperty("value/Pbr.RoughnessFactor/0");
				++Scenario.ExerciseStep;
			}
			break;
		case 102:
			AssetKey(InEvents, EKey::None, true, 1);
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("value/Pbr.RoughnessFactor/0"));
			break;
		case 103:
			++Scenario.ExerciseWait;
			if (Scenario.ExerciseWait == 1)
			{
				AssetKey(InEvents, EKey::A, true, 1);
			}
			if (Scenario.ExerciseWait == 2)
			{
				AssetKey(InEvents, EKey::A, false);
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "0.31";
				InEvents.push_back(Text);
			}
			if (Scenario.ExerciseWait == 4)
			{
				AssetKey(InEvents, EKey::Enter, true);
			}
			if (Scenario.ExerciseWait == 5)
			{
				AssetKey(InEvents, EKey::Enter, false);
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		case 104:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == .31f,
			             "Material numeric input failed");
			RequireAsset(Roughness(*Model->Data->Materials.front()->Asset) == Scenario.AssetExerciseRoughness,
			             "Draft leaked into scene");
			Editor.Gui->FinishEditing();
			AssetShortcut(InEvents, EKey::Z);
			++Scenario.ExerciseStep;
			break;
		case 105:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == Scenario.AssetExerciseRoughness,
			             "Material numeric Undo failed");
			AssetShortcut(InEvents, EKey::Y);
			++Scenario.ExerciseStep;
			break;
		case 106:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == .31f,
			             "Material numeric Redo failed");
			AssetShortcut(InEvents, EKey::S);
			Scenario.ExerciseStep = 110;
			break;
		case 110:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    Roughness(*Model->Data->Materials.front()->Asset) == .31f)
			{
				RequireAsset(Model->Data->Asset == Scenario.AssetExerciseModel->Asset &&
				                 Model->Data->QueryGeometry == Scenario.AssetExerciseModel->QueryGeometry,
				             "Material refresh rebuilt geometry");
				RequireAsset(Model->Material.Roughness == .73f && Model->Surface.Reference &&
				                 !Model->Surface.Overrides.empty(),
				             "Material refresh discarded scene overrides");
				++Scenario.ExerciseStep;
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetReferences(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	const auto& Model = Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Model();
	const std::string ReferenceControl = "Texture##BaseColorTexture";
	switch (Scenario.ExerciseStep)
	{
		case 111:
			Editor.AssetWorkspace->RevealProperty(ReferenceControl);
			++Scenario.ExerciseStep;
			break;
		case 112:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds(ReferenceControl));
			break;
		case 113:
			Editor.AssetWorkspace->RevealProperty("choice/" + ReferenceControl + "/Game/SecondTexture.hasset");
			++Scenario.ExerciseStep;
			break;
		case 114:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("choice/" + ReferenceControl +
			                                                              "/Game/SecondTexture.hasset"));
			break;
		case 115:
			if (AssetValue(ReadValue<FMaterialAsset>(Document->Snapshot()), "BaseColorTexture").Texture->Path ==
			    "/Game/SecondTexture.hasset")
			{
				RequireAsset(AssetValue(*Model->Data->Materials.front()->Asset, "BaseColorTexture").Texture->Path !=
				                 "/Game/SecondTexture.hasset",
				             "Draft reference leaked into scene");
				AssetShortcut(InEvents, EKey::S);
				++Scenario.ExerciseStep;
			}
			break;
		case 116:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    AssetValue(*Model->Data->Materials.front()->Asset, "BaseColorTexture").Texture->Path ==
			        "/Game/SecondTexture.hasset")
			{
				Editor.AssetWorkspace->Open("/Game/Model.hasset");
				Scenario.ExerciseStep = 120;
			}
			break;
		case 120:
			if (Document && Document->Loaded().Path == "/Game/Model.hasset" &&
			    (Scenario.AssetPreviewExercise.Step != 0 || Editor.AssetWorkspace->IsPreviewReady()))
			{
				if (!ExerciseAssetPreviewInput(InEvents))
				{
					break;
				}
				Editor.AssetWorkspace->RevealProperty("Slot 0");
				++Scenario.ExerciseStep;
			}
			break;
		case 121:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("Slot 0"));
			break;
		case 122:
			Editor.AssetWorkspace->RevealProperty("choice/Slot 0/Game/CustomMaterial.hasset");
			++Scenario.ExerciseStep;
			break;
		case 123:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("choice/Slot 0/Game/CustomMaterial.hasset"));
			break;
		case 124:
			if (ReadValue<std::vector<FAssetRef>>(Document->Get("materialSlots")).front().Path ==
			    "/Game/CustomMaterial.hasset")
			{
				AssetShortcut(InEvents, EKey::S);
				++Scenario.ExerciseStep;
			}
			break;
		case 125:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    Model->Data->Materials.front()->Asset->Name == "Edited CustomMaterial")
			{
				Editor.AssetWorkspace->Open("/Game/Texture.hasset");
				Scenario.ExerciseStep = 130;
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetTextureInput(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 130:
			if (Document && Document->Loaded().Path == "/Game/Texture.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Editor.AssetWorkspace->RevealProperty("encoding");
				++Scenario.ExerciseStep;
			}
			break;
		case 131:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("encoding"));
			break;
		case 132:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("encoding/sRGB"));
			break;
		case 133:
			if (ReadValue<EMaterialTextureEncoding>(Document->Get("encoding")) == EMaterialTextureEncoding::Srgb)
			{
				RequireAsset(Scenario.bPendingAssetEditChecked, "Pending encoding protection was not exercised");
				AssetShortcut(InEvents, EKey::S);
				++Scenario.ExerciseStep;
			}
			break;
		case 134:
			if (!Document->IsSaving() && !Document->IsDirty() && Editor.AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Editor.Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(Editor.Tasks);
				RequireAsset(Saved->Encoding == EMaterialTextureEncoding::Srgb &&
				                 Saved->Mips.front() == Document->Loaded().As<FTextureAsset>()->Mips.front(),
				             "Encoding save changed mip zero");
				Editor.AssetWorkspace->Open("/Game/Radiance.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		case 135:
			if (Document && Document->Loaded().Path == "/Game/Radiance.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseAssets.parent_path() / "AssetEditor-Cube.png";
				Editor.AssetWorkspace->Open("/Game/Model.hasset");
				++Scenario.ExerciseStep;
			}
			break;
		case 136:
			if (Document && Document->Loaded().Path == "/Game/Model.hasset" && Editor.AssetWorkspace->IsPreviewReady())
			{
				AssetShortcut(InEvents, EKey::Z);
				++Scenario.ExerciseStep;
			}
			break;
		case 137:
		case 140:
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
			if (Scenario.ExerciseWait < 30)
			{
				++Scenario.ExerciseWait;
				break;
			}
			ExerciseClick(InEvents, Bounds);
			break;
		}
		case 138:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("close/cancel"));
			break;
		case 139:
			RequireAsset(Document && Document->IsDirty(), "Cancel asset close discarded draft");
			++Scenario.ExerciseStep;
			break;
		case 141:
			if (Scenario.ExerciseWait == 60)
			{
				Scenario.PlacementCapture =
				    Editor.Options.ExerciseAssets.parent_path() / "AssetEditor-CloseRequest.png";
			}
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("close/save"));
			break;
		case 142:
			if (++Scenario.ExerciseWait == 60)
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseAssets.parent_path() / "AssetEditor-Close.png";
				Log(ELogLevel::Info, "Waiting for close: " + Editor.AssetWorkspace->ActiveStatus());
			}
			if (!Document || Document->Loaded().Path != "/Game/Model.hasset")
			{
				Scenario.ExerciseWait = 0;
				Scenario.ExerciseStep = 150;
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseCustomMaterialInput(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ExerciseStep >= 156)
	{
		ExerciseCustomMaterialReset(InEvents);
		return;
	}
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 150:
			Editor.AssetWorkspace->Open("/Game/CustomMaterial.hasset");
			++Scenario.ExerciseStep;
			break;
		case 151:
			if (Document && Document->Loaded().Path == "/Game/CustomMaterial.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Editor.AssetWorkspace->RevealProperty("value/Tint/3");
				++Scenario.ExerciseStep;
			}
			break;
		case 152:
			AssetKey(InEvents, EKey::None, true, 1);
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("value/Tint/3"));
			break;
		case 153:
			++Scenario.ExerciseWait;
			if (Scenario.ExerciseWait == 1)
			{
				AssetKey(InEvents, EKey::A, true, 1);
			}
			if (Scenario.ExerciseWait == 2)
			{
				AssetKey(InEvents, EKey::A, false);
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "0.37";
				InEvents.push_back(Text);
			}
			if (Scenario.ExerciseWait == 4)
			{
				AssetKey(InEvents, EKey::Enter, true);
			}
			if (Scenario.ExerciseWait == 5)
			{
				AssetKey(InEvents, EKey::Enter, false);
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		case 154:
			RequireAsset(std::bit_cast<float>(
			                 AssetValue(ReadValue<FMaterialAsset>(Document->Snapshot()), "Tint").Words[3]) == .37f,
			             "Custom vector parameter edit failed");
			AssetShortcut(InEvents, EKey::S);
			++Scenario.ExerciseStep;
			break;
		case 155:
			if (!Document->IsSaving() && !Document->IsDirty() && Editor.AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Editor.Assets.LoadAsync<FMaterialAsset>(Document->Loaded().Path).Get(Editor.Tasks);
				RequireAsset(std::bit_cast<float>(AssetValue(*Saved, "Tint").Words[3]) == .37f,
				             "Custom vector parameter save failed");
				Editor.AssetWorkspace->RevealProperty("reset/Tint");
				++Scenario.ExerciseStep;
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
	switch (Scenario.ExerciseStep)
	{
		case 156:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("reset/Tint"));
			break;
		case 157:
			RequireAsset(!bOverridden && Document->IsDirty(), "Reset did not restore declared default");
			AssetShortcut(InEvents, EKey::Z);
			++Scenario.ExerciseStep;
			break;
		case 158:
			RequireAsset(bOverridden && !Document->IsDirty(), "Custom reset undo did not restore saved baseline");
			AssetShortcut(InEvents, EKey::Y);
			++Scenario.ExerciseStep;
			break;
		case 159:
			RequireAsset(!bOverridden && Document->IsDirty(), "Custom reset redo failed");
			AssetShortcut(InEvents, EKey::S);
			++Scenario.ExerciseStep;
			break;
		case 160:
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
				Scenario.ExerciseStep = 161;
			}
			break;
	}
}
} // namespace Hyperion
