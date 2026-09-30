#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
bool CanAddDefaultSceneComponent(const FSceneComponentDescriptor& InType)
{
	return !InType.bRequired && InType.CppType != typeid(FSceneModelComponent) &&
	       InType.CppType != typeid(FSceneModelSource);
}

void AddDefaultSceneComponent(FSceneNode& InNode, std::string InId, std::string_view InType)
{
	if (!CanAddDefaultSceneComponent(*SceneComponentRegistry().Find(InType)))
	{
		throw std::invalid_argument("Component cannot be default-created; model bindings require prepared placement");
	}
	InNode.Components.Add(std::move(InId), InType);
}

FSceneSelectionInfo GetSceneSelection(const FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	InDocument.RequireCurrent(InRequest.Document, InRequest.Revision);
	return {InDocument.Id(), InDocument.Target().Revision(), InDocument.Selection().All(),
	        InDocument.Selection().Primary()};
}

void ApplySceneSelection(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	FSceneSelection Selection;
	for (const auto Handle : InRequest.Handles)
	{
		InDocument.RequireNode(Handle);
		if (Selection.Contains(Handle))
		{
			throw std::invalid_argument("Selection contains duplicate handles");
		}
		Selection.Toggle(Handle);
	}
	InDocument.ReplaceSelection(std::move(Selection));
}

FSceneSelectionInfo SetSceneSelection(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest)
{
	ApplySceneSelection(InDocument, InRequest);
	return GetSceneSelection(InDocument, {InRequest.Document, InRequest.Revision});
}

FSceneSelectionSummary SelectAllSceneNodes(FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	auto Handles = InDocument.Target().Nodes();
	if (const auto Primary = InDocument.Selection().Primary();
	    Primary && std::find(Handles.begin(), Handles.end(), *Primary) != Handles.end())
	{
		std::erase(Handles, *Primary);
		Handles.push_back(*Primary);
	}
	ApplySceneSelection(InDocument, {InRequest.Document, InRequest.Revision, std::move(Handles)});
	return {InDocument.Id(), InDocument.Target().Revision(), InDocument.Selection().All().size(),
	        InDocument.Selection().Primary()};
}

FSceneDocumentInfo SetSceneMetadata(FSceneEditDocument& InDocument, const FSceneMetadataRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InRequest.Edits.empty() || InRequest.Edits.size() > 128)
	{
		throw std::invalid_argument("Edit between 1 and 128 distinct objects");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (const auto& Edit : InRequest.Edits)
	{
		auto Node = InDocument.RequireNode(Edit.Handle);
		if (Edit.Name)
		{
			Node.Name = *Edit.Name;
		}
		if (Edit.Enabled)
		{
			Node.bEnabled = *Edit.Enabled;
		}
		Edits.push_back({Edit.Handle, std::move(Node)});
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

FSceneNodeInfo CreateSceneNode(FSceneEditDocument& InDocument, const FSceneCreateRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	FSceneNode Node;
	Node.Name = InRequest.Name;
	Node.Local() = InRequest.Local;
	if (InRequest.Parent)
	{
		Node.Parent() = InDocument.RequireNode(*InRequest.Parent).Id;
	}
	if (InRequest.Components.size() > 32)
	{
		throw std::invalid_argument("Create at most 32 components per node");
	}
	for (const auto& Type : InRequest.Components)
	{
		AddDefaultSceneComponent(Node, Type, Type);
	}
	const auto Handle = InDocument.CommitCreate(std::move(Node));
	return DescribeSceneNode(InDocument, {InDocument.Id(), Handle});
}

FSceneDocumentInfo ReparentSceneNode(FSceneEditDocument& InDocument, const FSceneReparentRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	InDocument.CommitReparent(InRequest.Handle, InRequest.Parent,
	                          InRequest.bKeepWorld ? ESceneReparentMode::KeepWorld : ESceneReparentMode::KeepLocal);
	return DescribeSceneDocument(InDocument);
}

FSceneDocumentInfo ReparentSceneNodes(FSceneEditDocument& InDocument, const FSceneNodesReparentRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	auto Edits = InDocument.PrepareReparent(InRequest.Handles, InRequest.Parent);
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

FSceneDocumentInfo DeleteSceneSelection(FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	InDocument.CommitDelete();
	return DescribeSceneDocument(InDocument);
}

FSceneSettings GetSceneSettings(const FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	InDocument.RequireCurrent(InRequest.Document, InRequest.Revision);
	return InDocument.Target().Settings();
}

FSceneDocumentInfo SetSceneSettings(FSceneEditDocument& InDocument, const FSceneSettingsRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	InDocument.CommitSettings(InRequest.Settings);
	return DescribeSceneDocument(InDocument);
}
} // namespace Hyperion
