#pragma once
#include "AcceptanceInput.h"
#include "Hyperion/SceneEditing/SceneDocumentHost.h"
#include <filesystem>

namespace Hyperion
{
struct FSceneLifecycleAcceptanceContext
{
	TAcceptanceState<ESceneLifecycleState> Progress;
	FAcceptanceClick Click;
	FAcceptanceTextInput PathInput;
	FAcceptanceFrameWait ReadySettle{8};
	std::shared_ptr<FSceneDocumentChange> Fixture;
	std::filesystem::path Destination;
	std::string OriginalDocument;
	std::string AssetDocument;
	std::uint64_t AssetGeneration{};
	FSceneHandle OriginalHandle;
	std::size_t CaseIndex{};
	bool bSavingEmpty = true;
	bool bVerified{};
};
} // namespace Hyperion
