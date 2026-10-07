#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/GraphTestSupport.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <array>
#include <type_traits>

using namespace Hyperion;

namespace
{
static_assert(BufferUsage(ERHIBufferUsage::Vertex) == 1);
static_assert(BufferUsage(ERHIBufferUsage::Index) == 2);
static_assert(BufferUsage(ERHIBufferUsage::Constant) == 4);
static_assert(BufferUsage(ERHIBufferUsage::StructuredRead) == 8);
static_assert(BufferUsage(ERHIBufferUsage::RawRead) == 16);
static_assert(BufferUsage(ERHIBufferUsage::StructuredWrite) == 32);
static_assert(BufferUsage(ERHIBufferUsage::RawWrite) == 64);
static_assert(BufferUsage({}) == 0);
static_assert(BufferUsage({ERHIBufferUsage::Vertex, ERHIBufferUsage::Index}) == 3);
static_assert(BufferUsage({ERHIBufferUsage::Index, ERHIBufferUsage::Vertex, ERHIBufferUsage::Index}) == 3);
static_assert(std::is_same_v<decltype(FBufferDesc::Usage), std::uint32_t>);
static_assert(std::is_same_v<decltype(FRHIBufferInfo::Usage), std::uint32_t>);

// Fixed historical values are independent of the production usage definitions.
constexpr std::array<std::uint32_t, 15> AcceptedGraphUsages{8,  16, 24, 32, 40,  48,  56, 64,
                                                            72, 80, 88, 96, 104, 112, 120};
constexpr std::array<std::uint32_t, 12> WritableGraphUsages{32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120};

void CheckUsageFoundation()
{
	static_assert(KnownBufferUsages == 127 && ShaderReadBufferUsages == 24 && ShaderWriteBufferUsages == 96);
	for (std::uint32_t Usage = 0; Usage < 128; ++Usage)
	{
		HYP_CHECK(IsKnownBufferUsage(Usage) == (Usage != 0));
		HYP_CHECK(HasAnyBufferUsage(Usage, ShaderReadBufferUsages) == ((Usage & 24U) != 0));
		HYP_CHECK(HasAnyBufferUsage(Usage, ShaderWriteBufferUsages) == ((Usage & 96U) != 0));
		HYP_CHECK(HasOnlyBufferUsage(Usage, ShaderReadBufferUsages) ==
		          (Usage == 0 || Usage == 8 || Usage == 16 || Usage == 24));
	}
	for (std::uint32_t Bit = 7; Bit < 32; ++Bit)
	{
		const std::uint32_t Unknown = std::uint32_t{1} << Bit;
		HYP_CHECK(!IsKnownBufferUsage(Unknown) && !IsKnownBufferUsage(Unknown | 127U));
		HYP_CHECK(!HasOnlyBufferUsage(Unknown, KnownBufferUsages));
		HYP_CHECK(!HasAnyBufferUsage(Unknown, KnownBufferUsages));
	}
	HYP_CHECK(!HasAnyBufferUsage(127, 0));
	HYP_CHECK(HasOnlyBufferUsage(0, 0) && !HasOnlyBufferUsage(1, 0));
}

class FUsageBuffer final : public IRHIBuffer
{
public:
	explicit FUsageBuffer(std::uint32_t InUsage) : Usage(InUsage)
	{
	}

	const void* GetDeviceIdentity() const noexcept override
	{
		return this;
	}

	FRHIBufferInfo GetInfo() const noexcept override
	{
		return {256, Usage};
	}

private:
	std::uint32_t Usage;
};

FGraphBufferImport Import(std::uint32_t InUsage, EResourceState InState)
{
	return {"Usage compatibility", {std::make_shared<FUsageBuffer>(InUsage)}, 256, InUsage, true, InState};
}

void CheckImport(std::uint32_t InUsage, EResourceState InState, bool bInAccepted)
{
	FRenderGraph Graph;
	bool bRejected = false;
	try
	{
		const auto Buffer = Graph.Import(Import(InUsage, InState));
		Graph.Export(Buffer, InState);
	}
	catch (const std::invalid_argument& Error)
	{
		const std::string Message = Error.what();
		HYP_CHECK(Message == "Invalid graph buffer source, size or shader usage" ||
		          Message == "Graph buffer state is incompatible with its usage");
		bRejected = true;
	}
	if (bRejected == bInAccepted)
	{
		throw std::runtime_error("Unexpected graph usage acceptance: " + std::to_string(InUsage));
	}
}

void CheckGraphUsageMatrix()
{
	for (std::uint32_t Usage = 0; Usage < 128; ++Usage)
	{
		CheckImport(Usage, EResourceState::ShaderRead,
		            std::ranges::find(AcceptedGraphUsages, Usage) != AcceptedGraphUsages.end());
		CheckImport(Usage, EResourceState::ShaderWrite,
		            std::ranges::find(WritableGraphUsages, Usage) != WritableGraphUsages.end());
	}
	for (std::uint32_t Bit = 7; Bit < 32; ++Bit)
	{
		const std::uint32_t Unknown = std::uint32_t{1} << Bit;
		for (const std::uint32_t Usage : {Unknown, Unknown | 8U, Unknown | 120U})
		{
			CheckImport(Usage, EResourceState::ShaderRead, false);
			CheckImport(Usage, EResourceState::ShaderWrite, false);
		}
	}
	for (const auto State : {EResourceState::Present, EResourceState::RenderTarget, EResourceState::DepthWrite,
	                         EResourceState::CopySource, EResourceState::CopyDestination})
	{
		CheckImport(120, State, false);
	}
}

void CheckWriteOnlyReadState()
{
	for (const std::uint32_t Usage : {32U, 64U, 96U})
	{
		FRenderGraph Graph;
		const auto Buffer = Graph.Import(Import(Usage, EResourceState::ShaderWrite));
		FComputePass Pass;
		Pass.Name = "Read state compatibility";
		Pass.Buffers = {{Buffer, EResourceState::ShaderRead}};
		Pass.Dispatches.resize(1);
		Graph.AddCompute(std::move(Pass));
		Graph.Export(Buffer, EResourceState::ShaderRead);
		const auto Commands = Graph.Compile();
		HYP_CHECK(Commands.size() == 1 && Commands.front().Transitions.size() == 1);
		HYP_CHECK(Commands.front().Transitions.front().Before == EResourceState::ShaderWrite);
		HYP_CHECK(Commands.front().Transitions.front().After == EResourceState::ShaderRead);
	}
}

void CheckRejectedAccess(EResourceState InState, bool bInCompute, bool bInInitialized, bool bInFullOverwrite,
                         std::string_view InExpected)
{
	FRenderGraph Graph;
	auto Source = Import(120, EResourceState::ShaderRead);
	Source.bInitialized = bInInitialized;
	const auto Buffer = Graph.Import(std::move(Source));
	if (bInCompute)
	{
		FComputePass Pass;
		Pass.Name = "Invalid buffer access";
		Pass.Buffers = {{Buffer, InState, bInFullOverwrite}};
		Pass.Dispatches.resize(1);
		Graph.AddCompute(std::move(Pass));
	}
	else
	{
		auto Pass = MakeColorPass(Graph, "Invalid graphics buffer write", EAttachmentLoad::Clear);
		Pass.Buffers = {{Buffer, InState, bInFullOverwrite}};
		Graph.Add(std::move(Pass));
	}
	bool bRejected = false;
	try
	{
		Graph.Compile();
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(Error.what() == InExpected);
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

void CheckGraphBufferUsage()
{
	CheckUsageFoundation();
	CheckGraphUsageMatrix();
	CheckWriteOnlyReadState();
	CheckRejectedAccess(EResourceState::ShaderWrite, false, true, false,
	                    "Invalid graph buffer access or conflicting SRV/UAV uses");
	CheckRejectedAccess(EResourceState::ShaderRead, true, true, true,
	                    "Invalid graph buffer access or conflicting SRV/UAV uses");
	CheckRejectedAccess(EResourceState::ShaderRead, true, false, false, "Graph uses undefined buffer contents");
}
