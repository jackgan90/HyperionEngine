#pragma once
#include "Hyperion/Renderer/RenderResources.h"
#include <map>

namespace Hyperion
{
class FOutlineMaterials
{
public:
	void BeginFrame();
	std::shared_ptr<const FRenderMaterial> Resolve(FRenderResourceService& InResources,
	                                               std::shared_ptr<const FRenderMaterial> InSource, bool& bOutPending);
	void EndFrame();
	void Reset();

private:
	struct FEntry
	{
		std::shared_ptr<const FMaterialSnapshot> Source;
		std::shared_ptr<const FRenderMaterial> Material;
		bool bUsed{};
		bool bReported{};
	};

	struct FDefinition
	{
		std::shared_ptr<const FCompiledMaterialDefinition> Source;
		std::shared_ptr<const FMaterialDefinition> Mask;
		bool bUsed{};
	};

	std::map<const FCompiledMaterialDefinition*, FDefinition> Definitions;
	std::map<const FMaterialSnapshot*, FEntry> Entries;
};
} // namespace Hyperion
