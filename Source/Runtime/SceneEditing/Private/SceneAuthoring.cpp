#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
namespace
{
void RequireCurrent(const FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	// Queries allow a busy scene but still reject document replacement and inconsistent snapshots.
	ListSceneNodes(InDocument, {InRequest.Document, InRequest.Revision, 0, 1});
}

const FSceneNode& RequireNode(const FSceneEditDocument& InDocument, FSceneHandle InHandle)
{
	const auto* Node = InDocument.Target().FindNode(InHandle);
	if (!Node)
	{
		throw FSceneEditError("stale_handle", "Node no longer exists in this document");
	}
	return *Node;
}
} // namespace

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
	RequireCurrent(InDocument, InRequest);
	return {InDocument.Id(), InDocument.Target().Revision(), InDocument.Selection().All(),
	        InDocument.Selection().Primary()};
}

FSceneSelectionInfo SetSceneSelection(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InRequest.Handles.size() > 128)
	{
		throw std::invalid_argument("Select at most 128 distinct objects");
	}
	FSceneSelection Selection;
	for (const auto Handle : InRequest.Handles)
	{
		RequireNode(InDocument, Handle);
		if (Selection.Contains(Handle))
		{
			throw std::invalid_argument("Selection contains duplicate handles");
		}
		Selection.Toggle(Handle);
	}
	InDocument.ReplaceSelection(std::move(Selection));
	return GetSceneSelection(InDocument, {InRequest.Document, InRequest.Revision});
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
		auto Node = RequireNode(InDocument, Edit.Handle);
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
		Node.Parent() = RequireNode(InDocument, *InRequest.Parent).Id;
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

FSceneDocumentInfo DeleteSceneSelection(FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	InDocument.CommitDelete();
	return DescribeSceneDocument(InDocument);
}

FSceneSettings GetSceneSettings(const FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest)
{
	RequireCurrent(InDocument, InRequest);
	return InDocument.Target().Settings();
}

FSceneDocumentInfo SetSceneSettings(FSceneEditDocument& InDocument, const FSceneSettingsRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	InDocument.CommitSettings(InRequest.Settings);
	return DescribeSceneDocument(InDocument);
}
} // namespace Hyperion
