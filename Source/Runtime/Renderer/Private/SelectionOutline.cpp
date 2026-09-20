#include "Hyperion/Renderer/SelectionOutline.h"
#include "Hyperion/Core/Profiling.h"
#include "OutlineMaterials.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
class FSelectionOutlineFeature final : public IRenderFeature
{
public:
	explicit FSelectionOutlineFeature(FRHICapabilities InCapabilities) : Capabilities(std::move(InCapabilities))
	{
		if (!Capabilities.SampledColorTargets.at(static_cast<std::size_t>(ERHIColorFormat::R8Unorm)))
		{
			throw std::invalid_argument("Selection outlines require sampled R8 render targets");
		}
	}

	void Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext) override;
	void Reset() override;

	std::uint64_t ResourceBytes() const override
	{
		return std::uint64_t(Width) * Height * (Scale * Scale + 1);
	}

private:
	void Resize(FRenderResourceService& InResources, const FRenderView& InView, bool bInSupersample);
	void AddObject(FRenderFeatureContext& InContext, const FSelectionOutlineSettings& InSettings,
	               std::span<const FRenderPrimitiveHandle> InHandles, bool bInFirst);
	FOutlineMaterials Materials;
	FRHICapabilities Capabilities;
	FRenderTargetSource Mask;
	FRenderTargetSource Outline;
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::uint32_t Scale = 1;
};

void FSelectionOutlineFeature::Reset()
{
	Mask = {};
	Outline = {};
	Width = Height = 0;
	Materials.Reset();
}

void FSelectionOutlineFeature::Resize(FRenderResourceService& InResources, const FRenderView& InView,
                                      bool bInSupersample)
{
	const std::uint32_t RequestedScale = bInSupersample && InView.Width <= Capabilities.MaxTextureDimension / 2 &&
	                                             InView.Height <= Capabilities.MaxTextureDimension / 2
	                                         ? 2
	                                         : 1;
	if (Width == InView.Width && Height == InView.Height && Scale == RequestedScale)
	{
		return;
	}
	Width = InView.Width;
	Height = InView.Height;
	Scale = RequestedScale;
	const auto Lifetime = InResources.CreateScopeLifetime();
	Mask = {ERenderTargetKind::Texture,
	        std::make_shared<const FMaterialTextureSource>(
	            FMaterialColorTexture{Width * Scale, Height * Scale, EMaterialColorFormat::R8Unorm}),
	        Lifetime, false};
	Outline = {ERenderTargetKind::Texture,
	           std::make_shared<const FMaterialTextureSource>(
	               FMaterialColorTexture{Width, Height, EMaterialColorFormat::R8Unorm}),
	           Lifetime, false};
}

void FSelectionOutlineFeature::AddObject(FRenderFeatureContext& InContext, const FSelectionOutlineSettings& InSettings,
                                         std::span<const FRenderPrimitiveHandle> InHandles, bool bInFirst)
{
	auto View = InContext.View;
	View.Identity = 0x6f75746c696e65;
	View.Usage = "SilhouetteMask";
	View.ExcludedPasses.clear();
	View.CullingMode = ESceneCullingMode::None; // Raster clipping is sufficient; selected occluded geometry survives.
	View.Width *= Scale;
	View.Height *= Scale;
	if (View.Viewport)
	{
		View.Viewport->X *= Scale;
		View.Viewport->Y *= Scale;
		View.Viewport->Width *= Scale;
		View.Viewport->Height *= Scale;
	}
	auto Snapshot = InContext.Session.GetScene().CollectPrimitives(View, InHandles);
	FRenderItemList Items;
	for (auto& Item : Snapshot.Items)
	{
		if (!Item.State.bVisible || !Item.State.Resource)
		{
			continue;
		}
		const auto Source =
		    Item.State.Surface ? Item.State.Surface : Item.State.Resource->GetMaterial(Item.State.Section);
		bool bPending{};
		const auto Surface = Materials.Resolve(InContext.Session.GetResources(), Source, bPending);
		if (!Surface)
		{
			if (bPending)
			{
				++InContext.Statistics.SelectionOutline.PendingItems;
			}
			else
			{
				++InContext.Statistics.SelectionOutline.UnsupportedItems;
			}
			continue;
		}
		Item.State.Surface = Surface;
		Item.Preparation.reset();
		Item.DynamicState.reset();
		Items.PushBack(std::move(Item));
	}
	Snapshot.Items = std::move(Items);
	InContext.Statistics.SelectionOutline.Items += Snapshot.Items.Size();
	Snapshot.Frame = InContext.FrameOwner;
	const auto Group = std::to_string(InContext.Statistics.SelectionOutline.MaskPasses);
	Snapshot.Targets.Name = "Outline/Mask/" + Group;
	Snapshot.Targets.Color = FRenderColorTarget{Mask, {EAttachmentLoad::Clear}, {}, EGraphColorView::Linear};
	InContext.Session.AppendTransientGeometry(InContext.Graph, std::move(Snapshot), InContext.bDeferPreparation);
	++InContext.Statistics.SelectionOutline.MaskPasses;
	const auto Viewport = InContext.View.Viewport.value_or(FViewport{0, 0, float(Width), float(Height)});
	AddSilhouetteOutlinePass(InContext.Session, InContext.Graph, Mask, Outline, Viewport, InSettings.Width, bInFirst,
	                         InContext.bDeferPreparation, "Outline/Exterior/" + Group);
}

void FSelectionOutlineFeature::Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext)
{
	if (InStage != ERenderFeatureStage::AfterTonemap)
	{
		return;
	}
	HYP_PERF_SCOPE_C(Render, SelectionOutline);
	const auto Request = InContext.SelectionOutline;
	if (!Request || Request->Objects.empty())
	{
		Reset();
		return;
	}
	if (Request->Publication != InContext.Frame.GetSceneToken())
	{
		InContext.Statistics.SelectionOutline.bRejectedPublication = true;
		Reset();
		return;
	}
	const auto& Settings = Request->Settings;
	if ((Settings.Overlap != EOutlineOverlapMode::Union && Settings.Overlap != EOutlineOverlapMode::PerObject) ||
	    !std::isfinite(Settings.Width) || Settings.Width <= 0 || Settings.Width > 8 ||
	    !std::isfinite(Settings.Color.X) || !std::isfinite(Settings.Color.Y) || !std::isfinite(Settings.Color.Z) ||
	    !std::isfinite(Settings.Color.W))
	{
		throw std::invalid_argument("Invalid selection outline settings");
	}
	Resize(InContext.Session.GetResources(), InContext.View, Settings.bSupersample);
	Materials.BeginFrame();
	bool bFirst = true;
	std::vector<FRenderPrimitiveHandle> Union;
	for (const auto& Object : Request->Objects)
	{
		if (Object.empty())
		{
			continue;
		}
		++InContext.Statistics.SelectionOutline.Objects;
		if (Settings.Overlap == EOutlineOverlapMode::Union)
		{
			Union.insert(Union.end(), Object.begin(), Object.end());
		}
		else
		{
			AddObject(InContext, Settings, Object, bFirst);
			bFirst = false;
		}
	}
	if (!Union.empty())
	{
		AddObject(InContext, Settings, Union, true);
		bFirst = false;
	}
	Materials.EndFrame();
	if (bFirst)
	{
		Reset();
		return;
	}
	AddOutlineCompositePass(InContext.Session, InContext.Graph, Outline, InContext.Resources.Output,
	                        InContext.View.Viewport.value_or(FViewport{0, 0, float(Width), float(Height)}),
	                        Settings.Color, InContext.bDeferPreparation);
}
} // namespace

std::unique_ptr<IRenderFeature> MakeSelectionOutlineFeature(FRHICapabilities InCapabilities)
{
	return std::make_unique<FSelectionOutlineFeature>(std::move(InCapabilities));
}
} // namespace Hyperion
