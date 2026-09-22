#include "EditorApplication.h"
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

void FEditorPlugin::ExerciseAssetPropertyInput(std::vector<FInputEvent>& InEvents)
{
	if (AssetExerciseLoggedStep != static_cast<int>(ExerciseStep))
	{
		AssetExerciseLoggedStep = static_cast<int>(ExerciseStep);
		Log(ELogLevel::Info, "Asset property acceptance step " + std::to_string(ExerciseStep));
	}
	if (ExerciseStep >= 161)
	{
		ExerciseAssetWorkspaceInput(InEvents);
		return;
	}
	if (ExerciseStep >= 130)
	{
		if (ExerciseStep >= 150)
		{
			ExerciseCustomMaterialInput(InEvents);
			return;
		}
		ExerciseAssetTextureInput(InEvents);
		return;
	}
	if (ExerciseStep >= 111)
	{
		ExerciseAssetReferences(InEvents);
		return;
	}
	const auto* Document = AssetWorkspace->ActiveDocument();
	const auto& Model = Scene->FindNode(Scene->FindHandle("model"))->Model();
	switch (ExerciseStep)
	{
		case 100:
			AssetWorkspace->Open("/Game/Material.hasset");
			AssetExerciseModel = Model->Data;
			AssetExerciseRoughness = Roughness(*Model->Data->Materials.front()->Asset);
			++ExerciseStep;
			break;
		case 101:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" && AssetWorkspace->IsPreviewReady())
			{
				AssetWorkspace->RevealProperty("value/Pbr.RoughnessFactor/0");
				++ExerciseStep;
			}
			break;
		case 102:
			AssetKey(InEvents, EKey::None, true, 1);
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("value/Pbr.RoughnessFactor/0"));
			break;
		case 103:
			++ExerciseWait;
			if (ExerciseWait == 1)
			{
				AssetKey(InEvents, EKey::A, true, 1);
			}
			if (ExerciseWait == 2)
			{
				AssetKey(InEvents, EKey::A, false);
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "0.31";
				InEvents.push_back(Text);
			}
			if (ExerciseWait == 4)
			{
				AssetKey(InEvents, EKey::Enter, true);
			}
			if (ExerciseWait == 5)
			{
				AssetKey(InEvents, EKey::Enter, false);
				ExerciseWait = 0;
				++ExerciseStep;
			}
			break;
		case 104:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == .31f,
			             "Material numeric input failed");
			RequireAsset(Roughness(*Model->Data->Materials.front()->Asset) == AssetExerciseRoughness,
			             "Draft leaked into scene");
			Gui->FinishEditing();
			AssetShortcut(InEvents, EKey::Z);
			++ExerciseStep;
			break;
		case 105:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == AssetExerciseRoughness,
			             "Material numeric Undo failed");
			AssetShortcut(InEvents, EKey::Y);
			++ExerciseStep;
			break;
		case 106:
			RequireAsset(Roughness(ReadValue<FMaterialAsset>(Document->Snapshot())) == .31f,
			             "Material numeric Redo failed");
			AssetShortcut(InEvents, EKey::S);
			ExerciseStep = 110;
			break;
		case 110:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    Roughness(*Model->Data->Materials.front()->Asset) == .31f)
			{
				RequireAsset(Model->Data->Asset == AssetExerciseModel->Asset &&
				                 Model->Data->QueryGeometry == AssetExerciseModel->QueryGeometry,
				             "Material refresh rebuilt geometry");
				RequireAsset(Model->Material.Roughness == .73f && Model->Surface.Reference &&
				                 !Model->Surface.Overrides.empty(),
				             "Material refresh discarded scene overrides");
				++ExerciseStep;
			}
			break;
	}
}

void FEditorPlugin::ExerciseAssetReferences(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = AssetWorkspace->ActiveDocument();
	const auto& Model = Scene->FindNode(Scene->FindHandle("model"))->Model();
	const std::string ReferenceControl = "Texture##BaseColorTexture";
	switch (ExerciseStep)
	{
		case 111:
			AssetWorkspace->RevealProperty(ReferenceControl);
			++ExerciseStep;
			break;
		case 112:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds(ReferenceControl));
			break;
		case 113:
			AssetWorkspace->RevealProperty("choice/" + ReferenceControl + "/Game/SecondTexture.hasset");
			++ExerciseStep;
			break;
		case 114:
			ExerciseClick(InEvents,
			              AssetWorkspace->ObservedBounds("choice/" + ReferenceControl + "/Game/SecondTexture.hasset"));
			break;
		case 115:
			if (AssetValue(ReadValue<FMaterialAsset>(Document->Snapshot()), "BaseColorTexture").Texture->Path ==
			    "/Game/SecondTexture.hasset")
			{
				RequireAsset(AssetValue(*Model->Data->Materials.front()->Asset, "BaseColorTexture").Texture->Path !=
				                 "/Game/SecondTexture.hasset",
				             "Draft reference leaked into scene");
				AssetShortcut(InEvents, EKey::S);
				++ExerciseStep;
			}
			break;
		case 116:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    AssetValue(*Model->Data->Materials.front()->Asset, "BaseColorTexture").Texture->Path ==
			        "/Game/SecondTexture.hasset")
			{
				AssetWorkspace->Open("/Game/Model.hasset");
				ExerciseStep = 120;
			}
			break;
		case 120:
			if (Document && Document->Loaded().Path == "/Game/Model.hasset" &&
			    (AssetPreviewExercise.Step != 0 || AssetWorkspace->IsPreviewReady()))
			{
				if (!ExerciseAssetPreviewInput(InEvents))
				{
					break;
				}
				AssetWorkspace->RevealProperty("Slot 0");
				++ExerciseStep;
			}
			break;
		case 121:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("Slot 0"));
			break;
		case 122:
			AssetWorkspace->RevealProperty("choice/Slot 0/Game/CustomMaterial.hasset");
			++ExerciseStep;
			break;
		case 123:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("choice/Slot 0/Game/CustomMaterial.hasset"));
			break;
		case 124:
			if (ReadValue<std::vector<FAssetRef>>(Document->Get("materialSlots")).front().Path ==
			    "/Game/CustomMaterial.hasset")
			{
				AssetShortcut(InEvents, EKey::S);
				++ExerciseStep;
			}
			break;
		case 125:
			if (!Document->IsSaving() && !Document->IsDirty() &&
			    Model->Data->Materials.front()->Asset->Name == "Edited CustomMaterial")
			{
				AssetWorkspace->Open("/Game/Texture.hasset");
				ExerciseStep = 130;
			}
			break;
	}
}

void FEditorPlugin::ExerciseAssetTextureInput(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (ExerciseStep)
	{
		case 130:
			if (Document && Document->Loaded().Path == "/Game/Texture.hasset" && AssetWorkspace->IsPreviewReady())
			{
				AssetWorkspace->RevealProperty("encoding");
				++ExerciseStep;
			}
			break;
		case 131:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("encoding"));
			break;
		case 132:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("encoding/sRGB"));
			break;
		case 133:
			if (ReadValue<EMaterialTextureEncoding>(Document->Get("encoding")) == EMaterialTextureEncoding::Srgb)
			{
				RequireAsset(bPendingAssetEditChecked, "Pending encoding protection was not exercised");
				AssetShortcut(InEvents, EKey::S);
				++ExerciseStep;
			}
			break;
		case 134:
			if (!Document->IsSaving() && !Document->IsDirty() && AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Assets.LoadAsync<FTextureAsset>("/Game/Texture.hasset").Get(Tasks);
				RequireAsset(Saved->Encoding == EMaterialTextureEncoding::Srgb &&
				                 Saved->Mips.front() == Document->Loaded().As<FTextureAsset>()->Mips.front(),
				             "Encoding save changed mip zero");
				AssetWorkspace->Open("/Game/Radiance.hasset");
				++ExerciseStep;
			}
			break;
		case 135:
			if (Document && Document->Loaded().Path == "/Game/Radiance.hasset" && AssetWorkspace->IsPreviewReady())
			{
				PlacementCapture = Options.ExerciseAssets.parent_path() / "AssetEditor-Cube.png";
				AssetWorkspace->Open("/Game/Model.hasset");
				++ExerciseStep;
			}
			break;
		case 136:
			if (Document && Document->Loaded().Path == "/Game/Model.hasset" && AssetWorkspace->IsPreviewReady())
			{
				AssetShortcut(InEvents, EKey::Z);
				++ExerciseStep;
			}
			break;
		case 137:
		case 140:
		{
			RequireAsset(Document && Document->IsDirty(), "Saved model undo did not remain local and dirty");
			auto Bounds = AssetWorkspace->ObservedBounds("tab//Game/Model.hasset");
			Bounds.X = Bounds.Z - Gui->Scale(28);
			FInputEvent Pointer;
			Pointer.Type = EEventType::MouseMove;
			Pointer.X = (Bounds.X + Bounds.Z) * .5f;
			Pointer.Y = (Bounds.Y + Bounds.W) * .5f;
			InEvents.push_back(Pointer);
			// Settle scrolling and hover before pressing the overlapping tab close control.
			if (ExerciseWait < 30)
			{
				++ExerciseWait;
				break;
			}
			ExerciseClick(InEvents, Bounds);
			break;
		}
		case 138:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("close/cancel"));
			break;
		case 139:
			RequireAsset(Document && Document->IsDirty(), "Cancel asset close discarded draft");
			++ExerciseStep;
			break;
		case 141:
			if (ExerciseWait == 60)
			{
				PlacementCapture = Options.ExerciseAssets.parent_path() / "AssetEditor-CloseRequest.png";
			}
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("close/save"));
			break;
		case 142:
			if (++ExerciseWait == 60)
			{
				PlacementCapture = Options.ExerciseAssets.parent_path() / "AssetEditor-Close.png";
				Log(ELogLevel::Info, "Waiting for close: " + AssetWorkspace->ActiveStatus());
			}
			if (!Document || Document->Loaded().Path != "/Game/Model.hasset")
			{
				ExerciseWait = 0;
				ExerciseStep = 150;
			}
			break;
	}
}

void FEditorPlugin::ExerciseCustomMaterialInput(std::vector<FInputEvent>& InEvents)
{
	if (ExerciseStep >= 156)
	{
		ExerciseCustomMaterialReset(InEvents);
		return;
	}
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (ExerciseStep)
	{
		case 150:
			AssetWorkspace->Open("/Game/CustomMaterial.hasset");
			++ExerciseStep;
			break;
		case 151:
			if (Document && Document->Loaded().Path == "/Game/CustomMaterial.hasset" &&
			    AssetWorkspace->IsPreviewReady())
			{
				AssetWorkspace->RevealProperty("value/Tint/3");
				++ExerciseStep;
			}
			break;
		case 152:
			AssetKey(InEvents, EKey::None, true, 1);
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("value/Tint/3"));
			break;
		case 153:
			++ExerciseWait;
			if (ExerciseWait == 1)
			{
				AssetKey(InEvents, EKey::A, true, 1);
			}
			if (ExerciseWait == 2)
			{
				AssetKey(InEvents, EKey::A, false);
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "0.37";
				InEvents.push_back(Text);
			}
			if (ExerciseWait == 4)
			{
				AssetKey(InEvents, EKey::Enter, true);
			}
			if (ExerciseWait == 5)
			{
				AssetKey(InEvents, EKey::Enter, false);
				ExerciseWait = 0;
				++ExerciseStep;
			}
			break;
		case 154:
			RequireAsset(std::bit_cast<float>(
			                 AssetValue(ReadValue<FMaterialAsset>(Document->Snapshot()), "Tint").Words[3]) == .37f,
			             "Custom vector parameter edit failed");
			AssetShortcut(InEvents, EKey::S);
			++ExerciseStep;
			break;
		case 155:
			if (!Document->IsSaving() && !Document->IsDirty() && AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Assets.LoadAsync<FMaterialAsset>(Document->Loaded().Path).Get(Tasks);
				RequireAsset(std::bit_cast<float>(AssetValue(*Saved, "Tint").Words[3]) == .37f,
				             "Custom vector parameter save failed");
				AssetWorkspace->RevealProperty("reset/Tint");
				++ExerciseStep;
			}
			break;
	}
}

void FEditorPlugin::ExerciseCustomMaterialReset(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = AssetWorkspace->ActiveDocument();
	const auto Material = ReadValue<FMaterialAsset>(Document->Snapshot());
	const bool bOverridden = std::any_of(Material.Values.begin(), Material.Values.end(),
	                                     [](const auto& InValue)
	                                     {
		                                     return InValue.Name == "Tint";
	                                     });
	switch (ExerciseStep)
	{
		case 156:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("reset/Tint"));
			break;
		case 157:
			RequireAsset(!bOverridden && Document->IsDirty(), "Reset did not restore declared default");
			AssetShortcut(InEvents, EKey::Z);
			++ExerciseStep;
			break;
		case 158:
			RequireAsset(bOverridden && !Document->IsDirty(), "Custom reset undo did not restore saved baseline");
			AssetShortcut(InEvents, EKey::Y);
			++ExerciseStep;
			break;
		case 159:
			RequireAsset(!bOverridden && Document->IsDirty(), "Custom reset redo failed");
			AssetShortcut(InEvents, EKey::S);
			++ExerciseStep;
			break;
		case 160:
			if (!Document->IsSaving() && !Document->IsDirty() && AssetWorkspace->IsPreviewReady())
			{
				const auto Saved = Assets.LoadAsync<FMaterialAsset>(Document->Loaded().Path).Get(Tasks);
				RequireAsset(std::none_of(Saved->Values.begin(), Saved->Values.end(),
				                          [](const auto& InValue)
				                          {
					                          return InValue.Name == "Tint";
				                          }),
				             "Reset was not persisted");
				AssetExerciseIndex = 0;
				ExerciseStep = 161;
			}
			break;
	}
}
} // namespace Hyperion
