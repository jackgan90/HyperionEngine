#include "AssetWorkspaceTestSupport.h"
#include <bit>

namespace Hyperion
{
namespace
{
using namespace Tests;

void OpenCompoundDefault(FWorkspaceFixture& InFixture, FAssetService& InAssets, FTaskSystem& InTasks)
{
	FMaterialAsset Material;
	Material.Name = "Compound default";
	Material.Passes = InAssets.LoadAsync<FMaterialAsset>("/Game/CustomMaterial.hasset").Get(InTasks)->Passes;
	const auto Texture = InAssets.LoadAsync("/Game/Texture.hasset").Get(InTasks);
	FMaterialAssetParameter Parameter;
	Parameter.Name = "Settings";
	Parameter.Type.Kind = EMaterialValueKind::Structure;
	Parameter.Type.MemberNames = {"Factor", "Texture"};
	Parameter.Type.Members = {FMaterialParameterType::Numeric(EMaterialScalar::Float),
	                          FMaterialParameterType::Resource(EMaterialValueKind::Texture2D)};
	FMaterialAssetValue Default{Parameter.Type};
	Default.Elements = {{Parameter.Type.Members[0], {std::bit_cast<std::uint32_t>(1.f)}}, {Parameter.Type.Members[1]}};
	Default.Elements[1].Texture = FAssetRef{Texture->Header.Id, "/Game/Texture.hasset", Texture->Header.TypeId, {}};
	Parameter.Default = Default;
	Material.Parameters.push_back(Parameter);
	InAssets.SaveAsync("/Game/CompoundDefault.hasset", std::make_shared<const FMaterialAsset>(Material)).Get(InTasks);
	InFixture.Open("/Game/CompoundDefault.hasset");
}

void ReplaceText(FWorkspaceFixture& InFixture, const char* InText)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Modifiers = InputModifiers::Control;
	Key.Key = EKey::A;
	Key.bDown = true;
	InFixture.Frame(std::array{Key});
	Key.bDown = false;
	Key.Modifiers = 0;
	FInputEvent Text;
	Text.Type = EEventType::Text;
	Text.Text = InText;
	InFixture.Frame(std::array{Key, Text});
	InFixture.Frame();
}

void AwaitEdits(FWorkspaceFixture& InFixture)
{
	InFixture.Await(
	    [&]
	    {
		    return !InFixture.Workspace.HasPendingEdits();
	    });
}

void CheckKeyboard(FWorkspaceFixture& InFixture, FAssetService& InAssets, FTaskSystem& InTasks, unsigned InTermination)
{
	OpenCompoundDefault(InFixture, InAssets, InTasks);
	const auto Document = InFixture.Workspace.ActiveDocument();
	const auto Before = Document->Snapshot();
	const auto Generation = Document->Generation();
	const auto Preview = Document->PreviewGeneration();
	InFixture.Click(InFixture.Workspace.ObservedBounds("element/Settings/Factor"));
	InFixture.Type(InFixture.Workspace.ObservedBounds("value/Settings/Factor/0"), "0.2");
	Check(InFixture.Workspace.HasPendingEdits() && InFixture.Workspace.HasActiveInteraction());
	Check(Document->Generation() == Generation && !Document->CanUndo());
	FInputEvent Text;
	Text.Type = EEventType::Text;
	Text.Text = "5";
	InFixture.Frame(std::array{Text});
	InFixture.Frame();
	Check(InFixture.Workspace.HasActiveInteraction() && InFixture.Gui.IsEditingText());
	Check(Document->Generation() == Generation && Document->Error.empty());
	if (InTermination != 1)
	{
		AwaitEdits(InFixture);
		Check(InFixture.Workspace.HasActiveInteraction() && InFixture.Gui.IsEditingText());
		const auto Values = ReadValue<FMaterialAssetValues>(Document->Get("values"));
		Check(Values.size() == 1 && std::bit_cast<float>(Values[0].Value.Elements[0].Words[0]) == .25f);
		Check(Values[0].Value.Elements[1].Texture ==
		      ReadValue<FMaterialAsset>(Before).Parameters[0].Default->Elements[1].Texture);
	}
	if (InTermination)
	{
		FInputEvent Escape;
		Escape.Type = EEventType::Key;
		Escape.Key = EKey::Escape;
		Escape.bDown = true;
		InFixture.Frame(std::array{Escape});
		Escape.bDown = false;
		InFixture.Frame(std::array{Escape});
		AwaitEdits(InFixture);
		Check(EqualInspectionValue(Document->Snapshot(), Before) && !Document->CanUndo() && !Document->CanRedo());
		Check(!Document->IsDirty() && !InFixture.Workspace.HasActiveInteraction());
	}
	else
	{
		ReplaceText(InFixture, "0.75");
		InFixture.Gui.FinishEditing();
		InFixture.Frame();
		Check(Document->Error.empty());
		if (Document->Generation() != Generation + 2 || Document->PreviewGeneration() != Preview + 2)
		{
			throw std::runtime_error(
			    "Keyboard generations: document=" + std::to_string(Document->Generation() - Generation) +
			    ", preview=" + std::to_string(Document->PreviewGeneration() - Preview));
		}
		Check(Document->IsDirty());
		const auto Values = ReadValue<FMaterialAssetValues>(Document->Get("values"));
		Check(std::bit_cast<float>(Values[0].Value.Elements[0].Words[0]) == .75f);
		InFixture.Workspace.Undo();
		Check(EqualInspectionValue(Document->Snapshot(), Before) && !Document->CanUndo());
		InFixture.Workspace.Redo();
		Check(ReadValue<FMaterialAssetValues>(Document->Get("values")) == Values);
	}
	InFixture.Workspace.CloseAll();
}

void CheckDrag(FWorkspaceFixture& InFixture, FAssetService& InAssets, FTaskSystem& InTasks, unsigned InTermination)
{
	OpenCompoundDefault(InFixture, InAssets, InTasks);
	const auto Document = InFixture.Workspace.ActiveDocument();
	const auto Before = Document->Snapshot();
	const auto Generation = Document->Generation();
	InFixture.Click(InFixture.Workspace.ObservedBounds("element/Settings/Factor"));
	const auto Bounds = InFixture.Workspace.ObservedBounds("value/Settings/Factor/0");
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = (Bounds.X + Bounds.Z) * .5f;
	Move.Y = (Bounds.Y + Bounds.W) * .5f;
	InFixture.Frame(std::array{Move});
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	InFixture.Frame(std::array{Button});
	Move.X += 24;
	InFixture.Frame(std::array{Move});
	InFixture.Frame();
	Check(InFixture.Workspace.HasPendingEdits() && InFixture.Workspace.HasActiveInteraction());
	Move.X += 24;
	InFixture.Frame(std::array{Move});
	Check(Document->Generation() == Generation && Document->Error.empty());
	if (InTermination == 2)
	{
		FInputEvent Escape;
		Escape.Type = EEventType::Key;
		Escape.Key = EKey::Escape;
		Escape.bDown = true;
		InFixture.Frame(std::array{Escape});
		Escape.bDown = false;
		Button.bDown = false;
		InFixture.Frame(std::array{Escape, Button});
	}
	else if (InTermination == 1)
	{
		Button.bDown = false;
		InFixture.Frame(std::array{Button});
	}
	AwaitEdits(InFixture);
	if (InTermination != 2)
	{
		Check(Document->Generation() == Generation + 1 && Document->Error.empty());
		if (!InTermination)
		{
			Check(InFixture.Workspace.HasActiveInteraction());
			Move.X += 24;
			InFixture.Frame(std::array{Move});
			Button.bDown = false;
			InFixture.Frame(std::array{Button});
		}
		InFixture.Workspace.Undo();
	}
	Check(EqualInspectionValue(Document->Snapshot(), Before) && !Document->CanUndo());
	InFixture.Workspace.CloseAll();
}
} // namespace

void CheckAssetMaterialInteractions(FTaskSystem& InTasks, FAssetService& InAssets, FRenderSession& InSession,
                                    FRHICapabilities InCapabilities)
{
	for (unsigned Termination = 0; Termination < 3; ++Termination)
	{
		{
			FWorkspaceFixture Fixture(InTasks, InAssets, InSession, InCapabilities);
			CheckKeyboard(Fixture, InAssets, InTasks, Termination);
		}
		{
			FWorkspaceFixture Fixture(InTasks, InAssets, InSession, InCapabilities);
			CheckDrag(Fixture, InAssets, InTasks, Termination);
		}
	}
}
} // namespace Hyperion
