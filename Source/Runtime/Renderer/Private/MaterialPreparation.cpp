#include "Hyperion/Renderer/MaterialPreparation.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "MaterialInterfaceBuilder.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::string_view ExecutionModeKey(EMaterialExecutionMode InMode)
{
	switch (InMode)
	{
		case EMaterialExecutionMode::Ordinary:
			return "ordinary";
		case EMaterialExecutionMode::Instanced:
			return "instanced";
		default:
			throw std::invalid_argument("Invalid material execution mode");
	}
}

void AppendVariantKey(std::string& OutKey, const FCompiledMaterialPass& InPass)
{
	OutKey += "/" + std::to_string(InPass.Usage.size()) + ":" + InPass.Usage + std::to_string(InPass.Variant.size()) +
	          ":" + InPass.Variant + "/" + std::string(ExecutionModeKey(InPass.ExecutionMode)) + "/" +
	          InPass.Vertex.CacheKey + "/" + InPass.Pixel.CacheKey;
}

FShaderCompileOptions CompileOptions(const FMaterialShader& InShader, const FMaterialVariantRequest& InVariant,
                                     std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	FShaderCompileOptions Result;
	for (auto& Include : GenerateShaderIncludes(InContracts))
	{
		Result.VirtualIncludes.push_back({std::move(Include.first), std::move(Include.second)});
	}
	for (const FMaterialShaderDefine& Define : InShader.Defines)
	{
		Result.Defines.push_back({Define.Name, Define.Value});
	}
	Result.Defines.insert(Result.Defines.end(), InVariant.Defines.begin(), InVariant.Defines.end());
	std::erase_if(Result.Defines,
	              [](const auto& InDefine)
	              {
		              return InDefine.Name == "HYP_ENABLE_INSTANCE";
	              });
	Result.Defines.push_back(
	    {"HYP_ENABLE_INSTANCE", InVariant.ExecutionMode == EMaterialExecutionMode::Instanced ? "1" : "0"});
	return Result;
}

FCompiledMaterialPass CompileVariant(FShaderCompiler& InCompiler, const FMaterialDefinition& InDefinition,
                                     EShaderFormat InFormat, const FMaterialVariantRequest& InVariant,
                                     const FShaderSourceSnapshot& InSources, FMaterialInterfaceBuilder& InBuilder)
{
	const FMaterialPass& Pass = InDefinition.GetPass(InVariant.Usage);
	FCompiledMaterialPass Compiled;
	Compiled.Usage = InVariant.Usage;
	Compiled.Variant = InVariant.Name;
	Compiled.VariantDefines = InVariant.Defines;
	Compiled.ExecutionMode = InVariant.ExecutionMode;
	Compiled.Vertex = InCompiler.Compile(Pass.Vertex.Path, Pass.Vertex.Entry, EShaderStage::Vertex, InFormat,
	                                     CompileOptions(Pass.Vertex, InVariant, InBuilder.GetContracts()), InSources);
	InBuilder.BindStage(Compiled, Compiled.Vertex, Pass);
	if (!Pass.Pixel.Path.empty())
	{
		Compiled.Pixel = InCompiler.Compile(Pass.Pixel.Path, Pass.Pixel.Entry, EShaderStage::Pixel, InFormat,
		                                    CompileOptions(Pass.Pixel, InVariant, InBuilder.GetContracts()), InSources);
		InBuilder.BindStage(Compiled, Compiled.Pixel, Pass);
	}
	Compiled.Bindings = MergeMaterialBindings(std::move(Compiled.Bindings));
	std::sort(Compiled.ActiveParameters.begin(), Compiled.ActiveParameters.end());
	Compiled.ActiveParameters.erase(std::unique(Compiled.ActiveParameters.begin(), Compiled.ActiveParameters.end()),
	                                Compiled.ActiveParameters.end());
	if (InVariant.ExecutionMode == EMaterialExecutionMode::Instanced)
	{
		Compiled.InstanceCapacity = UINT32_MAX;
		for (const auto& Array : Pass.InstanceArrays)
		{
			const auto Binding = std::find_if(Compiled.Bindings.begin(), Compiled.Bindings.end(),
			                                  [&](const auto& InBinding)
			                                  {
				                                  return InBinding.Resource.Name == Array.Block;
			                                  });
			if (Binding == Compiled.Bindings.end() || !Binding->InstanceStride)
			{
				throw std::invalid_argument("Missing material instance block: " + Array.Block);
			}
			Compiled.InstanceCapacity = std::min(Compiled.InstanceCapacity, Binding->InstanceCapacity);
		}
		const bool bInstanceId =
		    std::any_of(Compiled.Vertex.Reflection.Inputs.begin(), Compiled.Vertex.Reflection.Inputs.end(),
		                [](const auto& InInput)
		                {
			                return InInput.bSystemValue &&
			                       (InInput.Semantic == "SV_InstanceID" || InInput.Semantic == "SV_INSTANCEID");
		                });
		if (!bInstanceId || Pass.InstanceArrays.empty() || Compiled.InstanceCapacity < 2)
		{
			throw std::invalid_argument("HYP_ENABLE_INSTANCE=1 must expose SV_InstanceID and instance constant arrays");
		}
	}
	return Compiled;
}

void CompileOptionalInstances(FShaderCompiler& InCompiler, const FMaterialDefinition& InDefinition,
                              EShaderFormat InFormat, const FShaderSourceSnapshot& InSources,
                              FMaterialInterfaceBuilder& InBuilder, FCompiledMaterialDefinition& OutResult)
{
	const auto Count = OutResult.Passes.size();
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		const auto& Default = OutResult.Passes[Index];
		if (Default.Variant != MaterialVariants::Default || Default.ExecutionMode != EMaterialExecutionMode::Ordinary ||
		    OutResult.FindInstancePass(Default.Usage))
		{
			continue;
		}
		const auto& Description = InDefinition.GetPass(Default.Usage);
		if (Description.InstanceArrays.empty())
		{
			continue;
		}
		auto Trial = InBuilder;
		try
		{
			if (std::any_of(OutResult.Passes.begin(), OutResult.Passes.end(),
			                [&](const auto& InPass)
			                {
				                return InPass.Usage == Default.Usage && InPass.Variant == MaterialVariants::Instance;
			                }))
			{
				throw std::invalid_argument("Optional instance variant identity is already occupied");
			}
			auto Instance = CompileVariant(
			    InCompiler, InDefinition, InFormat,
			    {Default.Usage, MaterialVariants::Instance, Default.VariantDefines, EMaterialExecutionMode::Instanced},
			    InSources, Trial);
			if (!std::includes(Default.ActiveParameters.begin(), Default.ActiveParameters.end(),
			                   Instance.ActiveParameters.begin(), Instance.ActiveParameters.end()))
			{
				throw std::invalid_argument("Instance permutation requires inputs unavailable to the ordinary pass");
			}
			AppendVariantKey(OutResult.Key, Instance);
			OutResult.Passes.push_back(std::move(Instance));
			InBuilder = std::move(Trial);
		}
		catch (const std::exception& Error)
		{
			OutResult.InstanceDiagnostics.push_back(Description.Usage + ": " + Error.what());
		}
	}
}
} // namespace

FCompiledMaterialDefinition CompileMaterialDefinition(FShaderCompiler& InCompiler,
                                                      std::shared_ptr<const FMaterialDefinition> InDefinition,
                                                      EShaderFormat InFormat,
                                                      std::vector<FMaterialVariantRequest> InVariants,
                                                      EMaterialEngineBindingMode InEngineMode)
{
	if (!InDefinition)
	{
		throw std::invalid_argument("Material preparation requires a definition");
	}
	if (InVariants.empty())
	{
		for (const FMaterialPass& Pass : InDefinition->GetDescription().Passes)
		{
			InVariants.push_back({Pass.Usage});
		}
	}
	std::sort(InVariants.begin(), InVariants.end(),
	          [](const FMaterialVariantRequest& InA, const FMaterialVariantRequest& InB)
	          {
		          return std::tie(InA.Usage, InA.Name) < std::tie(InB.Usage, InB.Name);
	          });
	std::set<std::pair<std::string, std::string>> Variants;
	std::set<std::string> InstanceUsages;
	std::vector<std::filesystem::path> SourcePaths;
	for (const FMaterialVariantRequest& Variant : InVariants)
	{
		(void)ExecutionModeKey(Variant.ExecutionMode);
		if (Variant.Name.empty() || !Variants.emplace(Variant.Usage, Variant.Name).second)
		{
			throw std::invalid_argument("Duplicate or empty material variant");
		}
		if (Variant.ExecutionMode == EMaterialExecutionMode::Instanced && !InstanceUsages.insert(Variant.Usage).second)
		{
			throw std::invalid_argument("Ambiguous material instance candidate");
		}
		const auto& Pass = InDefinition->GetPass(Variant.Usage);
		SourcePaths.push_back(Pass.Vertex.Path);
		if (!Pass.Pixel.Path.empty())
		{
			SourcePaths.push_back(Pass.Pixel.Path);
		}
	}
	const auto Sources = InCompiler.CaptureSources(SourcePaths);
	FMaterialInterfaceBuilder Builder(InDefinition, InEngineMode);
	FCompiledMaterialDefinition Result;
	Result.Key = "material-v3/contract" + std::to_string(GetEngineSemanticContractVersion()) + "/" +
	             std::to_string(static_cast<unsigned>(InEngineMode)) + "/" +
	             std::to_string(InDefinition->GetIdentity()) + "/" +
	             std::to_string(InDefinition->GetDescription().Version);
	for (const FMaterialVariantRequest& Variant : InVariants)
	{
		auto Compiled = CompileVariant(InCompiler, *InDefinition, InFormat, Variant, Sources, Builder);
		AppendVariantKey(Result.Key, Compiled);
		Result.Passes.push_back(std::move(Compiled));
	}
	CompileOptionalInstances(InCompiler, *InDefinition, InFormat, Sources, Builder, Result);
	Result.Interface = Builder.Finish();
	return Result;
}

TAsyncResult<FCompiledMaterialDefinition> PrepareMaterialDefinition(
    FTaskSystem& InTasks, std::shared_ptr<FShaderCompiler> InCompiler,
    std::shared_ptr<const FMaterialDefinition> InDefinition, EShaderFormat InFormat,
    std::vector<FMaterialVariantRequest> InVariants, FCancellationToken InCancellation)
{
	InTasks.Require({EDomain::Main});
	if (!InCompiler || !InDefinition)
	{
		throw std::invalid_argument("Material preparation requires owned compiler and definition");
	}
	return DispatchAsync<FCompiledMaterialDefinition>(
	    InTasks, {EDomain::Worker},
	    [Compiler = std::move(InCompiler), Definition = std::move(InDefinition), InFormat,
	     Variants = std::move(InVariants)]
	    {
		    return CompileMaterialDefinition(*Compiler, Definition, InFormat, Variants);
	    },
	    InCancellation);
}
} // namespace Hyperion
