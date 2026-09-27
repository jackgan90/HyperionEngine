#include "Hyperion/Automation/Session.h"
#include "Hyperion/Scene/SceneClipboard.h"
#include "SceneOperations.h"
#include "SceneTestTarget.h"
#include <source_location>

namespace Hyperion
{
struct FClipboardTestComponent
{
	std::string Reference;
	bool operator==(const FClipboardTestComponent&) const = default;
};

struct FUnsupportedClipboardComponent
{
	int Value{};
	bool operator==(const FUnsupportedClipboardComponent&) const = default;
};

template<> const FRecordDescriptor& RecordType<FClipboardTestComponent>()
{
	static const auto Type = MakeRecord<FClipboardTestComponent>(
	    "test.clipboard.reference", {Member("reference", &FClipboardTestComponent::Reference)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FUnsupportedClipboardComponent>()
{
	static const auto Type = MakeRecord<FUnsupportedClipboardComponent>(
	    "test.clipboard.unsupported", {Member("value", &FUnsupportedClipboardComponent::Value)});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Clipboard check failed at " + std::to_string(InLocation.line()));
	}
}

struct FClipboardFixture
{
	FTaskSystem Tasks{1, 1};
	FSceneTestTarget Target{Tasks};
	FSceneEditDocument Document;
	std::string Token;
	std::string Text;
	bool bWriteFails{};
	bool bReadFails{};

	FClipboardFixture()
	{
		Document.Attach(Target);
		Document.SetClipboardProvider({[this]
		                               {
			                               if (bReadFails)
			                               {
				                               throw std::runtime_error("clipboard busy");
			                               }
			                               return Token;
		                               },
		                               [this](const std::string& InToken, const std::string& InText)
		                               {
			                               if (bWriteFails)
			                               {
				                               throw std::runtime_error("clipboard busy");
			                               }
			                               Token = InToken;
			                               Text = InText;
		                               }});
	}

	~FClipboardFixture()
	{
		Document.Detach(Tasks);
	}

	void Copy()
	{
		Document.CopySelection(Document.Id(), Target.Revision());
	}

	void Paste()
	{
		Document.PasteClipboard(Document.Id(), Target.Revision());
	}

	FSceneHandle Add(std::string InName, std::string InParent = {})
	{
		auto Node = MakeScenePointLightNode({});
		Node.Name = std::move(InName);
		Node.Parent() = std::move(InParent);
		return Target.AddNode(std::move(Node));
	}
};

template<class T> void RejectUnchanged(FClipboardFixture& InFixture, T InAction)
{
	const auto Revision = InFixture.Target.Revision();
	const auto Cursor = InFixture.Document.GetState().HistoryCursor;
	const auto Size = InFixture.Document.GetState().History.size();
	const auto Selection = InFixture.Document.Selection();
	const bool bDirty = InFixture.Document.IsDirty();
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	Check(bRejected && Revision == InFixture.Target.Revision() &&
	      Cursor == InFixture.Document.GetState().HistoryCursor &&
	      Size == InFixture.Document.GetState().History.size() && Selection == InFixture.Document.Selection() &&
	      bDirty == InFixture.Document.IsDirty());
}

void ClipboardHierarchyAndHistory()
{
	FClipboardFixture F;
	const auto Parent = F.Add("Parent");
	const auto Root = F.Add("Light", F.Target.FindNode(Parent)->Id);
	const auto Child = F.Add("Child", F.Target.FindNode(Root)->Id);
	auto Source = *F.Target.FindNode(Child);
	Source.Local() = Scale({0, -2, 3});
	Source.bEnabled = false;
	Source.PointLight()->Intensity = 123;
	F.Target.EditNodes({{Child, Source}}, F.Target.Revision());
	F.Document.Selection().Toggle(Root);
	F.Document.Selection().Toggle(Child);
	const auto Before = F.Document.Selection();
	F.Copy();
	Check(F.Document.ClipboardInfo().Nodes == 2 && !F.Document.IsDirty());
	Source.PointLight()->Intensity = 321;
	F.Document.CommitEdits({{Child, Source}}, F.Target.Revision());
	F.Paste();
	const auto Pasted = F.Document.Selection().All();
	Check(Pasted.size() == 2 && F.Document.GetState().HistoryCursor == 2);
	Check(F.Target.FindNode(Pasted[0])->Parent() == F.Target.FindNode(Parent)->Id);
	const auto* Copy = F.Target.FindNode(Pasted[1]);
	Check(Copy->Parent() == F.Target.FindNode(Pasted[0])->Id && Copy->Name == "Child (1)" &&
	      Copy->PointLight()->Intensity == 123 && !Copy->bEnabled && Copy->Local().Values == Source.Local().Values);
	const auto CopiedId = Copy->Id;
	F.Document.Undo();
	Check(F.Document.Selection() == Before && !F.Target.FindNode(Pasted[0]));
	F.Token = "ordinary text";
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	F.Document.Redo();
	Check(F.Target.FindHandle(CopiedId) != Pasted[1] &&
	      F.Document.Selection().Primary() == F.Target.FindHandle(CopiedId));
	F.Document.CommitDelete();
	F.Document.Undo();
	F.Document.Undo();
	Check(F.Document.Selection() == Before);
	F.Document.Redo();
	F.Document.Redo();
	Check(!F.Target.FindHandle(CopiedId).Scene);
}

void ClipboardNamesAndLifetime()
{
	FClipboardFixture F;
	const auto Source = F.Add("Camera2026");
	F.Add("Camera2026 (1)");
	F.Document.ReplaceSelection(FSceneSelection(Source));
	F.Copy();
	const auto Token = F.Token;
	F.Document.CommitDelete();
	F.Paste();
	Check(F.Target.FindNode(*F.Document.Selection())->Name == "Camera2026 (2)");
	F.Paste();
	Check(F.Target.FindNode(*F.Document.Selection())->Name == "Camera2026 (3)");
	F.Copy();
	F.Paste();
	Check(F.Target.FindNode(*F.Document.Selection())->Name == "Camera2026 (4)");
	F.Document.Undo();
	const auto HistorySize = F.Document.GetState().History.size();
	F.Copy();
	Check(F.Document.GetState().History.size() == HistorySize);
	F.bWriteFails = true;
	const auto LastToken = F.Token;
	RejectUnchanged(F,
	                [&]
	                {
		                F.Copy();
	                });
	Check(F.Token == LastToken && F.Document.ClipboardInfo().bCanPaste);
	F.bWriteFails = false;
	F.bReadFails = true;
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	F.bReadFails = false;
	F.Document.Selection().Clear();
	F.Copy();
	Check(F.Token == LastToken);
	const auto Id = F.Document.Id();
	F.Document.Reset();
	Check(!F.Document.ClipboardInfo().bCanPaste && F.Token == LastToken);
	RejectUnchanged(F,
	                [&]
	                {
		                F.Document.PasteClipboard(Id, F.Target.Revision());
	                });
}

void ClipboardExternalReferences()
{
	FClipboardFixture F;
	const auto Parent = F.Add("Parent");
	const auto Source = F.Add("Child", F.Target.FindNode(Parent)->Id);
	F.Document.ReplaceSelection(FSceneSelection(Source));
	F.Copy();
	auto Moved = *F.Target.FindNode(Parent);
	Moved.Local() = Translation({10, 0, 0});
	F.Document.CommitEdits({{Parent, Moved}}, F.Target.Revision());
	F.Paste();
	FSceneNodeView View;
	F.Target.NodeView(*F.Document.Selection(), View);
	Check(View.World.Values == Moved.Local().Values);
	F.Document.Undo();
	F.Document.ReplaceSelection(FSceneSelection(Parent));
	F.Document.CommitDelete();
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	F.Document.Undo();
	Check(F.Document.ClipboardInfo().bCanPaste);
	F.Paste();
	Check(F.Target.FindNode(*F.Document.Selection())->Parent() == Moved.Id);
	F.Document.SetInteractionState(true, false);
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	F.Document.SetInteractionState(false, false);
	F.Document.Invalidate();
	Check(!F.Document.ClipboardInfo().bCanPaste);
}

void ClipboardMaterialIsolation()
{
	FClipboardFixture F;
	FMaterialDescription Description;
	Description.Name = "clipboard";
	FMaterialPass Pass;
	Pass.Vertex = {"Clipboard.hlsl", "VSMain"};
	Pass.Pixel = {"Clipboard.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	FMaterialParameterDeclaration Parameter;
	Parameter.Name = "Pixel:Surface.Value";
	Parameter.Default = FMaterialValue::Float(1);
	Description.Parameters.push_back(Parameter);
	auto Material = std::make_shared<FMaterialInstance>(std::make_shared<FMaterialDefinition>(Description));
	Material->Set("Value", FMaterialValue::Float(2));
	FSceneNode Node;
	Node.Name = "Mesh";
	Node.Model().emplace();
	Node.Model()->Surface.Instance = Material;
	Node.Model()->SectionSurfaces[0].Instance = Material;
	const auto Handle = F.Target.AddNode(Node);
	F.Document.ReplaceSelection(FSceneSelection(Handle));
	F.Copy();
	const auto Snapshot = Material->Freeze();
	Material->Set("Value", FMaterialValue::Float(3));
	F.Paste();
	const auto& Model = *F.Target.FindNode(*F.Document.Selection())->Model();
	Check(!Model.Surface.Instance && Model.Surface.Snapshot == Snapshot &&
	      Model.SectionSurfaces.at(0).Snapshot == Snapshot);
	Check(Material->Freeze() != Model.Surface.Snapshot);
}

void ClipboardAutomation()
{
	FClipboardFixture F;
	const auto Handle = F.Add("Agent");
	F.Document.ReplaceSelection(FSceneSelection(Handle));
	FOperationCatalog Catalog;
	RegisterSceneOperations(Catalog, &F.Document);
	Catalog.Seal();
	Check(!Catalog.Find("scene.selection.copy").Info.bReadOnly);
	Catalog.Search("scene clipboard");
	Catalog.Describe("scene.clipboard.paste");
	Catalog.DescribeType(RecordType<FSceneClipboardInfo>().Id);
	FAutomationSession Session(Catalog);
	const auto Invoke = [&](const char* InOperation)
	{
		const FSceneMutationRequest Request{F.Document.Id(), F.Target.Revision()};
		const auto Result = Session.Call(InOperation, WriteRecordWire(RecordType<FSceneMutationRequest>(), &Request));
		Check(ReadValue<std::string>(std::get<FArchiveNode::FObject>(Result.Value).at("status")) == "completed");
	};
	Invoke("scene.selection.copy");
	F.Paste();
	Check(F.Document.GetState().HistoryCursor == 1);
	F.Copy();
	Invoke("scene.clipboard.paste");
	Check(F.Document.GetState().HistoryCursor == 2 && F.Target.Nodes().size() == 3);
	const FSceneInfoRequest Info;
	const auto Result = Session.Call("scene.clipboard.info", WriteRecordWire(RecordType<FSceneInfoRequest>(), &Info));
	Check(ReadValue<std::string>(std::get<FArchiveNode::FObject>(Result.Value).at("status")) == "completed");
	FOperationCatalog Missing;
	RegisterSceneOperations(Missing, nullptr);
	Missing.Seal();
	Check(!Missing.Find("scene.clipboard.paste").Info.Unavailable.empty());
}

void ClipboardComponentsAndReferences()
{
	FClipboardFixture F;
	auto Node = MakeSceneCameraNode("components");
	Node.Name = "Rig";
	Node.DirectionalLight() = FSceneDirectionalLight{};
	Node.EnvironmentLight() = FSceneEnvironmentLight{};
	Node.PointLight() = FScenePointLight{};
	Node.SpotLight() = FSceneSpotLight{};
	Node.Components.Slot<FSceneModelSource>() = FSceneModelSource{"model-asset", "asset-node", Node.Id};
	Node.Components.Rename(RecordType<FSceneCamera>().Id, "custom-camera-id");
	const auto Handle = F.Target.AddNode(Node);
	F.Document.ReplaceSelection(FSceneSelection(Handle));
	F.Copy();
	F.Paste();
	auto Copy = *F.Target.FindNode(*F.Document.Selection());
	Check(Copy.Components.Slot<FSceneModelSource>()->InstanceRoot == Copy.Id);
	Copy.Id = Node.Id;
	Copy.Name = Node.Name;
	Copy.Components.Slot<FSceneModelSource>()->InstanceRoot = Node.Id;
	Check(Copy == Node);
	Check(F.Target.Settings() == FSceneSettings{});
	F.Document.MarkSaved(F.Document.GetState().Epoch, F.Document.GetState().State);
	Check(!F.Document.IsDirty());
	F.Document.Undo();
	Check(F.Document.IsDirty());
	F.Document.Redo();
	Check(!F.Document.IsDirty());
	const auto Token = F.Token;
	auto Opaque = Node;
	Opaque.Id.clear();
	Opaque.Components.AddOpaque("opaque", std::make_shared<const FArchiveNode>(FArchiveNode::FObject{}));
	const auto Unknown = F.Target.AddNode(Opaque);
	F.Document.ReplaceSelection(FSceneSelection(Unknown));
	RejectUnchanged(F,
	                [&]
	                {
		                F.Copy();
	                });
	Check(F.Token == Token);
}

void ClipboardBudgetAndStaleParent()
{
	FClipboardFixture F;
	const auto Parent = F.Add("Parent");
	const auto OriginalParent = *F.Target.FindNode(Parent);
	const auto Child = F.Add("Child", OriginalParent.Id);
	F.Document.ReplaceSelection(FSceneSelection(Child));
	F.Copy();
	F.Document.ReplaceSelection(FSceneSelection(Parent));
	F.Document.CommitDelete();
	F.Target.AddNode(OriginalParent);
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	std::vector<FSceneNode> Nodes(SceneClipboardMaxNodes + 1);
	Nodes[0].Id = "large-root";
	for (std::size_t Index = 1; Index < Nodes.size(); ++Index)
	{
		Nodes[Index].Id = "large-" + std::to_string(Index);
		Nodes[Index].Parent() = "large-root";
	}
	const auto Handles = F.Target.AddNodes(std::move(Nodes));
	F.Document.ReplaceSelection(FSceneSelection(Handles[0]));
	RejectUnchanged(F,
	                [&]
	                {
		                F.Copy();
	                });
}

void ClipboardAdmissionFailures()
{
	FClipboardFixture F;
	const auto Handle = F.Add("Source");
	F.Document.ReplaceSelection(FSceneSelection(Handle));
	F.Copy();
	F.Target.bRejectAdd = true;
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	Check(F.Target.Nodes().size() == 1);
	F.Target.bRejectAdd = false;
	F.Document.AssetsRefreshed();
	F.Target.bRejectRebind = true;
	RejectUnchanged(F,
	                [&]
	                {
		                F.Paste();
	                });
	F.Target.bRejectRebind = false;
	F.Paste();
	Check(F.Target.RebindCount == 2 && F.Target.Nodes().size() == 2);
	F.Document.Undo();
	F.Target.bRejectAdd = true;
	RejectUnchanged(F,
	                [&]
	                {
		                F.Document.Redo();
	                });
	F.Target.bRejectAdd = false;
	F.Document.Redo();
	Check(F.Target.Nodes().size() == 2);
	std::size_t Bytes = SceneClipboardMaxBytes;
	bool bRejected{};
	try
	{
		CloneSceneClipboardNode(*F.Target.FindNode(Handle), Bytes);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	Check(bRejected);
}

void ClipboardExtensionContracts()
{
	auto Descriptor = MakeSceneComponent<FClipboardTestComponent>("Clipboard reference");
	ConfigureSceneClipboardComponent(Descriptor);
	Descriptor.ClipboardReferences = [](std::any& InState, const auto& InVisitor)
	{
		InVisitor(std::any_cast<std::optional<FClipboardTestComponent>&>(InState)->Reference);
	};
	SceneComponentRegistry().Register(std::move(Descriptor));
	SceneComponentRegistry().Register(MakeSceneComponent<FUnsupportedClipboardComponent>("Unsupported clipboard"));
	FClipboardFixture F;
	FSceneNode Node;
	Node.Id = "extension";
	Node.Components.Slot<FClipboardTestComponent>() = FClipboardTestComponent{Node.Id};
	const auto Source = F.Target.AddNode(Node);
	F.Document.ReplaceSelection(FSceneSelection(Source));
	F.Copy();
	F.Paste();
	const auto* Copy = F.Target.FindNode(*F.Document.Selection());
	Check(Copy->Components.Slot<FClipboardTestComponent>()->Reference == Copy->Id);
	const auto Token = F.Token;
	Node.Id.clear();
	Node.Components.Slot<FClipboardTestComponent>().reset();
	Node.Components.Slot<FUnsupportedClipboardComponent>().emplace();
	F.Document.ReplaceSelection(FSceneSelection(F.Target.AddNode(Node)));
	RejectUnchanged(F,
	                [&]
	                {
		                F.Copy();
	                });
	Check(F.Token == Token);
}
} // namespace

void CheckSceneClipboardOperations()
{
	ClipboardHierarchyAndHistory();
	ClipboardNamesAndLifetime();
	ClipboardExternalReferences();
	ClipboardMaterialIsolation();
	ClipboardAutomation();
	ClipboardComponentsAndReferences();
	ClipboardBudgetAndStaleParent();
	ClipboardAdmissionFailures();
	ClipboardExtensionContracts();
}
