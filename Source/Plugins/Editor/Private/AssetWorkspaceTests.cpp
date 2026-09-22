#include "AssetWorkspace.h"
#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>
#include <source_location>
#include <thread>

namespace Hyperion
{
namespace
{
void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Asset workspace check failed: " + std::to_string(InLocation.line()));
	}
}

struct FWorkspaceFixture
{
	FAssetWorkspace Workspace;
	FGui Gui;

	FWorkspaceFixture(FTaskSystem& InTasks, FAssetService& InAssets, FRenderSession& InSession,
	                  FRHICapabilities InCapabilities)
	    : Workspace(InAssets, InTasks, InSession, InCapabilities)
	{
		(void)Gui.FontImage();
	}

	FGuiDrawData Frame(std::span<const FInputEvent> InEvents = {}, bool bInPreview = false)
	{
		Gui.BeginFrame({1200, 1800}, {1200, 1800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Workspace regression", {0, 0}, {1150, 1750});
		if (bInPreview)
		{
			Workspace.DrawTabs(Gui, 1.f / 60, InEvents);
		}
		else
		{
			Workspace.DrawProperties(Gui);
		}
		Gui.EndPanel();
		return Gui.Render();
	}

	template<class Predicate> void Await(const Predicate& InReady, bool bInPreview = false)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (!InReady())
		{
			Workspace.Poll();
			if (bInPreview)
			{
				Frame({}, true);
			}
			if (std::chrono::steady_clock::now() >= Deadline)
			{
				throw std::runtime_error(Workspace.ActiveStatus());
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void Open(const char* InPath)
	{
		Workspace.Open(InPath);
		Await(
		    [&]
		    {
			    return Workspace.ActiveDocument() != nullptr;
		    });
		Frame();
		Frame();
	}

	void Click(FVec4 InBounds)
	{
		Check(InBounds.Z > InBounds.X && InBounds.W > InBounds.Y);
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (InBounds.X + InBounds.Z) * .5f;
		Move.Y = (InBounds.Y + InBounds.W) * .5f;
		Frame(std::array{Move});
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::array{Button});
		Button.bDown = false;
		Frame(std::array{Button});
		Frame();
	}

	void Type(FVec4 InBounds, const char* InText)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Modifiers = 1;
		Frame(std::array{Key});
		Click(InBounds);
		Key.Key = EKey::A;
		Key.bDown = true;
		Frame(std::array{Key});
		Key.bDown = false;
		Key.Modifiers = 0;
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		Frame(std::array{Key, Text});
		Frame();
	}
};

void CheckInvalidTransform(FWorkspaceFixture& InFixture, FRenderSession& InSession)
{
	InFixture.Open("/Game/Model.hasset");
	InFixture.Await(
	    [&]
	    {
		    return InFixture.Workspace.IsPreviewReady();
	    });
	InFixture.Frame();
	const auto Before = HashArchive(InFixture.Workspace.ActiveDocument()->Snapshot());
	InFixture.Type(InFixture.Workspace.ObservedBounds("node/position/x"), "1e40");
	Check(!InFixture.Workspace.ActiveDocument()->Error.empty());
	Check(HashArchive(InFixture.Workspace.ActiveDocument()->Snapshot()) == Before);
	InFixture.Gui.FinishEditing();
	InFixture.Frame();
	const auto Statistics = InSession.GetResources().Statistics();
	InFixture.Type(InFixture.Workspace.ObservedBounds("node/name"), "Renamed after invalid transform");
	InFixture.Gui.FinishEditing();
	InFixture.Frame();
	Check(InFixture.Workspace.ActiveDocument()->Error.empty());
	Check(ReadValue<std::vector<FModelNode>>(InFixture.Workspace.ActiveDocument()->Get("nodes")).front().Name ==
	      "Renamed after invalid transform");
	InFixture.Workspace.Poll();
	Check(InFixture.Workspace.IsPreviewReady());
	Check(InSession.GetResources().Statistics().GeometryUploads == Statistics.GeometryUploads);
	InFixture.Workspace.Undo();
	Check(HashArchive(InFixture.Workspace.ActiveDocument()->Snapshot()) == Before);
	InFixture.Workspace.CloseAll();
}

void CheckPendingReference(FWorkspaceFixture& InFixture, FAssetService& InAssets, FTaskSystem& InTasks,
                           bool bInPreviousSave)
{
	std::vector<FAssetRef> Index;
	for (const auto* Path : {"/Game/Material.hasset", "/Game/CustomMaterial.hasset"})
	{
		const auto Loaded = InAssets.LoadAsync(Path).Get(InTasks);
		Index.push_back({Loaded->Header.Id, Path, Loaded->Header.TypeId, {}});
	}
	InAssets.SetAssetIndex(Index, "/Game");
	InFixture.Open("/Game/Model.hasset");
	if (bInPreviousSave)
	{
		InFixture.Type(InFixture.Workspace.ObservedBounds("field/name"), "Saved before reference");
		InFixture.Gui.FinishEditing();
		InFixture.Frame();
		InFixture.Workspace.SaveActive();
		Check(InFixture.Workspace.ActiveDocument()->IsSaving());
	}
	InFixture.Click(InFixture.Workspace.ObservedBounds("Slot 0"));
	InFixture.Click(InFixture.Workspace.ObservedBounds("choice/Slot 0/Game/CustomMaterial.hasset"));
	Check(InFixture.Workspace.HasPendingEdits() && InFixture.Workspace.IsDirty());
	Check(!InFixture.Workspace.CanUndo());
	InFixture.Workspace.SaveActive();
	Check(InFixture.Workspace.ActiveDocument()->IsSaving() == bInPreviousSave);
	// Finish asset work while deliberately retaining the document's old pending-save result.
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (InAssets.Statistics().InFlight)
	{
		InTasks.PumpMain();
		Check(std::chrono::steady_clock::now() < Deadline);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	InFixture.Workspace.Open("/Game/Texture.hasset");
	// No property draw after selection: completion and save must belong to the document, not the widget.
	InFixture.Await(
	    [&]
	    {
		    return !InFixture.Workspace.HasPendingEdits() && !InFixture.Workspace.IsSaving();
	    });
	const auto Saved = InAssets.LoadAsync<FModelAsset>("/Game/Model.hasset").Get(InTasks);
	Check(Saved->MaterialSlots.front().Path == "/Game/CustomMaterial.hasset");
	InFixture.Workspace.Open("/Game/Model.hasset");
	Check(!InFixture.Workspace.ActiveDocument()->IsDirty());
	InFixture.Workspace.Undo();
	Check(ReadValue<std::vector<FAssetRef>>(InFixture.Workspace.ActiveDocument()->Get("materialSlots")).front().Path ==
	      "/Game/Material.hasset");
	InFixture.Workspace.SaveActive();
	InFixture.Await(
	    [&]
	    {
		    return !InFixture.Workspace.IsSaving();
	    });
	InFixture.Workspace.CloseAll();
}

void CheckDependencyRecovery(FWorkspaceFixture& InFixture, FAssetService& InAssets, FTaskSystem& InTasks)
{
	const auto Original = InAssets.LoadAsync("/Game/Material.hasset").Get(InTasks);
	auto Bad = *Original->As<FMaterialAsset>();
	for (auto& Value : Bad.Values)
	{
		if (Value.Value.Texture)
		{
			Value.Value.Texture = FAssetRef{"", "/Game/Missing.hasset", RecordType<FTextureAsset>().Id, {}};
		}
	}
	const auto Broken =
	    InAssets
	        .SaveDocumentAsync(Original->Path, *Original->Type, WriteValue(Bad),
	                           {Original->Header.Id, {}, Original->Header.TypeId, Original->Header.Revision})
	        .Get(InTasks);
	InFixture.Open("/Game/Model.hasset");
	InFixture.Await(
	    [&]
	    {
		    return InFixture.Workspace.ActiveStatus().find("Missing.hasset") != std::string::npos;
	    });
	Check(!InFixture.Workspace.IsPreviewReady());
	const auto Fixed =
	    InAssets
	        .SaveDocumentAsync(Original->Path, *Original->Type, WriteValue(*Original->As<FMaterialAsset>()),
	                           {Broken->Header.Id, {}, Broken->Header.TypeId, Broken->Header.Revision})
	        .Get(InTasks);
	InFixture.Workspace.RefreshDependencies(std::array{*Fixed});
	InFixture.Await(
	    [&]
	    {
		    return InFixture.Workspace.IsPreviewReady();
	    });
	InFixture.Workspace.CloseAll();
}

FVec2 TextureCorner(const FGuiDrawData& InData)
{
	for (const auto& Command : InData.Commands)
	{
		if (Command.TextureId > 1 && Command.IndexCount)
		{
			return InData.Vertices.at(InData.Indices.at(Command.FirstIndex) + Command.VertexOffset).Position;
		}
	}
	throw std::runtime_error("Texture overlay missing");
}

void CheckOptionalAggregates(FWorkspaceFixture& InFixture, FAssetService& InAssets, FTaskSystem& InTasks)
{
	FMaterialAsset Material;
	Material.Name = "Optional aggregates";
	Material.Passes = InAssets.LoadAsync<FMaterialAsset>("/Game/CustomMaterial.hasset").Get(InTasks)->Passes;
	FMaterialAssetParameter Parameter;
	Parameter.Name = "Settings";
	Parameter.bRequired = false;
	Parameter.Type.Kind = EMaterialValueKind::Structure;
	Parameter.Type.MemberNames = {"Weights"};
	Parameter.Type.Members = {
	    FMaterialParameterType::Array(FMaterialParameterType::Numeric(EMaterialScalar::Float), 2)};
	Material.Parameters.push_back(Parameter);
	Parameter.Name = "Huge";
	Parameter.Type = FMaterialParameterType::Array(
	    FMaterialParameterType::Array(FMaterialParameterType::Numeric(EMaterialScalar::Float), 65536), 65536);
	Material.Parameters.push_back(Parameter);
	ValidateMaterialAsset(Material);
	InAssets.SaveAsync("/Game/Optional.hasset", std::make_shared<const FMaterialAsset>(Material)).Get(InTasks);
	InFixture.Open("/Game/Optional.hasset");
	Check(InFixture.Workspace.ActiveDocument()->Error.empty());
	InFixture.Click(InFixture.Workspace.ObservedBounds("element/Settings/Weights"));
	InFixture.Click(InFixture.Workspace.ObservedBounds("element/Settings/Weights/0"));
	for (int Iteration = 0; Iteration < 2; ++Iteration)
	{
		InFixture.Type(InFixture.Workspace.ObservedBounds("value/Settings/Weights/0/0"), "0.25");
		InFixture.Gui.FinishEditing();
		InFixture.Frame();
		const auto Edited = ReadValue<FMaterialAsset>(InFixture.Workspace.ActiveDocument()->Snapshot());
		Check(Edited.Values.size() == 1);
		const auto& Weights = Edited.Values.front().Value.Elements.at(0).Elements;
		Check(Weights.size() == 2 && std::bit_cast<float>(Weights.front().Words.at(0)) == .25f);
		InFixture.Click(InFixture.Workspace.ObservedBounds("reset/Settings"));
		Check(ReadValue<FMaterialAsset>(InFixture.Workspace.ActiveDocument()->Snapshot()).Values.empty());
	}
	InFixture.Workspace.CloseAll();
}

void CheckTextureFocusLoss(FWorkspaceFixture& InFixture)
{
	InFixture.Open("/Game/Texture.hasset");
	InFixture.Await(
	    [&]
	    {
		    return InFixture.Workspace.IsPreviewReady();
	    },
	    true);
	const auto Bounds = InFixture.Workspace.ObservedBounds("canvas");
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = (Bounds.X + Bounds.Z) * .5f;
	Move.Y = (Bounds.Y + Bounds.W) * .5f;
	InFixture.Frame(std::array{Move}, true);
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	InFixture.Frame(std::array{Button}, true);
	Move.X += 20;
	const auto Before = TextureCorner(InFixture.Frame(std::array{Move}, true));
	FInputEvent Focus;
	Focus.Type = EEventType::Focus;
	Focus.bDown = false;
	InFixture.Frame(std::array{Focus}, true);
	Focus.bDown = true;
	const auto After = TextureCorner(InFixture.Frame(std::array{Focus, Move}, true));
	Check(std::isfinite(After.X) && std::isfinite(After.Y));
	Check(std::abs(Before.X - After.X) < .1f && std::abs(Before.Y - After.Y) < .1f);
	InFixture.Workspace.CloseAll();
}
} // namespace

void CheckAssetWorkspaceRegressions(FTaskSystem& InTasks, FAssetService& InAssets, FRenderSession& InSession,
                                    FRHICapabilities InCapabilities)
{
	FWorkspaceFixture Fixture(InTasks, InAssets, InSession, InCapabilities);
	CheckInvalidTransform(Fixture, InSession);
	CheckPendingReference(Fixture, InAssets, InTasks, false);
	CheckPendingReference(Fixture, InAssets, InTasks, true);
	CheckDependencyRecovery(Fixture, InAssets, InTasks);
	CheckTextureFocusLoss(Fixture);
	CheckOptionalAggregates(Fixture, InAssets, InTasks);
	std::cout << "Invalid input, hidden pending saves, dependency recovery, metadata uploads and focus loss passed\n";
}
} // namespace Hyperion
