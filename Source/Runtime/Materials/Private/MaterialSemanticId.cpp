#include "Hyperion/Materials/MaterialSemanticId.h"
#include "Hyperion/Materials/MaterialBlocks.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include <stdexcept>

namespace Hyperion
{
std::string_view GetEngineSemanticName(EEngineSemantic InSemantic)
{
	if (InSemantic == EEngineSemantic::None)
	{
		return {};
	}
	if (InSemantic >= EEngineSemantic::Count)
	{
		throw std::invalid_argument("Invalid builtin semantic identity");
	}
	return GetEngineShaderSemantics().at(static_cast<std::size_t>(InSemantic) - 1).Name;
}

EEngineSemantic FindEngineSemantic(std::string_view InName)
{
	const auto& Semantics = GetEngineShaderSemantics();
	for (std::size_t Index = 0; Index < Semantics.size(); ++Index)
	{
		const auto& Semantic = Semantics[Index];
		if (Semantic.Name == InName ||
		    std::find(Semantic.Aliases.begin(), Semantic.Aliases.end(), InName) != Semantic.Aliases.end())
		{
			return static_cast<EEngineSemantic>(Index + 1);
		}
	}
	return EEngineSemantic::None;
}

FMaterialSemanticId::FMaterialSemanticId(EEngineSemantic InBuiltin) : Builtin(InBuiltin)
{
	GetEngineSemanticName(InBuiltin);
}

FMaterialSemanticId::FMaterialSemanticId(std::string_view InName) : Builtin(FindEngineSemantic(InName))
{
	if (Builtin == EEngineSemantic::None)
	{
		const auto Standard = FindStandardShaderSemantic(InName);
		Descriptor = Standard.GetDescriptor();
		if (!Descriptor)
		{
			Custom = InName;
		}
	}
}

std::string_view FMaterialSemanticId::GetName() const
{
	return Descriptor                         ? std::string_view(Descriptor->Name)
	       : Builtin == EEngineSemantic::None ? std::string_view(Custom)
	                                          : GetEngineSemanticName(Builtin);
}
} // namespace Hyperion
