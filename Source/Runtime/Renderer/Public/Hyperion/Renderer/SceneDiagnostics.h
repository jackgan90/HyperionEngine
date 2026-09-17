#pragma once
#include "Hyperion/Renderer/ScenePublication.h"

namespace Hyperion
{
struct FScenePrimitiveDiagnostic
{
	std::string Primitive;
	std::string Name;
	std::uint64_t AppliedRevision{};
	FMat4 AppliedWorld = Identity();
	bool bAppliedVisible{};
	std::string Status;
	std::uint64_t LastDrawFrame{};
	std::uint64_t LastDrawView{};
	std::string LastDrawPass;
	bool bLastDrawReady{};
	std::string Error;
	std::uint32_t RenderSlot{};
	std::uint64_t RenderGeneration{};
};

struct FSceneComponentDiagnostics
{
	FSceneHandle Object;
	std::string Component;
	FScenePublicationToken Publication;
	std::uint64_t ExpectedPrimitiveRevision{};
	bool bApplied{};
	std::vector<FScenePrimitiveDiagnostic> Primitives;
};

template<> const FRecordDescriptor& RecordType<FScenePrimitiveDiagnostic>();
template<> const FRecordDescriptor& RecordType<FSceneComponentDiagnostics>();
} // namespace Hyperion
