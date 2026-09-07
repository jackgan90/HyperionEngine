#include "Hyperion/Renderer/MaterialProviders.h"
#include "Support/TestSupport.h"

using namespace Hyperion;

namespace
{
void SetScope(FMaterialProviderInputs& InInputs, EMaterialScope InScope, std::uint64_t InId,
              FMaterialParameterValues InValues)
{
	const FMaterialScopeKey Key{InId, 1};
	const auto Index = static_cast<std::size_t>(InScope);
	InInputs.Scopes[Index] = {Key, std::make_shared<const FMaterialScopeKey>(Key)};
	InInputs.Values[Index] = std::move(InValues);
}

void CheckProviderHistory()
{
	FMaterialProviderRegistry Providers;
	Providers.Freeze();
	FMaterialProviderInputs Inputs;
	SetScope(Inputs, EMaterialScope::Object, 1, {});
	const auto Object = static_cast<std::size_t>(EMaterialScope::Object);
	const std::vector<std::string> Names{"Engine.Object.World", "Engine.Object.Normal",
	                                     "Engine.Object.OrientationSign"};
	for (std::uint32_t Index = 0; Index < 2048; ++Index)
	{
		Inputs.Values[Object] = {
		    {"Engine.Object.World", FMaterialValue::Matrix(Identity())},
		    {"Engine.Object.Normal", FMaterialValue::Matrix(Identity())},
		    {"Engine.Object.OrientationSign", FMaterialValue::Float(1)},
		    {"Engine.Object.WorldViewProjection", FMaterialValue::Matrix(Translation({float(Index), 0, 0}))}};
		HYP_CHECK(Providers.Evaluate(Inputs, Names).size() == 3);
		Providers.Collect();
		HYP_CHECK(Providers.Statistics().CachedEntries == 3);
	}
	HYP_CHECK(Providers.Statistics().Evaluations[Object] == 3);
	Inputs.Scopes[Object] = {};
	Providers.Collect();
	HYP_CHECK(Providers.Statistics().CachedEntries == 0);
}

void CheckAbsentProviderDependencies()
{
	auto Semantics = std::make_shared<FMaterialSemanticRegistry>();
	Semantics->Register(
	    {"Test.Exposure", FMaterialParameterType::Numeric(EMaterialScalar::Float), EMaterialScope::Global, "Exposure"});
	FMaterialProviderRegistry Providers(Semantics);
	const auto Dependencies = MaterialScopeBit(EMaterialScope::Global) | MaterialScopeBit(EMaterialScope::Scene);
	Providers.Register({"Test.Exposure", Dependencies, [](const auto& InInputs) -> std::optional<FMaterialValue>
	                    {
		                    const auto Value = InInputs.Find(EMaterialScope::Scene, "Input");
		                    return Value ? std::optional<FMaterialValue>(*Value) : std::nullopt;
	                    }});
	Providers.Freeze();
	FMaterialProviderInputs Inputs;
	SetScope(Inputs, EMaterialScope::Global, 1, {});
	SetScope(Inputs, EMaterialScope::Scene, 2, {});
	const std::vector<std::string> Names{"Test.Exposure"};
	const auto Missing = Providers.Evaluate(Inputs, Names);
	HYP_CHECK(Missing.size() == 1 && !Missing[0].Value && Missing[0].Dependencies == Dependencies);
	FMaterialDescription Description;
	Description.Name = "Absent custom provider dependencies";
	FMaterialPass Pass;
	Pass.Vertex = {"Test.hlsl", "VSMain"};
	Pass.Pixel = {"Test.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	Description.Parameters = {DeclareMaterialSemantic("Exposure", "Test.Exposure", *Semantics)};
	Description.Parameters[0].Default = FMaterialValue::Float(1);
	auto Definition = std::make_shared<const FMaterialDefinition>(Description, Semantics);
	FCompiledMaterialDefinition Compiled;
	Compiled.Interface = {Definition, std::make_shared<const FMaterialParameterSchema>(Description.Parameters)};
	FCompiledMaterialPass Program;
	Program.ActiveParameters = {0};
	FMaterialInstance Instance(Compiled.Interface);
	FMaterialBindingContext Context;
	Context.Scopes = Inputs.Scopes;
	Context.Providers = Missing;
	const auto Fallback = ResolveMaterialBindingContext(Instance.Freeze(), Compiled, Program, Context);
	HYP_CHECK((Fallback.Dependencies[0] & Dependencies) == Dependencies);
	HYP_CHECK(Fallback.Values[0] == FMaterialValue::Float(1));
	const std::array<std::byte, 4> Bytes{};
	std::vector<std::weak_ptr<const FMaterialReadBufferSource>> Sources;
	for (std::uint32_t Index = 0; Index < 64; ++Index)
	{
		auto Source = std::make_shared<const FMaterialReadBufferSource>(Bytes);
		Sources.push_back(Source);
		Inputs.Values[static_cast<std::size_t>(EMaterialScope::Scene)] = {
		    {"Input", FMaterialValue::Float(float(Index))},
		    {"Unused", FMaterialValue::FromBuffer({Source, EMaterialBufferViewKind::Raw, 0, 4, 0})}};
		HYP_CHECK(Providers.Evaluate(Inputs, Names)[0].Value == FMaterialValue::Float(float(Index)));
		Providers.Collect();
		HYP_CHECK(Providers.Statistics().CachedEntries == 1);
	}
	HYP_CHECK(Sources.front().expired() && !Sources.back().expired());
}
} // namespace

void RunMaterialProviderTests()
{
	CheckProviderHistory();
	CheckAbsentProviderDependencies();
	auto Semantics = std::make_shared<FMaterialSemanticRegistry>();
	Semantics->Register({"Experiment.Exposure", FMaterialParameterType::Numeric(EMaterialScalar::Float),
	                     EMaterialScope::Global, "Global exposure multiplier"});
	Semantics->Freeze();
	FMaterialProviderRegistry Providers(Semantics);
	std::uint32_t Evaluations{};
	Providers.Register({"Experiment.Exposure", MaterialScopeBit(EMaterialScope::Global),
	                    [&](const FMaterialProviderInputs& InInputs) -> std::optional<FMaterialValue>
	                    {
		                    ++Evaluations;
		                    HYP_CHECK(!InInputs.Find(EMaterialScope::Object, "ObjectValue"));
		                    const auto* Value = InInputs.Find(EMaterialScope::Global, "ExposureInput");
		                    return Value ? std::optional<FMaterialValue>(*Value) : std::nullopt;
	                    }});
	Providers.Freeze();
	FMaterialProviderInputs Inputs;
	SetScope(Inputs, EMaterialScope::Global, 1, {{"ExposureInput", FMaterialValue::Float(2)}});
	SetScope(Inputs, EMaterialScope::View, 2, {{"Engine.View.CameraPosition", FMaterialValue::Float(FVec3{1, 2, 3})}});
	SetScope(Inputs, EMaterialScope::Object, 3, {{"ObjectValue", FMaterialValue::Float(3)}});
	const std::vector<std::string> Names{"Experiment.Exposure", "Engine.View.CameraPosition"};
	const auto First = Providers.Evaluate(Inputs, Names);
	HYP_CHECK(First.size() == 2 && Evaluations == 1);
	for (std::uint64_t Index = 4; Index < 10; ++Index)
	{
		SetScope(Inputs, EMaterialScope::Object, Index,
		         {{"ObjectValue", FMaterialValue::Float(static_cast<float>(Index))}});
		const auto Values = Providers.Evaluate(Inputs, Names);
		HYP_CHECK(Values.size() == 2 && Values[0].Value == First[0].Value && Values[1].Value == First[1].Value);
	}
	HYP_CHECK(Evaluations == 1 &&
	          Providers.Statistics().Evaluations[static_cast<std::size_t>(EMaterialScope::View)] == 1);
	Inputs.Values[static_cast<std::size_t>(EMaterialScope::Global)].front().Value = FMaterialValue::Float(4);
	HYP_CHECK(Providers.Evaluate(Inputs, Names).front().Value == FMaterialValue::Float(4));
	HYP_CHECK(Evaluations == 2);
	Inputs.Scopes[static_cast<std::size_t>(EMaterialScope::View)] = {};
	const auto MissingView = Providers.Evaluate(Inputs, Names);
	HYP_CHECK(MissingView.size() == 2 && !MissingView[1].Value);
	bool bRejected = false;
	try
	{
		Providers.Register({"Experiment.Exposure", 1, [](const auto&)
		                    {
			                    return std::optional<FMaterialValue>{};
		                    }});
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Inputs.Values[0].push_back(Inputs.Values[0].front());
	bRejected = false;
	try
	{
		Providers.Evaluate(Inputs, Names);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
