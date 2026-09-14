#include "RenderGraphBuffers.h"
#include <set>

namespace Hyperion
{
void ValidateGraphBufferState(const FGraphBufferImport& InImport, EResourceState InState)
{
	if (InState != EResourceState::ShaderRead &&
	    (InState != EResourceState::ShaderWrite || (InImport.Usage & 96U) == 0))
	{
		throw std::invalid_argument("Graph buffer state is incompatible with its usage");
	}
}

FGraphBuffer FRenderGraph::Import(FGraphBufferImport InResource)
{
	CheckMutable();
	if (InResource.Name.empty() || !InResource.Size || !InResource.Usage || (InResource.Usage & ~120U) != 0 ||
	    (InResource.Resolve ? (!InResource.Identity || bool(InResource.Buffer)) : !InResource.Buffer))
	{
		throw std::invalid_argument("Invalid graph buffer source, size or shader usage");
	}
	ValidateGraphBufferState(InResource, InResource.InitialState);
	for (std::size_t Index = 0; Index < Buffers.size(); ++Index)
	{
		const auto& Existing = Buffers[Index];
		if (InResource.Identity ? InResource.Identity == Existing.Identity : InResource.Buffer == Existing.Buffer)
		{
			if (InResource.Buffer != Existing.Buffer || InResource.Size != Existing.Size ||
			    InResource.Usage != Existing.Usage || InResource.InitialState != Existing.InitialState ||
			    InResource.bInitialized != Existing.bInitialized || InResource.Identity != Existing.Identity ||
			    bool(InResource.Resolve) != bool(Existing.Resolve))
			{
				throw std::invalid_argument("Conflicting graph buffer import");
			}
			return {Identity, Index};
		}
	}
	Buffers.push_back(std::move(InResource));
	return {Identity, Buffers.size() - 1};
}

void FRenderGraph::Export(FGraphBuffer InBuffer, EResourceState InState)
{
	CheckMutable();
	if (InBuffer.Graph != Identity || InBuffer.Index >= Buffers.size())
	{
		throw std::invalid_argument("Foreign graph buffer export");
	}
	ValidateGraphBufferState(Buffers[InBuffer.Index], InState);
	for (const auto& Existing : BufferExports)
	{
		if (Existing.first == InBuffer)
		{
			if (Existing.second != InState)
			{
				throw std::invalid_argument("Conflicting graph buffer export");
			}
			return;
		}
	}
	BufferExports.emplace_back(InBuffer, InState);
}

void ValidateGraphBuffers(const FGraphicsPass& InPass, std::span<const FGraphBufferImport> InBuffers,
                          std::uint64_t InGraph)
{
	std::set<std::size_t> Seen;
	for (const auto& Access : InPass.Buffers)
	{
		if (Access.Buffer.Graph != InGraph || Access.Buffer.Index >= InBuffers.size() ||
		    !Seen.insert(Access.Buffer.Index).second ||
		    (!InPass.bCompute && Access.State == EResourceState::ShaderWrite) ||
		    (Access.State == EResourceState::ShaderRead && Access.bFullOverwrite))
		{
			throw std::invalid_argument("Invalid graph buffer access or conflicting SRV/UAV uses");
		}
		ValidateGraphBufferState(InBuffers[Access.Buffer.Index], Access.State);
	}
}

std::vector<FGraphBufferState> ResolveGraphBuffers(std::span<const FGraphBufferImport> InBuffers, bool bInResolve)
{
	std::set<const IRHIBuffer*> Seen;
	std::vector<FGraphBufferState> Result;
	for (const auto& Import : InBuffers)
	{
		const auto Buffer = bInResolve && Import.Resolve ? Import.Resolve() : Import.Buffer;
		if (bInResolve)
		{
			if (!Buffer || Buffer.Payload->GetInfo().Size != Import.Size ||
			    Buffer.Payload->GetInfo().Usage != Import.Usage || !Seen.insert(Buffer.Payload.get()).second)
			{
				throw std::invalid_argument("Graph buffer physical description or alias mismatch");
			}
		}
		Result.push_back({Buffer, Import.InitialState, Import.bInitialized});
	}
	return Result;
}

void TransitionGraphBuffer(FGraphBufferState& InBuffer, EResourceState InState, FPassCommands& OutCommands)
{
	if (InBuffer.State != InState || InState == EResourceState::ShaderWrite)
	{
		OutCommands.Transitions.push_back(
		    {{}, InBuffer.State, InState, 0, 0, InBuffer.Buffer, InBuffer.State == InState});
		InBuffer.State = InState;
	}
}

void ApplyGraphBuffers(const FGraphicsPass& InPass, std::span<const FGraphBufferImport> InBuffers,
                       std::vector<FGraphBufferState>& InStates, FPassCommands& OutCommands)
{
	for (const auto& Access : InPass.Buffers)
	{
		auto& State = InStates[Access.Buffer.Index];
		if (!State.bInitialized && !Access.bFullOverwrite)
		{
			throw std::invalid_argument("Graph uses undefined buffer contents");
		}
		TransitionGraphBuffer(State, Access.State, OutCommands);
		OutCommands.BufferAccesses.push_back(
		    {{State.Buffer, ERHIBufferViewKind::Raw, 0, InBuffers[Access.Buffer.Index].Size, 0}, Access.State});
		if (Access.bFullOverwrite)
		{
			State.bInitialized = true;
		}
	}
}
} // namespace Hyperion
