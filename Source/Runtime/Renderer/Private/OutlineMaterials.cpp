#include "OutlineMaterials.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
std::shared_ptr<const FMaterialDefinition> MaskDefinition(const FCompiledMaterialDefinition& InProgram)
{
	const auto& Source = *InProgram.Interface.Definition;
	auto Description = Source.GetDescription();
	FMaterialPass Pass;
	if (Source.HasPass("SilhouetteMask"))
	{
		Pass = Source.GetPass("SilhouetteMask");
	}
	else
	{
		const auto Found = std::find_if(
		    Description.Passes.begin(), Description.Passes.end(),
		    [](const auto& InPass)
		    {
			    return (InPass.Vertex.Path == "Model.hlsl" || InPass.Vertex.Path == "/Engine/Shaders/Model.hlsl") &&
			           InPass.Vertex.Entry == "VSMain" && InPass.Pixel.Path == InPass.Vertex.Path &&
			           InPass.Pixel.Entry == "PSMain" && InPass.Vertex.Defines.empty();
		    });
		if (Found == Description.Passes.end())
		{
			return {};
		}
		Pass = *Found;
		Pass.Vertex.Defines = {{"HYP_SILHOUETTE_MASK", "1"}};
		Pass.Pixel.Defines = Pass.Vertex.Defines;
	}
	Pass.Usage = "SilhouetteMask";
	Pass.Queue = EMaterialQueue::Opaque;
	Pass.bSrgbTarget = false;
	Pass.bAllowBatchReordering = true;
	Pass.bAllowDynamicOverrides = false;
	Pass.State.bDepthTest = false;
	Pass.State.bDepthWrite = false;
	Pass.State.bStencil = false;
	Pass.State.bBlend = false;
	Pass.State.Fill = EMaterialFill::Solid;
	Pass.State.ColorWriteMask = 1;
	Description.Name += " silhouette";
	// Preserve reflected inputs, including overrides unused by the mask. Its compiler determines activity.
	Description.Parameters = InProgram.Interface.Schema->GetParameters();
	Description.Passes = {std::move(Pass)};
	return std::make_shared<const FMaterialDefinition>(
	    std::move(Description), std::make_shared<const FMaterialSemanticRegistry>(Source.GetSemantics()));
}
} // namespace

void FOutlineMaterials::BeginFrame()
{
	for (auto& [Key, Entry] : Entries)
	{
		Entry.bUsed = false;
	}
	for (auto& [Key, Entry] : Definitions)
	{
		Entry.bUsed = false;
	}
}

std::shared_ptr<const FRenderMaterial> FOutlineMaterials::Resolve(FRenderResourceService& InResources,
                                                                  std::shared_ptr<const FRenderMaterial> InSource,
                                                                  bool& bOutPending)
{
	bOutPending = false;
	if (!InSource)
	{
		bOutPending = true;
		return {};
	}
	const auto Program = InSource->GetCompiled();
	if (!Program)
	{
		const auto Status = InSource->GetStatus();
		bOutPending = Status != ERenderMaterialStatus::Failed && Status != ERenderMaterialStatus::Retired;
		return {};
	}
	const auto& Source = InSource->GetSnapshot();
	auto& Definition = Definitions[Program.get()];
	if (!Definition.Source)
	{
		Definition.Source = Program;
		Definition.Mask = MaskDefinition(*Program);
	}
	Definition.bUsed = true;
	auto& Entry = Entries[Source.get()];
	Entry.bUsed = true;
	if (!Entry.Source)
	{
		Entry.Source = Source;
		if (Definition.Mask)
		{
			auto Mask = std::make_shared<FMaterialSnapshot>(*Source);
			Mask->Definition = Definition.Mask;
			Mask->Schema = Definition.Mask->GetSchema();
			Entry.Material = InResources.RequestAuxiliaryMaterial(std::move(Mask));
		}
	}
	if (!Entry.Material || Entry.Material->GetStatus() == ERenderMaterialStatus::Failed)
	{
		if (!Entry.bReported)
		{
			Log(ELogLevel::Warning,
			    "Selection outline: " + Source->Definition->GetDescription().Name + ": " +
			        (Entry.Material ? Entry.Material->GetError() : "requires a SilhouetteMask pass"));
			Entry.bReported = true;
		}
		return {};
	}
	bOutPending = Entry.Material->GetStatus() != ERenderMaterialStatus::Ready;
	return bOutPending ? nullptr : Entry.Material;
}

void FOutlineMaterials::EndFrame()
{
	std::erase_if(Entries,
	              [](const auto& InEntry)
	              {
		              return !InEntry.second.bUsed;
	              });
	std::erase_if(Definitions,
	              [](const auto& InEntry)
	              {
		              return !InEntry.second.bUsed;
	              });
}

void FOutlineMaterials::Reset()
{
	Entries.clear();
	Definitions.clear();
}
} // namespace Hyperion
