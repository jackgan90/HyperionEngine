#include "Renderer/InstanceBatchSupport.h"
#include <algorithm>

namespace Hyperion::InstanceTests
{
namespace
{
template<typename TAction> void ExpectInvalid(TAction InAction)
{
	bool bRejected = false;
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckIndependentModes(FShaderCompiler& InCompiler, const std::shared_ptr<const FMaterialDefinition>& InDefinition,
                           EShaderFormat InFormat)
{
	const auto Automatic = CompileMaterialDefinition(InCompiler, InDefinition, InFormat);
	const auto Ordinary = CompileMaterialDefinition(InCompiler, InDefinition, InFormat, {{"Forward", "Instance"}});
	HYP_CHECK(Ordinary.Passes.size() == 1 && !Ordinary.FindInstancePass());
	const auto& OrdinaryPass = Ordinary.GetPass("Forward", "Instance");
	HYP_CHECK(OrdinaryPass.ExecutionMode == EMaterialExecutionMode::Ordinary && OrdinaryPass.InstanceCapacity == 1);
	HYP_CHECK(OrdinaryPass.Vertex.CacheKey == Automatic.GetPass().Vertex.CacheKey);
	HYP_CHECK(OrdinaryPass.Pixel.CacheKey == Automatic.GetPass().Pixel.CacheKey);
	HYP_CHECK(std::none_of(OrdinaryPass.Bindings.begin(), OrdinaryPass.Bindings.end(),
	                       [](const auto& InBinding)
	                       {
		                       return InBinding.InstanceStride != 0;
	                       }));
	const auto Instance = CompileMaterialDefinition(InCompiler, InDefinition, InFormat,
	                                                {{"Forward", "Crowd", {}, EMaterialExecutionMode::Instanced}});
	const auto& InstancePass = Instance.GetInstancePass();
	HYP_CHECK(&InstancePass == &Instance.GetPass("Forward", "Crowd"));
	HYP_CHECK(InstancePass.ExecutionMode == EMaterialExecutionMode::Instanced && InstancePass.InstanceCapacity == 4);
	HYP_CHECK(InstancePass.Vertex.CacheKey == Automatic.GetInstancePass().Vertex.CacheKey);
	HYP_CHECK(InstancePass.Pixel.CacheKey == Automatic.GetInstancePass().Pixel.CacheKey);
	HYP_CHECK(std::all_of(Instance.Interface.Mappings.begin(), Instance.Interface.Mappings.end(),
	                      [](const auto& InMapping)
	                      {
		                      return InMapping.Variant == "Crowd";
	                      }));
	HYP_CHECK(&Instance.GetDrawPass("Forward", EMaterialExecutionMode::Instanced) == &InstancePass);
	const auto SameName = CompileMaterialDefinition(InCompiler, InDefinition, InFormat, {{"Forward", "Crowd"}});
	HYP_CHECK(SameName.Key != Instance.Key);
	HYP_CHECK(SameName.GetPass("Forward", "Crowd").Vertex.CacheKey != InstancePass.Vertex.CacheKey);
	ExpectInvalid(
	    [&]
	    {
		    Ordinary.GetInstancePass();
	    });
}

void CheckSelectionAndCache(FShaderCompiler& InCompiler, const std::shared_ptr<const FMaterialDefinition>& InDefinition)
{
	std::vector<FMaterialVariantRequest> Requests{{"Forward", "Default"},
	                                              {"Forward", "Crowd", {}, EMaterialExecutionMode::Instanced}};
	const auto First = CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil, Requests);
	HYP_CHECK(First.Passes.size() == 2 && First.InstanceDiagnostics.empty());
	HYP_CHECK(&First.GetDrawPass("Forward", EMaterialExecutionMode::Ordinary) == &First.GetPass());
	std::reverse(Requests.begin(), Requests.end());
	const auto Reordered = CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil, Requests);
	HYP_CHECK(First.Key == Reordered.Key);
	Requests.front().Name = "Renamed";
	const auto Renamed = CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil, Requests);
	HYP_CHECK(Renamed.Key != First.Key);
	HYP_CHECK(Renamed.GetInstancePass().Vertex.CacheKey == First.GetInstancePass().Vertex.CacheKey);
	const auto Collision = CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil,
	                                                 {{"Forward", "Default"}, {"Forward", "Instance"}});
	HYP_CHECK(Collision.Passes.size() == 2 && !Collision.FindInstancePass());
	HYP_CHECK(Collision.InstanceDiagnostics.size() == 1);
	HYP_CHECK(Collision.GetPass("Forward", "Instance").ExecutionMode == EMaterialExecutionMode::Ordinary);
	const auto ReverseName = CompileMaterialDefinition(
	    InCompiler, InDefinition, EShaderFormat::Dxil,
	    {{"Forward", "Default", {{"HYP_ENABLE_INSTANCE", "0"}}, EMaterialExecutionMode::Instanced}});
	HYP_CHECK(ReverseName.Passes.size() == 1 && ReverseName.GetInstancePass().Variant == "Default");
	HYP_CHECK(&ReverseName.GetPass("Forward", "Default") == &ReverseName.GetInstancePass());
	ExpectInvalid(
	    [&]
	    {
		    ReverseName.GetPass();
	    });
	ExpectInvalid(
	    [&]
	    {
		    ReverseName.GetDrawPass("Forward", EMaterialExecutionMode::Ordinary);
	    });
	const auto OrdinaryOverride = CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil,
	                                                        {{"Forward", "Instance", {{"HYP_ENABLE_INSTANCE", "1"}}}});
	HYP_CHECK(!OrdinaryOverride.FindInstancePass());
	HYP_CHECK(OrdinaryOverride.GetPass("Forward", "Instance").Vertex.CacheKey == First.GetPass().Vertex.CacheKey);
}

void CheckInvalidModes(FShaderCompiler& InCompiler, const std::shared_ptr<const FMaterialDefinition>& InDefinition)
{
	const auto Invalid = static_cast<EMaterialExecutionMode>(255);
	for (const auto& Requests : std::vector<std::vector<FMaterialVariantRequest>>{
	         {{"Forward", "Bad", {}, Invalid}},
	         {{"Forward", "Same"}, {"Forward", "Same", {}, EMaterialExecutionMode::Instanced}},
	         {{"Forward", "A", {}, EMaterialExecutionMode::Instanced},
	          {"Forward", "B", {}, EMaterialExecutionMode::Instanced}},
	         {{"Forward", "", {}, EMaterialExecutionMode::Instanced}},
	         {{"Forward", "Unsupported", {{"FIXED_ZERO", "1"}}, EMaterialExecutionMode::Instanced}}})
	{
		ExpectInvalid(
		    [&]
		    {
			    CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil, Requests);
		    });
	}
	const auto MultipleOrdinary =
	    CompileMaterialDefinition(InCompiler, InDefinition, EShaderFormat::Dxil, {{"Forward", "A"}, {"Forward", "B"}});
	HYP_CHECK(MultipleOrdinary.Passes.size() == 2 && !MultipleOrdinary.FindInstancePass());
	ExpectInvalid(
	    [&]
	    {
		    MultipleOrdinary.GetDrawPass("Forward", Invalid);
	    });
	auto Description = InDefinition->GetDescription();
	Description.Passes.front().InstanceArrays.clear();
	const auto MissingArrays = std::make_shared<const FMaterialDefinition>(std::move(Description));
	ExpectInvalid(
	    [&]
	    {
		    CompileMaterialDefinition(InCompiler, MissingArrays, EShaderFormat::Dxil,
		                              {{"Forward", "Crowd", {}, EMaterialExecutionMode::Instanced}});
	    });
}
} // namespace

void RunMaterialExecutionTests(FShaderCompiler& InCompiler)
{
	const auto Definition = Surface()->Definition;
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		CheckIndependentModes(InCompiler, Definition, Format);
	}
	CheckSelectionAndCache(InCompiler, Definition);
	CheckInvalidModes(InCompiler, Definition);
}
} // namespace Hyperion::InstanceTests
