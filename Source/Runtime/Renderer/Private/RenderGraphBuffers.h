#pragma once
#include "RenderGraphPass.h"

namespace Hyperion
{
struct FGraphBufferState
{
	FBuffer Buffer;
	EResourceState State;
	bool bInitialized{};
};

void ValidateGraphBufferState(const FGraphBufferImport& InImport, EResourceState InState);
void ValidateGraphBuffers(const FAcceptedGraphPass& InPass, std::span<const FGraphBufferImport> InBuffers,
                          std::uint64_t InGraph);
std::vector<FGraphBufferState> ResolveGraphBuffers(std::span<const FGraphBufferImport> InBuffers, bool bInResolve);
void ApplyGraphBuffers(const FAcceptedGraphPass& InPass, std::span<const FGraphBufferImport> InBuffers,
                       std::vector<FGraphBufferState>& InStates, FPassCommands& OutCommands);
void TransitionGraphBuffer(FGraphBufferState& InBuffer, EResourceState InState, FPassCommands& OutCommands);
} // namespace Hyperion
