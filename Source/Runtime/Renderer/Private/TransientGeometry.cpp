#include "Hyperion/Renderer/TransientGeometry.h"

namespace Hyperion
{
namespace
{
class FTransientGeometryFeature final : public IRenderFeature
{
public:
	void Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext) override
	{
		if (InStage != ERenderFeatureStage::BeforeTonemap || !InContext.TransientGeometry ||
		    InContext.TransientGeometry->Items.empty())
		{
			return;
		}
		FRenderSceneSnapshot Snapshot;
		Snapshot.View = InContext.View;
		Snapshot.View.Identity = 0x7472616e7369656e;
		Snapshot.View.Usage = "GeometryPreview";
		Snapshot.View.ExcludedPasses.clear();
		Snapshot.View.bInstanceBatching = false;
		Snapshot.Frame = InContext.FrameOwner;
		Snapshot.Targets.Name = "Transient geometry";
		Snapshot.Targets.Color =
		    FRenderColorTarget{InContext.Resources.Color, {EAttachmentLoad::Load}, {}, EGraphColorView::Linear};
		Snapshot.Targets.DepthStencil = FRenderDepthTarget{InContext.Resources.Depth, ERHIDepthFormat::D32,
		                                                   FAttachmentActions{EAttachmentLoad::Load}};
		for (const auto& State : InContext.TransientGeometry->Items)
		{
			FRenderItem Item;
			Item.State = State;
			Item.Lifetime = InContext.TransientGeometry->Lifetime;
			Item.Primitive = {Snapshot.View.Identity, static_cast<std::uint32_t>(Snapshot.Items.Size()), 1};
			Snapshot.Items.PushBack(std::move(Item));
		}
		InContext.Session.AppendTransientGeometry(InContext.Graph, std::move(Snapshot), InContext.bDeferPreparation);
	}
};
} // namespace

std::unique_ptr<IRenderFeature> MakeTransientGeometryFeature()
{
	return std::make_unique<FTransientGeometryFeature>();
}

std::shared_ptr<const FMaterialSnapshot> MakeGeometryPreviewMaterial()
{
	FMaterialDescription Description;
	Description.Name = "Geometry preview";
	FMaterialPass Pass;
	Pass.Usage = "GeometryPreview";
	Pass.Vertex = {"GeometryPreview.hlsl", "VSMain"};
	Pass.Pixel = {"GeometryPreview.hlsl", "PSMain"};
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = true;
	Pass.State.bViewRelativeDepth = true;
	Pass.State.Cull = EMaterialCull::None;
	Description.Passes.push_back(std::move(Pass));
	return FMaterialInstance(std::make_shared<FMaterialDefinition>(std::move(Description))).Freeze();
}

void FRenderSession::AppendTransientGeometry(FRenderGraph& InGraph, FRenderSceneSnapshot InSnapshot,
                                             bool bInDeferPreparation)
{
	Tasks.Require({EDomain::Render});
	if (!InSnapshot.Frame)
	{
		throw std::invalid_argument("Transient geometry requires an owned material frame");
	}
	ValidateSceneFrame(*InSnapshot.Frame);
	InSnapshot = PrepareSceneSnapshot(std::move(InSnapshot));
	PrepareMaterials(InSnapshot, false);
	const auto Snapshot = std::make_shared<const FRenderSceneSnapshot>(std::move(InSnapshot));
	const auto Preparation = Resources.GetPreparation();
	auto Pass = Preparation.DeclarePass(InGraph, *Snapshot);
	Pass.Prepare = [Preparation, Snapshot]
	{
		return Preparation.BuildDraws(*Snapshot);
	};
	if (!bInDeferPreparation)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Pass.Batches = Pass.Prepare();
		                          }));
		Pass.Prepare = {};
	}
	InGraph.Add(std::move(Pass));
}
} // namespace Hyperion
