#include "Renderer/InstanceBatchSupport.h"
#include <algorithm>

namespace Hyperion::InstanceTests
{
namespace
{
FMaterialValue& Default(FMaterialDescription& InDescription, const std::string& InName)
{
	const auto Parameter = std::find_if(InDescription.Parameters.begin(), InDescription.Parameters.end(),
	                                    [&](const auto& InParameter)
	                                    {
		                                    return InParameter.Name == InName;
	                                    });
	HYP_CHECK(Parameter != InDescription.Parameters.end() && Parameter->Default);
	return *Parameter->Default;
}

FRenderBatchCandidate Candidate(FFixture& InFixture, FMaterialDescription InDescription)
{
	auto Item = Snapshot(InFixture, 1).Items.Front();
	Item.State.Surface = InFixture.Material(std::move(InDescription));
	const auto Compiled = Item.State.Surface->GetCompiled();
	Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(
	    ResolveMaterialBindingContext(Item.State.Surface->GetSnapshot(), *Compiled, Compiled->GetPass(), Item.Context));
	return DescribeBatchCandidate(Item, InFixture.View, {false, ERHIDepthFormat::D32S8});
}

void CheckPackingCompatibility(FFixture& InFixture)
{
	auto Items = Snapshot(InFixture, 2);
	const std::array<std::size_t, 2> Indices{0, 1};
	const auto Baseline = PackInstanceBatch(Items, Indices);
	const auto Original = InFixture.Resource->GetMaterial(0)->GetSnapshot()->Definition->GetDescription();
	const auto Select = [&](FMaterialDescription InDescription)
	{
		auto& Item = Items.Items[1];
		Item.State.Surface = InFixture.Material(std::move(InDescription));
		const auto Program = Item.State.Surface->GetCompiled();
		Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
		    Item.State.Surface->GetSnapshot(), *Program, Program->GetPass(), Item.Context));
	};
	auto Reordered = Original;
	std::reverse(Reordered.Parameters.begin(), Reordered.Parameters.end());
	Select(std::move(Reordered));
	const auto Compatible = PackInstanceBatch(Items, Indices);
	HYP_CHECK(Compatible->Constants.size() == Baseline->Constants.size());
	for (std::size_t Index = 0; Index < Baseline->Constants.size(); ++Index)
	{
		HYP_CHECK(*Compatible->Constants[Index].Bytes == *Baseline->Constants[Index].Bytes);
	}
	auto Renamed = Original;
	const auto Placement = std::find_if(Renamed.Parameters.begin(), Renamed.Parameters.end(),
	                                    [](const auto& InParameter)
	                                    {
		                                    return InParameter.Name == "Placement";
	                                    });
	HYP_CHECK(Placement != Renamed.Parameters.end());
	Placement->Name = "OtherPlacement";
	Items.Items[1].Context.ObjectParameters.front().Name = "OtherPlacement";
	Select(std::move(Renamed));
	bool bRejected{};
	try
	{
		(void)PackInstanceBatch(Items, Indices);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

void RunInstanceResourceTests(FFixture& InFixture)
{
	CheckPackingCompatibility(InFixture);
	const auto Original = InFixture.Resource->GetMaterial(0)->GetSnapshot()->Definition->GetDescription();
	const auto Base = Candidate(InFixture, Original);
	HYP_CHECK(Base.Signature == Candidate(InFixture, Original).Signature);
	auto Changed = Original;
	Default(Changed, "SharedTint") = FMaterialValue::Float(FVec4{.5f, 1, 1, 1});
	HYP_CHECK(!(Base.Signature == Candidate(InFixture, Changed).Signature));
	Changed = Original;
	Default(Changed, "ReadData").Buffer.Offset = 16;
	HYP_CHECK(!(Base.Signature == Candidate(InFixture, Changed).Signature));
	Changed = Original;
	Default(Changed, "ReadData").Buffer.Size = 32;
	HYP_CHECK(!(Base.Signature == Candidate(InFixture, Changed).Signature));
	Changed = Original;
	Default(Changed, "MapSampler").Sampler.W = EMaterialAddressMode::Clamp;
	HYP_CHECK(!(Base.Signature == Candidate(InFixture, Changed).Signature));
	Changed = Original;
	Default(Changed, "MapSampler").Sampler.BorderColor[2] = .5f;
	HYP_CHECK(!(Base.Signature == Candidate(InFixture, Changed).Signature));
	Changed = Original;
	Default(Changed, "MapSampler").Sampler.MaxLod = 4;
	HYP_CHECK(!(Base.Signature == Candidate(InFixture, Changed).Signature));
	Changed = Original;
	const auto Texture = std::make_shared<const FMaterialTextureSource>(
	    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {255, 255, 255, 255}}});
	Default(Changed, "Maps").Elements[1] = FMaterialValue::FromTexture(Texture);
	const auto NewSource = Candidate(InFixture, Changed);
	HYP_CHECK(!(Base.Signature == NewSource.Signature));
	std::swap(Default(Changed, "Maps").Elements[0], Default(Changed, "Maps").Elements[1]);
	HYP_CHECK(!(NewSource.Signature == Candidate(InFixture, Changed).Signature));
}
} // namespace Hyperion::InstanceTests
