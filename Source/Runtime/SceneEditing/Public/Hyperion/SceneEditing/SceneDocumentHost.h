#pragma once
#include "Hyperion/SceneEditing/SceneAuthoring.h"

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

// Main-owned document orchestration. The host owns resource retirement and exposes the same document to its GUI.
class ISceneDocumentHost
{
public:
	virtual ~ISceneDocumentHost() = default;
	virtual void OpenDocument(const FSceneOpenRequest& InRequest) = 0;
	virtual FSceneHostStatus DocumentStatus() const = 0;
	virtual FSceneHostStatus PollDocument() = 0;
	// True only when the host has registered all content consumers and persists its root preferences.
	virtual bool SupportsContentTransitions() const = 0;
};

template<> const FRecordDescriptor& RecordType<FSceneOpenRequest>();
template<> const FRecordDescriptor& RecordType<FSceneHostStatus>();
} // namespace Hyperion
