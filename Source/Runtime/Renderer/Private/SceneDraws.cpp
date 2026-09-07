#include "RenderResourcesInternal.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FModelConstants
{
	FMat4 World;
	FMat4 ViewProjection;
	FMat4 Normal;
	FVec4 Camera;
	FVec4 BaseColor;
	FVec4 EmissiveAndNormal;
	FVec4 Pbr;
	FVec4 Modes;
	FVec4 UvSets;
	FVec4 Extra;
	std::array<std::byte, 208> Padding{};
};

static_assert(sizeof(FModelConstants) == 512);

FModelConstants ModelConstants(const FRenderItem& InItem, const FRenderView& InView, const FModelMaterial& InMaterial)
{
	FModelConstants Result{};
	Result.World = InItem.State.World;
	Result.ViewProjection = InView.ViewProjection;
	Result.Normal = NormalMatrix(InItem.State.World);
	Result.Camera = {InView.Eye.X, InView.Eye.Y, InView.Eye.Z, 1};
	Result.BaseColor = InItem.State.Material.BaseColor.value_or(InMaterial.BaseColor);
	Result.EmissiveAndNormal = {InMaterial.Emissive.X, InMaterial.Emissive.Y, InMaterial.Emissive.Z,
	                            InMaterial.NormalScale};
	Result.Pbr = {InItem.State.Material.Metallic.value_or(InMaterial.Metallic),
	              InItem.State.Material.Roughness.value_or(InMaterial.Roughness), InMaterial.OcclusionStrength,
	              InMaterial.AlphaCutoff};
	Result.Modes = {static_cast<float>(InMaterial.AlphaMode), InMaterial.bDoubleSided ? 1.f : 0.f,
	                InMaterial.bUnlit ? 1.f : 0.f, Determinant(InItem.State.World) < 0 ? -1.f : 1.f};
	Result.UvSets = {float(InMaterial.BaseColorTexture.TexCoord), float(InMaterial.MetallicRoughnessTexture.TexCoord),
	                 float(InMaterial.NormalTexture.TexCoord), float(InMaterial.OcclusionTexture.TexCoord)};
	Result.Extra = {float(InMaterial.EmissiveTexture.TexCoord), InMaterial.NormalTexture.Image >= 0 ? 1.f : 0.f, 0, 0};
	return Result;
}
} // namespace

FDrawPacket FRenderResourceCoordinator::Draw(const FRenderItem& InItem, const FRenderView& InView)
{
	const auto& Record = *InItem.State.Resource->Record;
	if (Record.Owner != this || Record.Status != ERenderResourceStatus::Ready)
	{
		throw std::invalid_argument("Unready or foreign render resource");
	}
	const auto& Section = Record.Description->Sections.at(InItem.State.Section);
	const auto& Material = Record.Description->Materials[Section.Material];
	FDrawPacket Result;
	Result.Pipeline = Record.Pipelines[Section.Material][Determinant(InItem.State.World) < 0 ? 1 : 0];
	Result.Vertices = Record.Vertices[Section.Geometry];
	Result.Indices = Record.Indices[Section.Geometry];
	Result.VertexStride = Record.Description->Geometries[Section.Geometry].VertexStride;
	Result.IndexCount = Section.IndexCount;
	Result.FirstIndex = Section.FirstIndex;
	Result.Constants = Material.bClipSpace ? InItem.State.World : Multiply(InView.ViewProjection, InItem.State.World);
	if (Material.Pipeline.bMaterialLayout)
	{
		for (std::size_t Slot = 0; Slot < 5; ++Slot)
		{
			Result.MaterialTextures[Slot] = Record.Textures[Material.Textures[Slot]];
		}
	}
	Result.Scissor = {0, 0, static_cast<std::int32_t>(InView.Width), static_cast<std::int32_t>(InView.Height)};
	return Result;
}

std::vector<FColorPass> FRenderResourceService::BuildPasses(const FRenderSceneSnapshot& InSnapshot)
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::vector<FColorPass> Passes;
	{
		std::lock_guard Lock(Owner.Mutex);
		if (Owner.bClosed)
		{
			throw std::logic_error("Render resource service is closed");
		}
		std::vector<FModelConstants> Data;
		bool bDepthInitialized = false;
		for (const auto& Item : InSnapshot.Items)
		{
			const auto& Record = *Item.State.Resource->Record;
			auto Packet = Owner.Draw(Item, InSnapshot.View);
			const auto& Section = Record.Description->Sections.at(Item.State.Section);
			const auto& Material = Record.Description->Materials[Section.Material];
			const bool bDepth = Material.Pipeline.bDepthTest;
			const bool bSrgb = Material.Pipeline.bSrgbTarget;
			if (Passes.empty() || Passes.back().Commands.bUseDepth != bDepth ||
			    Passes.back().Commands.bSrgbTarget != bSrgb)
			{
				FColorPass Pass;
				Pass.Commands.Name = "Scene " + std::to_string(Passes.size());
				Pass.Commands.bUseDepth = bDepth;
				Pass.Commands.bClearDepth = bDepth && !bDepthInitialized;
				Pass.Commands.bSrgbTarget = bSrgb;
				bDepthInitialized |= bDepth;
				Passes.push_back(std::move(Pass));
			}
			if (Material.Pipeline.bMaterialLayout)
			{
				Packet.MaterialConstantOffset = Data.size() * sizeof(FModelConstants);
				Data.push_back(ModelConstants(Item, InSnapshot.View, Material.Parameters));
			}
			Passes.back().Commands.Draws.push_back(std::move(Packet));
		}
		if (!Data.empty())
		{
			Owner.Constants.push_back(Owner.Device.CreateBuffer(std::as_bytes(std::span(Data))));
			for (auto& Pass : Passes)
			{
				for (auto& Packet : Pass.Commands.Draws)
				{
					if (Packet.MaterialTextures[0])
					{
						Packet.MaterialConstants = Owner.Constants.back();
					}
				}
			}
		}
	}
	Owner.Schedule();
	return Passes;
}
} // namespace Hyperion
