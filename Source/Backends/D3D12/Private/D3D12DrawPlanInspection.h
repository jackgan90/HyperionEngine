#pragma once
#include "D3D12Draws.h"

namespace Hyperion
{
// Out-of-band observations only. These values never own resources or participate in native recording.
struct FNativeDrawBindCounts
{
	std::uint64_t Root{};
	std::uint64_t Heap{};
	std::uint64_t Constant{};
	std::uint64_t Table{};
	std::uint64_t Pipeline{};
	std::uint64_t Geometry{};
	std::uint64_t Dynamic{};
	bool operator==(const FNativeDrawBindCounts&) const = default;
};

struct FNativeDrawPlanMetrics
{
	bool bHasPlan{};
	std::size_t CommandSize{};
	std::size_t CommandAlignment{};
	std::size_t CommandCount{};
	std::size_t CommandCapacity{};
	std::size_t UsedBytes{};
	std::size_t CapacityBytes{};
	FNativeDrawBindCounts Binds;
};

enum class ENativeDrawObservation
{
	Pipeline,
	Topology,
	Vertices,
	Indices,
	Stencil,
	Blend,
	Scissor,
	Root,
	Heaps,
	Constant,
	Table,
	Draw
};

struct FNativeDrawObservation
{
	ENativeDrawObservation Operation{};
	ID3D12PipelineState* Pipeline{};
	D3D_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	D3D12_VERTEX_BUFFER_VIEW Vertices{};
	D3D12_INDEX_BUFFER_VIEW Indices{};
	UINT StencilReference{};
	std::array<float, 4> BlendConstants{};
	D3D12_RECT Scissor{};
	ID3D12RootSignature* Root{};
	std::array<ID3D12DescriptorHeap*, 2> Heaps{};
	UINT RootParameter{};
	D3D12_GPU_VIRTUAL_ADDRESS ConstantAddress{};
	D3D12_GPU_DESCRIPTOR_HANDLE TableHandle{};
	UINT IndexCount{};
	UINT InstanceCount{};
	UINT FirstIndex{};
	INT VertexOffset{};
	UINT FirstInstance{};
};

FNativeDrawPlanMetrics DescribeNativeDrawPlan(const FD3D12DrawPlan* InPlan);
std::vector<FNativeDrawObservation> InspectNativeDrawPlan(const FD3D12DrawPlan& InPlan);
} // namespace Hyperion
