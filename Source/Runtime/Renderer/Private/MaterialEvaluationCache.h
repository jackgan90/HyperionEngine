#pragma once
#include "Hyperion/Renderer/MaterialProviders.h"
#include <bit>
#include <map>
#include <set>
#include <tuple>

namespace Hyperion
{
struct FSceneItemPreparation;
struct FMaterialSharedBinding;

// Render-owned by the primitive. Replaced on publication; frames receive immutable resolved values.
struct FMaterialEvaluationCache
{
	struct FObjectEntry
	{
		FMaterialScopeInput Scope;
		FMaterialParameterValues Inputs;
		FMaterialParameterValues Overrides;
		std::uint64_t AccessFrame{};
	};

	std::map<std::uint64_t, FObjectEntry> Objects;

	void TouchObject(std::uint64_t InLocalItemId, std::uint64_t InFrame)
	{
		if (const auto It = Objects.find(InLocalItemId); It != Objects.end())
		{
			It->second.AccessFrame = InFrame;
		}
	}

	struct FEntry
	{
		std::shared_ptr<const FMaterialSnapshot> Snapshot;
		std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
		std::string Usage;
		std::shared_ptr<const FMaterialProviderInputs> Inputs;
		FMaterialParameterValues Object;
		FMaterialParameterValues Draw;
		FMaterialParameterValues ObjectInputs;
		FMat4 World;
		bool bClipSpace{};
		std::shared_ptr<const FResolvedMaterialParameters> Resolved;
		std::shared_ptr<const FMaterialSharedParameters> Shared;
		std::shared_ptr<const FMaterialSharedBinding> SharedBinding;
		std::vector<std::size_t> SharedResources;
		bool bSeparateShared{};
		std::uint32_t Dependencies{};
		std::vector<std::size_t> ProviderParameters;
		std::vector<std::size_t> SharedProviderParameters;
		std::shared_ptr<const std::vector<std::size_t>> SharedGroup;
		std::weak_ptr<const FSceneItemPreparation> Preparation;
		std::uint64_t AccessFrame{};

		bool MatchesEngine(const FMaterialProviderInputs& InInputs, std::uint32_t InIgnoredScopes = 0) const
		{
			if (Dependencies & ~InIgnoredScopes & MaterialScopeBit(EMaterialScope::Draw))
			{
				return false;
			}
			for (std::size_t Scope = 0; Scope < MaterialScopeCount; ++Scope)
			{
				if ((FMaterialResolvedScopes::EngineMask & Dependencies & ~InIgnoredScopes & (1U << Scope)) &&
				    (Inputs->Scopes[Scope].Key != InInputs.Scopes[Scope].Key ||
				     Inputs->Scopes[Scope].Lifetime != InInputs.Scopes[Scope].Lifetime ||
				     Inputs->Values[Scope] != InInputs.Values[Scope]))
				{
					return false;
				}
			}
			return true;
		}

		bool Matches(const FMaterialProviderInputs& InInputs, const FMaterialParameterValues& InObject,
		             const FMaterialParameterValues& InDraw, const FMat4& InWorld, bool bInClipSpace,
		             const FMaterialParameterValues& InObjectInputs, std::uint32_t InIgnoredScopes = 0) const
		{
			if ((Dependencies & ~InIgnoredScopes & MaterialScopeBit(EMaterialScope::Draw)) || Object != InObject ||
			    Draw != InDraw)
			{
				return false;
			}
			if ((Dependencies & ~InIgnoredScopes & MaterialScopeBit(EMaterialScope::Object)) &&
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
				if ((Dependencies & ~InIgnoredScopes & (1U << Scope)) &&
				    (Inputs->Scopes[Scope].Key != InInputs.Scopes[Scope].Key ||
				     Inputs->Scopes[Scope].Lifetime != InInputs.Scopes[Scope].Lifetime ||
				     Inputs->Values[Scope] != InInputs.Values[Scope]))
				{
					return false;
				}
			}
			return true;
		}
	};

	std::map<std::pair<std::uint64_t, std::uint64_t>, FEntry> Entries;
};
struct FRenderItem;
struct FRenderView;
struct FRenderSceneSnapshot;
class FRenderResourceService;

// One view preparation owns immutable engine scopes. Share their provider results across its objects.
struct FViewMaterialProviders
{
	struct FRefresh
	{
		std::shared_ptr<const FMaterialValueTable::FSharedValues> Values;
		std::shared_ptr<const FMaterialDependencyTable::FSharedValues> Dependencies;
		std::shared_ptr<const FMaterialSharedParameters> Parameters;
		std::shared_ptr<const std::vector<std::size_t>> Group;
	};

	struct FRefreshKey
	{
		const FCompiledMaterialPass* Pass;
		std::vector<std::size_t> Indices;
	};

	struct FRefreshLookup
	{
		const FCompiledMaterialPass* Pass;
		std::span<const std::size_t> Indices;
	};

	struct FRefreshOrder
	{
		using is_transparent = void; // NOLINT(readability-identifier-naming): standard associative lookup contract.

		template<typename TLeft, typename TRight> bool operator()(const TLeft& InLeft, const TRight& InRight) const
		{
			if (InLeft.Pass != InRight.Pass)
			{
				return std::less<const FCompiledMaterialPass*>{}(InLeft.Pass, InRight.Pass);
			}
			return std::lexicographical_compare(InLeft.Indices.begin(), InLeft.Indices.end(), InRight.Indices.begin(),
			                                    InRight.Indices.end());
		}
	};

	FMaterialProviderRegistry& Registry;
	std::map<std::string, FMaterialProvidedValue, std::less<>> Shared;
	std::map<FRefreshKey, FRefresh, FRefreshOrder> Refreshes;
	std::map<const std::vector<std::size_t>*, FRefresh> GroupRefreshes;
	std::set<const void*> ValidatedSharedBases;
	std::map<std::uint32_t, std::shared_ptr<const FMaterialProviderInputs>> RetainedInputs;
	FRefresh UncachedRefresh;
	using FBindingKey = std::tuple<const FCompiledMaterialPass*, const void*, const void*>;
	std::map<FBindingKey, std::shared_ptr<const FMaterialSharedBinding>> SharedBindings;
	std::vector<
	    std::pair<std::shared_ptr<const FMaterialSharedBinding>, std::shared_ptr<const FMaterialSharedParameters>>>
	    BindingUpdates;
	std::shared_ptr<const FMaterialSharedParameters> UncachedBindingUpdate;
	std::size_t BindingEvaluations{};
	std::vector<std::weak_ptr<const FMaterialSharedBinding>>* BindingHistory{};
	std::shared_ptr<const FMaterialSharedBinding> RetainSharedBinding(const FMaterialEvaluationCache::FEntry& InEntry);
	const std::shared_ptr<const FMaterialSharedParameters>& UpdateSharedBinding(
	    const std::shared_ptr<const FMaterialSharedBinding>& InBinding, const FMaterialProviderInputs& InInputs);
	std::shared_ptr<const FMaterialProviderInputs> RetainInputs(const FMaterialProviderInputs& InInputs,
	                                                            std::uint32_t InDependencies);
	FMaterialProvidedValue Evaluate(const FMaterialProviderInputs& InInputs, std::string_view InSemantic);
	const FRefresh& PrepareShared(const FMaterialEvaluationCache::FEntry& InEntry, const FCompiledMaterialPass& InPass,
	                              const FMaterialProviderInputs& InInputs);
};

void FillMaterialObjectInputs(FMaterialProviderInputs& InInputs, const FRenderItem& InItem,
                              const FRenderSceneSnapshot& InSnapshot, const FRenderResourceService& InResources);
void FillMaterialDrawInputs(FMaterialProviderInputs& InInputs, const FRenderItem& InItem,
                            const FRenderSceneSnapshot& InSnapshot);
bool RefreshMaterialEvaluation(FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot,
                               const FMaterialProviderInputs& InInputs, const FRenderResourceService& InResources,
                               const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                               FViewMaterialProviders& InProviders);
void PrepareSharedMaterialEligibility(FMaterialEvaluationCache::FEntry& InEntry);
bool ShareMaterialEvaluation(FRenderItem& InItem, const FRenderSceneSnapshot& InSnapshot,
                             const FMaterialProviderInputs& InInputs,
                             const std::shared_ptr<const FCompiledMaterialDefinition>& InCompiled,
                             FViewMaterialProviders& InProviders);
} // namespace Hyperion
