#include "Hyperion/SceneEditing/SceneRequests.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
void RequireDocument(const FSceneEditDocument& InDocument, const std::string& InId)
{
	if (InDocument.Id() != InId)
	{
		throw FSceneEditError("stale_document", "Query the current scene document before accessing nodes");
	}
	if (!InDocument.Target().IsLoaded())
	{
		throw FSceneEditError("unavailable", "Scene has not loaded");
	}
}
} // namespace

FSceneDocumentInfo DescribeSceneDocument(const FSceneEditDocument& InDocument)
{
	const auto& State = InDocument.GetState();
	const auto& Target = InDocument.Target();
	const bool bLoaded = Target.IsLoaded();
	return {InDocument.Id(),
	        State.Path,
	        bLoaded ? Target.Revision() : 0,
	        bLoaded ? Target.Nodes().size() : 0,
	        bLoaded,
	        Target.IsReady(),
	        InDocument.IsDirty(),
	        InDocument.IsBusy(),
	        State.Save.has_value(),
	        InDocument.HasHistory(),
	        State.HistoryCursor != 0,
	        State.HistoryCursor < State.History.size()};
}

FSceneNodeInfo DescribeSceneNode(const FSceneEditDocument& InDocument, const FSceneNodeRequest& InRequest)
{
	RequireDocument(InDocument, InRequest.Document);
	FSceneNodeView View;
	if (!InDocument.Target().NodeView(InRequest.Handle, View))
	{
		throw FSceneEditError("stale_handle", "Node handle is no longer valid in this document");
	}
	const auto& Node = *View.Node;
	return {InDocument.Id(),
	        InDocument.Target().Revision(),
	        View.Handle,
	        Node.Id,
	        Node.Name,
	        std::string(ToString(Node.GetKind())),
	        Node.Parent(),
	        Node.Local(),
	        View.World,
	        Node.bEnabled,
	        View.bEffectiveEnabled};
}

FSceneNodePage ListSceneNodes(const FSceneEditDocument& InDocument, const FSceneListRequest& InRequest)
{
	RequireDocument(InDocument, InRequest.Document);
	if (InRequest.Revision != InDocument.Target().Revision())
	{
		throw FSceneEditError("stale_revision", "Scene changed during pagination; query scene.info and restart");
	}
	if (!InRequest.Limit || InRequest.Limit > 100)
	{
		throw std::invalid_argument("Scene page limit must be between 1 and 100");
	}
	const auto Nodes = InDocument.Target().Nodes();
	FSceneNodePage Result{InDocument.Id(), InRequest.Revision, Nodes.size()};
	const auto End = std::min(Nodes.size(), std::size_t(InRequest.Offset) + InRequest.Limit);
	for (std::size_t Index = InRequest.Offset; Index < End; ++Index)
	{
		Result.Nodes.push_back(DescribeSceneNode(InDocument, {Result.Document, Nodes[Index]}));
	}
	if (End < Nodes.size())
	{
		Result.Next = static_cast<std::uint32_t>(End);
	}
	return Result;
}

FSceneDocumentInfo SetSceneTransforms(FSceneEditDocument& InDocument, const FSceneTransformRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InRequest.Transforms.empty() || InRequest.Transforms.size() > 128)
	{
		throw std::invalid_argument("Transform batch must contain between 1 and 128 objects");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (const auto& Transform : InRequest.Transforms)
	{
		const auto* Source = InDocument.Target().FindNode(Transform.Handle);
		if (!Source)
		{
			throw FSceneEditError("stale_handle", "One or more transform targets no longer exist");
		}
		auto Node = *Source;
		Node.Local() = Transform.Local;
		Edits.push_back({Transform.Handle, std::move(Node)});
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}
} // namespace Hyperion
