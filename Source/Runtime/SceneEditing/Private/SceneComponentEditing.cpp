#include "Hyperion/SceneEditing/SceneComponentEditing.h"

namespace Hyperion
{
namespace
{
bool IsImmutableModelBinding(const FRecordDescriptor& InType, std::string_view InMember)
{
	if (InType.CppType == typeid(FSceneModelSource))
	{
		return true;
	}
	return InType.CppType == typeid(FSceneModelComponent) &&
	       (InMember == "asset" || InMember == "sourceNode" || InMember == "sourcePrimitive");
}
} // namespace

const FSceneComponent& GetSceneComponent(const FSceneEditDocument& InDocument, const FSceneComponentRequest& InRequest)
{
	GetSceneSettings(InDocument, {InRequest.Document, InRequest.Revision});
	DescribeSceneNode(InDocument, {InRequest.Document, InRequest.Handle});
	const auto* Component = InDocument.Target().FindNode(InRequest.Handle)->Components.Find(InRequest.Component);
	if (!Component || !Component->Get())
	{
		throw FSceneEditError("not_found", "Node does not contain the requested component");
	}
	return *Component;
}

FSceneComponentList ListSceneComponents(const FSceneEditDocument& InDocument, const FSceneNodeRequest& InRequest)
{
	DescribeSceneNode(InDocument, InRequest);
	FSceneComponentList Result;
	for (const auto& Component : InDocument.Target().FindNode(InRequest.Handle)->Components.All())
	{
		if (Component.Get())
		{
			Result.Components.push_back(
			    {Component.Id, Component.Type->Id, Component.Type->Label, Component.Type->bRequired});
		}
	}
	return Result;
}

FSceneDocumentInfo EditSceneComponentStructure(FSceneEditDocument& InDocument,
                                               const FSceneComponentStructureRequest& InRequest)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InRequest.Handles.empty() || InRequest.Handles.size() > 128)
	{
		throw std::invalid_argument("Edit between 1 and 128 distinct objects");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (const auto Handle : InRequest.Handles)
	{
		DescribeSceneNode(InDocument, {InRequest.Document, Handle});
		auto Node = *InDocument.Target().FindNode(Handle);
		if (InRequest.bRemove)
		{
			Node.Components.Remove(InRequest.Component);
		}
		else
		{
			AddDefaultSceneComponent(Node, InRequest.Component, InRequest.Type);
		}
		Edits.push_back({Handle, std::move(Node)});
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

FSceneDocumentInfo SetSceneComponent(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest,
                                     std::string_view InComponent, const FRecordDescriptor& InType, const void* InValue)
{
	InDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (InRequest.Handles.empty() || InRequest.Handles.size() > 128)
	{
		throw std::invalid_argument("Edit between 1 and 128 distinct objects");
	}
	std::vector<FSceneNodeEdit> Edits;
	for (const auto Handle : InRequest.Handles)
	{
		Edits.push_back(PrepareSceneComponentEdit(
		    InDocument, {InRequest.Document, InRequest.Revision, Handle, std::string(InComponent)}, InType, InValue));
	}
	InDocument.CommitEdits(std::move(Edits), InRequest.Revision);
	return DescribeSceneDocument(InDocument);
}

FSceneNodeEdit PrepareSceneComponentEdit(const FSceneEditDocument& InDocument, const FSceneComponentRequest& InRequest,
                                         const FRecordDescriptor& InType, const void* InValue)
{
	const auto& Source = GetSceneComponent(InDocument, InRequest);
	if (Source.Type->Record != &InType)
	{
		throw std::invalid_argument("Component type does not match the operation");
	}
	auto Node = *InDocument.Target().FindNode(InRequest.Handle);
	auto* Value = Node.Components.Find(InRequest.Component)->Edit();
	for (const auto& Member : InType.Members)
	{
		if (!Member.Options.bPersistent)
		{
			continue;
		}
		const auto Candidate = Member.Write(InValue);
		if (IsImmutableModelBinding(InType, Member.Id) && !EqualInspectionValue(Candidate, Member.Write(Source.Get())))
		{
			throw FSceneEditError("read_only", "Immutable component field: " + Member.Id);
		}
		Member.Read(Value, Candidate, {Member.Id});
	}
	if (InType.Validate)
	{
		InType.Validate(Value);
	}
	return {InRequest.Handle, std::move(Node)};
}
} // namespace Hyperion
