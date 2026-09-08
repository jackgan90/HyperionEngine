#include "Hyperion/Renderer/MaterialPacking.h"
#include "Renderer/InstanceBatchSupport.h"
#include <cstring>

namespace Hyperion::InstanceTests
{
namespace
{
template<typename T> T Read(std::span<const std::byte> InBytes, std::size_t InOffset)
{
	HYP_CHECK(InOffset + sizeof(T) <= InBytes.size());
	T Result;
	std::memcpy(&Result, InBytes.data() + InOffset, sizeof(T));
	return Result;
}

void CheckPacking(const FCompiledMaterialDefinition& InCompiled)
{
	const auto& Pass = *InCompiled.FindInstancePass();
	HYP_CHECK(Pass.InstanceCapacity == 4);
	const auto Binding = std::find_if(Pass.Bindings.begin(), Pass.Bindings.end(),
	                                  [](const auto& InBinding)
	                                  {
		                                  return InBinding.InstanceStride != 0;
	                                  });
	HYP_CHECK(Binding != Pass.Bindings.end());
	FMaterialInstance Instance(InCompiled.Interface);
	const auto Values = ResolveMaterialBindingContext(Instance.Freeze(), InCompiled, Pass, {});
	const auto Bytes = PackMaterialConstants(*Binding, Values.Values);
	HYP_CHECK(Bytes.size() == Binding->InstanceStride);
	if (Pass.Vertex.Format == EShaderFormat::Dxil)
	{
		HYP_CHECK(Bytes.size() == 112);
		HYP_CHECK(Read<float>(Bytes, 12) == 1);
		HYP_CHECK(Read<float>(Bytes, 16) == .2f);
		HYP_CHECK(Read<std::uint32_t>(Bytes, 28) == 3);
		HYP_CHECK(Read<std::uint32_t>(Bytes, 32) == 1);
		HYP_CHECK(Read<std::int32_t>(Bytes, 36) == -2);
		HYP_CHECK(Read<float>(Bytes, 52) == 2 && Read<float>(Bytes, 64) == 3);
		HYP_CHECK(Read<float>(Bytes, 88) == 3 && Read<float>(Bytes, 96) == 4 && Read<float>(Bytes, 104) == 6);
	}
}

void CheckMalformed(FShaderCompiler& InCompiler)
{
	for (const auto& Declaration :
	     std::vector<FMaterialInstanceArray>{{"Missing", "Records"}, {"InstanceData", "Wrong"}})
	{
		auto Desc = Surface()->Definition->GetDescription();
		Desc.Passes.front().InstanceArrays = {Declaration};
		const auto Definition = std::make_shared<const FMaterialDefinition>(std::move(Desc));
		const auto Compiled = CompileMaterialDefinition(InCompiler, Definition, EShaderFormat::Dxil);
		HYP_CHECK(!Compiled.FindInstancePass() && !Compiled.InstanceDiagnostics.empty());
		HYP_CHECK(!Compiled.GetPass().Vertex.Bytes.empty());
	}
}
} // namespace

void RunInstanceContractTests(FShaderCompiler& InCompiler)
{
	const auto Definition = Surface()->Definition;
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Compiled = CompileMaterialDefinition(InCompiler, Definition, Format);
		if (!Compiled.FindInstancePass())
		{
			throw std::runtime_error(Compiled.InstanceDiagnostics.front());
		}
		HYP_CHECK(Compiled.GetPass().InstanceCapacity == 1);
		HYP_CHECK(Compiled.GetPass().Vertex.CacheKey != Compiled.FindInstancePass()->Vertex.CacheKey);
		CheckPacking(Compiled);
	}
	auto Fixed = Definition->GetDescription();
	Fixed.Passes.front().Vertex.Defines = {{"FIXED_ZERO", "1"}};
	Fixed.Passes.front().Pixel.Defines = {{"FIXED_ZERO", "1"}};
	const auto FixedProgram =
	    CompileMaterialDefinition(InCompiler, std::make_shared<const FMaterialDefinition>(Fixed), EShaderFormat::Dxil);
	HYP_CHECK(!FixedProgram.FindInstancePass() && FixedProgram.InstanceDiagnostics.size() == 1);
	const auto Requested = CompileMaterialDefinition(InCompiler, Definition, EShaderFormat::Dxil,
	                                                 {{"Forward", "Default", {{"FIXED_ZERO", "1"}}}});
	HYP_CHECK(!Requested.FindInstancePass() && Requested.InstanceDiagnostics.size() == 1);
	Fixed.Passes.front().InstanceArrays.clear();
	const auto Ordinary =
	    CompileMaterialDefinition(InCompiler, std::make_shared<const FMaterialDefinition>(Fixed), EShaderFormat::Dxil);
	HYP_CHECK(Ordinary.Passes.size() == 1 && !Ordinary.FindInstancePass());
	CheckMalformed(InCompiler);
}
} // namespace Hyperion::InstanceTests
