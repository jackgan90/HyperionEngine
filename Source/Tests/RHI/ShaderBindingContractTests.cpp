#include "Hyperion/RHI/RHIBindingContracts.h"
#include "Support/TestSupport.h"
#include <iostream>

namespace
{
using namespace Hyperion;

template<class F> void Rejects(F InOperation)
{
	bool bRejected = false;
	try
	{
		InOperation();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckStages()
{
	HYP_CHECK(static_cast<unsigned>(ShaderStageMask(EShaderStage::Vertex)) == 1);
	HYP_CHECK(static_cast<unsigned>(ShaderStageMask(EShaderStage::Pixel)) == 2);
	HYP_CHECK(static_cast<unsigned>(ShaderStageMask(EShaderStage::Compute)) == 4);
	HYP_CHECK((EShaderStageMask::Vertex | EShaderStageMask::Pixel) == EShaderStageMask::Graphics);
	HYP_CHECK(RHIShaderVisibility(EShaderStageMask::Vertex) == ERHIShaderVisibility::Vertex);
	HYP_CHECK(RHIShaderVisibility(EShaderStageMask::Pixel) == ERHIShaderVisibility::Pixel);
	HYP_CHECK(RHIShaderVisibility(EShaderStageMask::Graphics) == ERHIShaderVisibility::Graphics);
	HYP_CHECK(RHIShaderVisibility(EShaderStageMask::Compute) == ERHIShaderVisibility::Compute);
	for (unsigned Bits = 0; Bits < 16; ++Bits)
	{
		const auto Mask = static_cast<EShaderStageMask>(Bits);
		HYP_CHECK(IsGraphicsShaderStages(Mask) == (Bits == 1 || Bits == 2 || Bits == 3));
		if (Bits >= 1 && Bits <= 4)
		{
			HYP_CHECK(ShaderStages(RHIShaderVisibility(Mask)) == Mask);
		}
		else
		{
			Rejects(
			    [&]
			    {
				    RHIShaderVisibility(Mask);
			    });
		}
	}
	HYP_CHECK(HasShaderStage(EShaderStageMask::Graphics, EShaderStage::Vertex));
	HYP_CHECK(HasShaderStage(EShaderStageMask::Graphics, EShaderStage::Pixel));
	HYP_CHECK(!HasShaderStage(EShaderStageMask::Graphics, EShaderStage::Compute));
	HYP_CHECK(!HasAnyShaderStage(EShaderStageMask::Pixel, EShaderStageMask::Vertex));
	Rejects(
	    []
	    {
		    ShaderStageMask(static_cast<EShaderStage>(99));
	    });
	Rejects(
	    []
	    {
		    ShaderStages(static_cast<ERHIShaderVisibility>(99));
	    });
}

void CheckResourceKinds()
{
	struct FMapping
	{
		EBindingKind Source;
		ERHIBindingKind Expected;
		EShaderResourceDimension Dimension = EShaderResourceDimension::None;
	};

	const std::array Mappings{
	    FMapping{EBindingKind::UniformBuffer, ERHIBindingKind::ConstantBuffer},
	    FMapping{EBindingKind::Texture, ERHIBindingKind::Texture2D, EShaderResourceDimension::Texture2D},
	    FMapping{EBindingKind::Texture, ERHIBindingKind::TextureCube, EShaderResourceDimension::TextureCube},
	    FMapping{EBindingKind::Sampler, ERHIBindingKind::Sampler},
	    FMapping{EBindingKind::StructuredBuffer, ERHIBindingKind::StructuredBuffer},
	    FMapping{EBindingKind::RawBuffer, ERHIBindingKind::RawBuffer},
	    FMapping{EBindingKind::StorageTexture, ERHIBindingKind::StorageTexture2D, EShaderResourceDimension::Texture2D},
	    FMapping{EBindingKind::StorageStructuredBuffer, ERHIBindingKind::StorageStructuredBuffer},
	    FMapping{EBindingKind::StorageRawBuffer, ERHIBindingKind::StorageRawBuffer}};
	for (const auto& Mapping : Mappings)
	{
		FShaderBinding Binding{};
		Binding.Kind = Mapping.Source;
		Binding.Dimension = Mapping.Dimension;
		HYP_CHECK(GetShaderBindingKind(Binding) == Mapping.Expected);
	}
	for (const auto Kind : {EBindingKind::Texture, EBindingKind::StorageTexture})
	{
		FShaderBinding Binding{};
		Binding.Kind = Kind;
		Binding.Dimension = EShaderResourceDimension::Unsupported;
		Rejects(
		    [&]
		    {
			    GetShaderBindingKind(Binding);
		    });
		Binding.Dimension = EShaderResourceDimension::Texture2D;
		Binding.ResourceScalar = EShaderScalar::Uint;
		Rejects(
		    [&]
		    {
			    GetShaderBindingKind(Binding);
		    });
	}
	FShaderBinding Binding{};
	Binding.Kind = EBindingKind::Unsupported;
	Rejects(
	    [&]
	    {
		    GetShaderBindingKind(Binding);
	    });
}

void CheckCoverage()
{
	FPipelineDesc Pipeline;
	FShaderBinding Binding{};
	Binding.Name = "Values";
	Binding.Kind = EBindingKind::StructuredBuffer;
	Binding.Register = 3;
	Binding.Space = 2;
	Binding.Count = 2;
	Binding.StructureByteStride = 16;
	Pipeline.Vertex.Bindings = {Binding};
	FResourceBindingSlot Slot;
	Slot.Kind = ERHIBindingKind::StructuredBuffer;
	Slot.Visibility = ERHIShaderVisibility::Vertex;
	Slot.Register = 2;
	Slot.Space = 2;
	Slot.Count = 3;
	Slot.StructureByteStride = 16;
	ValidatePipelineBindings(Pipeline, {{Slot}});
	for (unsigned Case = 0; Case < 5; ++Case)
	{
		auto Bad = Slot;
		switch (Case)
		{
			case 0:
				Bad.Space = 1;
				break;
			case 1:
				Bad.Count = 2;
				break;
			case 2:
				Bad.Visibility = ERHIShaderVisibility::Pixel;
				break;
			case 3:
				Bad.StructureByteStride = 32;
				break;
			case 4:
				Bad.Kind = ERHIBindingKind::RawBuffer;
				break;
		}
		Rejects(
		    [&]
		    {
			    ValidatePipelineBindings(Pipeline, {{Bad}});
		    });
	}
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Pipeline, {});
	    });
	Pipeline.Vertex.Bindings.front().StructureByteStride = 0;
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Pipeline, {{Slot}});
	    });
}

void CheckConstantsAndSamplers()
{
	FPipelineDesc Pipeline;
	FShaderBinding Binding{};
	Binding.Kind = EBindingKind::UniformBuffer;
	Binding.ByteSize = 64;
	Pipeline.Vertex.Bindings = {Binding};
	FResourceBindingSlot Slot;
	Slot.MinimumBufferSize = 64;
	ValidatePipelineBindings(Pipeline, {{Slot}});
	Slot.MinimumBufferSize = 63;
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Pipeline, {{Slot}});
	    });
	Slot.MinimumBufferSize = 64;
	Slot.InstanceStride = 16;
	Slot.InstanceCapacity = 4;
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Pipeline, {{Slot}});
	    });
	FShaderMember Record;
	Record.Kind = EShaderValueKind::Structure;
	FShaderMember Array;
	Array.Kind = EShaderValueKind::Array;
	Array.ArrayStride = 16;
	Array.ArrayCount = 4;
	Array.Members = {Record};
	Pipeline.Vertex.Bindings.front().Members = {Array};
	ValidatePipelineBindings(Pipeline, {{Slot}});
	Pipeline.Vertex.Bindings.front().Members.front().ArrayStride = 32;
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Pipeline, {{Slot}});
	    });
	Binding.Kind = EBindingKind::Sampler;
	Binding.bComparison = true;
	Pipeline.Vertex.Bindings = {Binding};
	Slot = {};
	Slot.Kind = ERHIBindingKind::Sampler;
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Pipeline, {{Slot}});
	    });
	Slot.bComparison = true;
	ValidatePipelineBindings(Pipeline, {{Slot}});
	FComputePipelineDesc Compute;
	Compute.Compute.Stage = EShaderStage::Compute;
	Compute.Compute.Bindings = {Binding};
	Rejects(
	    [&]
	    {
		    ValidatePipelineBindings(Compute, {{Slot}});
	    });
	Slot.Visibility = ERHIShaderVisibility::Compute;
	ValidatePipelineBindings(Compute, {{Slot}});
}
} // namespace

int main()
{
	try
	{
		CheckStages();
		CheckResourceKinds();
		CheckCoverage();
		CheckConstantsAndSamplers();
		std::cout << "PASS: typed shader stages and backend-independent binding contracts\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
