#pragma once
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
struct FSceneInfoRequest
{
};

struct FSceneMutationRequest
{
	std::string Document;
	std::uint64_t Revision{};
};

struct FSceneListRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

struct FSceneNodeRequest
{
	std::string Document;
	FSceneHandle Handle;
};

struct FSceneTransformValue
{
	FSceneHandle Handle;
	FMat4 Local = Identity();
};

struct FSceneTransformRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneTransformValue> Transforms;
};

struct FSceneSaveRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::string Path;
};

struct FSceneDocumentInfo
{
	std::string Document;
	std::string Path;
	std::uint64_t Revision{};
	std::uint64_t Nodes{};
	bool bLoaded{};
	bool bReady{};
	bool bDirty{};
	bool bBusy{};
	bool bSaving{};
	bool bHistory{};
	bool bCanUndo{};
	bool bCanRedo{};
};

struct FSceneNodeInfo
{
	std::string Document;
	std::uint64_t Revision{};
	FSceneHandle Handle;
	std::string Id;
	std::string Name;
	std::string Kind;
	std::string Parent;
	FMat4 Local = Identity();
	FMat4 World = Identity();
	bool bEnabled{};
	bool bEffectiveEnabled{};
};

struct FSceneNodePage
{
	std::string Document;
	std::uint64_t Revision{};
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
	std::vector<FSceneNodeInfo> Nodes;
};

FSceneDocumentInfo DescribeSceneDocument(const FSceneEditDocument& InDocument);
FSceneNodeInfo DescribeSceneNode(const FSceneEditDocument& InDocument, const FSceneNodeRequest& InRequest);
FSceneNodePage ListSceneNodes(const FSceneEditDocument& InDocument, const FSceneListRequest& InRequest);
FSceneDocumentInfo SetSceneTransforms(FSceneEditDocument& InDocument, const FSceneTransformRequest& InRequest);

template<> const FRecordDescriptor& RecordType<FSceneHandle>();
template<> const FRecordDescriptor& RecordType<FSceneInfoRequest>();
template<> const FRecordDescriptor& RecordType<FSceneMutationRequest>();
template<> const FRecordDescriptor& RecordType<FSceneListRequest>();
template<> const FRecordDescriptor& RecordType<FSceneNodeRequest>();
template<> const FRecordDescriptor& RecordType<FSceneTransformValue>();
template<> const FRecordDescriptor& RecordType<FSceneTransformRequest>();
template<> const FRecordDescriptor& RecordType<FSceneSaveRequest>();
template<> const FRecordDescriptor& RecordType<FSceneDocumentInfo>();
template<> const FRecordDescriptor& RecordType<FSceneNodeInfo>();
template<> const FRecordDescriptor& RecordType<FSceneNodePage>();
} // namespace Hyperion
