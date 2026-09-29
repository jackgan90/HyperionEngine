#include "Hyperion/Renderer/TransientGeometry.h"
#include <set>
#include <tuple>

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

void FTransientGeometry::AddModelInstance(FRenderPrimitiveState InState,
                                          std::shared_ptr<const FRenderMaterial> InFallback)
{
	const auto Surface = InState.Resource ? InState.Resource->GetMaterial(InState.Section) : nullptr;
	if (Surface && Surface->GetStatus() == ERenderMaterialStatus::Ready)
	{
		InState.Surface = Surface;
		SceneItems.push_back(std::move(InState));
	}
	else
	{
		InState.Surface = std::move(InFallback);
		Items.push_back(std::move(InState));
	}
}

void AppendTransientSceneItems(FRenderSceneSnapshot& InSnapshot, const FTransientGeometry& InGeometry)
{
	std::set<std::tuple<std::uint64_t, std::uint32_t, std::uint64_t>> Replaced;
	for (const auto& Handle : InGeometry.ReplacedPrimitives)
	{
		Replaced.emplace(Handle.Scene, Handle.Slot, Handle.Generation);
	}
	if (!Replaced.empty())
	{
		FRenderItemList Retained;
		for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
		{
			const auto& Handle = InSnapshot.Items[Index].Primitive;
			if (!Replaced.contains({Handle.Scene, Handle.Slot, Handle.Generation}))
			{
				Retained.MoveFrom(InSnapshot.Items, Index);
			}
		}
		InSnapshot.Items = std::move(Retained);
	}
	if (InSnapshot.View.Usage == "ShadowDepth")
	{
		InSnapshot.Statistics.VisibleItems = InSnapshot.Items.Size();
		return;
	}
	for (const auto& State : InGeometry.SceneItems)
	{
		FRenderItem Item;
		Item.State = State;
		Item.Lifetime = InGeometry.Lifetime;
		Item.Primitive = {0x7472616e7369656e, static_cast<std::uint32_t>(InSnapshot.Items.Size()), 1};
		InSnapshot.Items.PushBack(std::move(Item));
	}
	InSnapshot = PrepareSceneSnapshot(std::move(InSnapshot));
}

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
	InSnapshot.Batches = Batches.Build(InSnapshot, InSnapshot.View.bInstanceBatching);
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
