#pragma once
#include "Hyperion/Renderer/MaterialProviders.h"
#include "Hyperion/Renderer/ScenePublication.h"

namespace Hyperion
{
// Main publishes owned inputs once; every view in a family reads this same immutable frame.
struct FMaterialFrameContext
{
	std::uint64_t Session{};
	std::uint64_t Frame{};
	FMaterialProviderInputs Inputs;

	std::optional<FScenePublicationToken> GetSceneToken() const
	{
		return SceneToken;
	}

	const std::shared_ptr<const FSceneMetadata>& GetSceneMetadata() const
	{
		return SceneMetadata;
	}

	bool CastsSceneShadows() const
	{
		return bSceneShadows;
	}

private:
	std::shared_ptr<const FSceneMetadata> SceneMetadata;
	std::optional<FScenePublicationToken> SceneToken;
	// Resolution belongs to this immutable instance, not to editable copies of its public inputs.
	std::weak_ptr<const FMaterialFrameContext> SceneResolutionOwner;
	bool bSceneShadows{};
	friend class FRenderSession;
};

struct FRenderDrawResult
{
	std::uint64_t Frame{};
	std::uint64_t Family{};
	std::uint64_t View{};
	std::uint64_t Revision{};
	std::uint64_t MaterialRevision{};
	std::string Usage;
	bool bReady{};
	std::string Error;
};
} // namespace Hyperion
