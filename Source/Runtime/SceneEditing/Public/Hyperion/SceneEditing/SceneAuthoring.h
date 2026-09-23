#pragma once
#include "Hyperion/SceneEditing/SceneRequests.h"

namespace Hyperion
{
struct FSceneSelectionRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
};

struct FSceneSelectionInfo
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
	std::optional<FSceneHandle> Primary;
};

struct FSceneMetadataEdit
{
	FSceneHandle Handle;
	std::optional<std::string> Name;
	std::optional<bool> Enabled;
};

struct FSceneMetadataRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneMetadataEdit> Edits;
};

struct FSceneCreateRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::string Name = "Object";
	std::optional<FSceneHandle> Parent;
	FMat4 Local = Identity();
	std::vector<std::string> Components;
};

struct FSceneReparentRequest
{
	std::string Document;
	std::uint64_t Revision{};
	FSceneHandle Handle;
	std::optional<FSceneHandle> Parent;
	bool bKeepWorld = true;
};

struct FSceneSettingsRequest
{
	std::string Document;
	std::uint64_t Revision{};
	FSceneSettings Settings;
};

bool CanAddDefaultSceneComponent(const FSceneComponentDescriptor& InType);
void AddDefaultSceneComponent(FSceneNode& InNode, std::string InId, std::string_view InType);
FSceneSelectionInfo GetSceneSelection(const FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest);
FSceneSelectionInfo SetSceneSelection(FSceneEditDocument& InDocument, const FSceneSelectionRequest& InRequest);
FSceneDocumentInfo SetSceneMetadata(FSceneEditDocument& InDocument, const FSceneMetadataRequest& InRequest);
FSceneNodeInfo CreateSceneNode(FSceneEditDocument& InDocument, const FSceneCreateRequest& InRequest);
FSceneDocumentInfo ReparentSceneNode(FSceneEditDocument& InDocument, const FSceneReparentRequest& InRequest);
FSceneDocumentInfo DeleteSceneSelection(FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest);
FSceneSettings GetSceneSettings(const FSceneEditDocument& InDocument, const FSceneMutationRequest& InRequest);
FSceneDocumentInfo SetSceneSettings(FSceneEditDocument& InDocument, const FSceneSettingsRequest& InRequest);

template<> const FRecordDescriptor& RecordType<FSceneSelectionRequest>();
template<> const FRecordDescriptor& RecordType<FSceneSelectionInfo>();
template<> const FRecordDescriptor& RecordType<FSceneMetadataEdit>();
template<> const FRecordDescriptor& RecordType<FSceneMetadataRequest>();
template<> const FRecordDescriptor& RecordType<FSceneCreateRequest>();
template<> const FRecordDescriptor& RecordType<FSceneReparentRequest>();
template<> const FRecordDescriptor& RecordType<FSceneSettings>();
template<> const FRecordDescriptor& RecordType<FSceneSettingsRequest>();
} // namespace Hyperion
