#pragma once
#include "Hyperion/Renderer/MaterialProviders.h"
#include <bit>
#include <map>

namespace Hyperion
{
// Render-owned by the primitive. Replaced on publication; frames receive immutable resolved values.
struct FMaterialEvaluationCache
{
	struct FObjectEntry
	{
		FMaterialScopeInput Scope;
		FMaterialParameterValues Inputs;
		FMaterialParameterValues Overrides;
	};

	std::map<std::uint64_t, FObjectEntry> Objects;

	struct FEntry
	{
		std::shared_ptr<const FMaterialSnapshot> Snapshot;
		std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
		std::string Usage;
		FMaterialProviderInputs Inputs;
		FMaterialParameterValues Object;
		FMaterialParameterValues Draw;
		FMaterialParameterValues ObjectInputs;
		FMat4 World;
		bool bClipSpace{};
		std::shared_ptr<const FResolvedMaterialParameters> Resolved;
		std::uint32_t Dependencies{};

		bool Matches(const FMaterialProviderInputs& InInputs, const FMaterialParameterValues& InObject,
		             const FMaterialParameterValues& InDraw, const FMat4& InWorld, bool bInClipSpace,
		             const FMaterialParameterValues& InObjectInputs) const
		{
			const auto Transient = MaterialScopeBit(EMaterialScope::Frame) | MaterialScopeBit(EMaterialScope::Pass) |
			                       MaterialScopeBit(EMaterialScope::Draw);
			if ((Dependencies & Transient) || Object != InObject || Draw != InDraw)
			{
				return false;
			}
			if ((Dependencies & MaterialScopeBit(EMaterialScope::Object)) &&
			    (std::bit_cast<std::array<std::uint32_t, 16>>(World.Values) !=
			         std::bit_cast<std::array<std::uint32_t, 16>>(InWorld.Values) ||
			     bClipSpace != bInClipSpace || ObjectInputs != InObjectInputs))
			{
				return false;
			}
			for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
			{
				// Object is compared above before derived matrices are built; snapshot identity guards Material.
				if (Scope == static_cast<std::size_t>(EMaterialScope::Object) ||
				    Scope == static_cast<std::size_t>(EMaterialScope::Material))
				{
					continue;
				}
				if ((Dependencies & (1U << Scope)) && (Inputs.Scopes[Scope].Key != InInputs.Scopes[Scope].Key ||
				                                       Inputs.Values[Scope] != InInputs.Values[Scope]))
				{
					return false;
				}
			}
			return true;
		}
	};

	std::map<std::pair<std::uint64_t, std::uint64_t>, FEntry> Entries;
};
} // namespace Hyperion
