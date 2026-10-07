#pragma once
#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include <exception>
#include <memory>

namespace Hyperion
{
struct FSceneOpenRequest
{
	std::string Document;
	std::uint64_t Revision{};
	std::string Path;
	bool bDiscard{};
};

struct FSceneHostStatus
{
	FSceneDocumentInfo Scene;
	std::string Error;
};

enum class ESceneDocumentAction
{
	New,
	Close
};

enum class ESceneDirtyAction
{
	RejectDirty,
	Save,
	Discard
};

struct FSceneLifecycleRequest
{
	std::string Document;
	std::uint64_t Revision{};
	ESceneDirtyAction Action = ESceneDirtyAction::RejectDirty;
	std::string ScenePath;
};

// Main-owned completion shared with consumers; admitted saves remain owned by the document.
struct FSceneDocumentChange
{
	std::optional<FSceneHostStatus> Result;
	std::exception_ptr Failure;
};

// Main-owned document orchestration. The host owns resource retirement and exposes the same document to its GUI.
class ISceneDocumentHost
{
public:
	virtual ~ISceneDocumentHost() = default;
	virtual void OpenDocument(const FSceneOpenRequest& InRequest) = 0;
	virtual std::shared_ptr<FSceneDocumentChange> ChangeDocument(ESceneDocumentAction InAction,
	                                                             const FSceneLifecycleRequest& InRequest) = 0;
	virtual FSceneHostStatus DocumentStatus() const = 0;
	virtual FSceneHostStatus PollDocument() = 0;
	// True only when the host has registered all content consumers and persists its root preferences.
	virtual bool SupportsContentTransitions() const = 0;
};

template<> const FRecordDescriptor& RecordType<FSceneOpenRequest>();
template<> const FRecordDescriptor& RecordType<FSceneHostStatus>();
template<> const FRecordDescriptor& RecordType<FSceneLifecycleRequest>();
template<> std::span<const TRecordEnumEntry<ESceneDirtyAction>> RecordEnumEntries<ESceneDirtyAction>();
} // namespace Hyperion
