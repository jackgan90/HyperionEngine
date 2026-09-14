#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

class FStorageTexture final : public IRHITexture
{
public:
	const void* GetDeviceIdentity() const noexcept override
	{
		return this;
	}

	FRHITextureInfo GetInfo() const noexcept override
	{
		return {17, 9,   ERHIDepthFormat::None, ERHIColorFormat::R32Float, false, ERHITextureDimension::Texture2D,
		        5,  true};
	}
};

class FStorageBuffer final : public IRHIBuffer
{
public:
	const void* GetDeviceIdentity() const noexcept override
	{
		return this;
	}

	FRHIBufferInfo GetInfo() const noexcept override
	{
		return {64, 96};
	}
};

template<typename TOperation> void Rejects(TOperation InOperation)
{
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		return;
	}
	throw std::runtime_error("Invalid compute graph accepted");
}

FGraphTexture ImportMip(FRenderGraph& InGraph, const FTexture& InTexture, std::uint32_t InMip)
{
	FGraphTextureImport Import;
	Import.Name = "Mip" + std::to_string(InMip);
	Import.Target = FRenderTarget::FromTexture(InTexture);
	Import.Size = {std::max(1U, 17U >> InMip), std::max(1U, 9U >> InMip)};
	Import.ColorFormat = ERHIColorFormat::R32Float;
	Import.MipLevel = InMip;
	Import.bStorage = true;
	return InGraph.Import(std::move(Import));
}

void CheckMipDependencies()
{
	FRenderGraph Graph;
	const FTexture Texture{std::make_shared<FStorageTexture>()};
	const auto Mip0 = ImportMip(Graph, Texture, 0);
	const auto Mip1 = ImportMip(Graph, Texture, 1);
	FComputePass First;
	First.Name = "Initialize";
	First.Writes = {{Mip0, true}};
	First.Dispatches.resize(1);
	Graph.AddCompute(First);
	FComputePass Second;
	Second.Name = "Reduce";
	Second.Reads = {Mip0};
	Second.Writes = {{Mip1, true}};
	Second.Dispatches.resize(1);
	Graph.AddCompute(Second);
	Graph.Export(Mip0, EResourceState::ShaderRead);
	Graph.Export(Mip1, EResourceState::ShaderRead);
	const auto Commands = Graph.Compile();
	HYP_CHECK(Commands.size() == 3 && Commands[0].bCompute && Commands[1].bCompute);
	HYP_CHECK(Commands[1].Transitions.size() == 2);
	HYP_CHECK(Commands[1].Transitions[0].FirstMip == 0 &&
	          Commands[1].Transitions[0].After == EResourceState::ShaderRead);
	HYP_CHECK(Commands[1].Transitions[1].FirstMip == 1 &&
	          Commands[1].Transitions[1].After == EResourceState::ShaderWrite);
	HYP_CHECK(Commands[2].Transitions.size() == 1 && Commands[2].Transitions[0].FirstMip == 1);
	Second.Name = "Preserve";
	Second.Reads.clear();
	Second.Writes[0].bFullOverwrite = false;
	Graph.AddCompute(Second);
	const auto Preserved = Graph.Compile();
	HYP_CHECK(Preserved[2].Transitions.size() == 1 && Preserved[2].Transitions[0].bUavBarrier);
}

void CheckUndefinedAndFeedback()
{
	FRenderGraph Graph;
	const auto Mip = ImportMip(Graph, {std::make_shared<FStorageTexture>()}, 0);
	FComputePass Pass;
	Pass.Name = "Invalid";
	Pass.Writes = {{Mip, false}};
	bool bPrepared = false;
	Pass.Prepare = [&]
	{
		bPrepared = true;
		return std::vector<FDispatchPacket>(1);
	};
	Graph.AddCompute(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	HYP_CHECK(!bPrepared);
	Graph = {};
	Pass.Writes = {{ImportMip(Graph, {std::make_shared<FStorageTexture>()}, 0), true}};
	Pass.Reads = {Pass.Writes[0].Texture};
	Graph.AddCompute(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	HYP_CHECK(!bPrepared);
	Graph = {};
	Pass.Writes = {{ImportMip(Graph, {std::make_shared<FStorageTexture>()}, 0), true}};
	Pass.Reads.clear();
	Pass.Prepare = []
	{
		return std::vector<FDispatchPacket>{};
	};
	Graph.AddCompute(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
}

void CheckBufferHazards()
{
	FRenderGraph Graph;
	const FBuffer Buffer{std::make_shared<FStorageBuffer>()};
	const auto Handle = Graph.Import(FGraphBufferImport{"Buffer", Buffer, 64, 96});
	FComputePass Pass;
	Pass.Name = "Write";
	Pass.Buffers = {{Handle, EResourceState::ShaderWrite, true}};
	Pass.Dispatches.resize(1);
	Graph.AddCompute(Pass);
	Pass.Name = "Update";
	Pass.Buffers[0].bFullOverwrite = false;
	Graph.AddCompute(Pass);
	Pass.Name = "Read";
	Pass.Buffers[0].State = EResourceState::ShaderRead;
	Graph.AddCompute(Pass);
	const auto Commands = Graph.Compile();
	HYP_CHECK(Commands.size() == 3 && Commands[0].Transitions[0].Buffer == Buffer);
	HYP_CHECK(Commands[1].Transitions[0].bUavBarrier);
	HYP_CHECK(Commands[2].Transitions[0].After == EResourceState::ShaderRead);
	Pass.Name = "Cycle";
	Pass.After = {3};
	Graph.AddCompute(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
}
} // namespace

void CheckComputeGraph()
{
	CheckMipDependencies();
	CheckUndefinedAndFeedback();
	CheckBufferHazards();
}
