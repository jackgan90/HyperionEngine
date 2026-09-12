#include "Hyperion/Materials/Material.h"
#include "MaterialIdentity.h"
#include <algorithm>
#include <limits>
#include <map>

namespace Hyperion
{
namespace
{
void ValidateInterface(const FPreparedMaterialInterface& InInterface)
{
	if (!InInterface.Definition || !InInterface.Schema || !InInterface.Schema->IsPrepared() ||
	    InInterface.Schema->GetVersion() != InInterface.Definition->GetDescription().Version)
	{
		throw std::invalid_argument("Material prepared interface has an invalid definition or schema version");
	}
	for (const FMaterialParameterDeclaration& Parameter : InInterface.Schema->GetParameters())
	{
		if (!Parameter.Semantic.empty() &&
		    InInterface.Definition->GetSemantics().Find(Parameter.Semantic).Type != Parameter.Type)
		{
			throw std::invalid_argument("Prepared material interface semantic type mismatch: " + Parameter.Name);
		}
	}
	for (const FMaterialTargetMapping& Mapping : InInterface.Mappings)
	{
		if (Mapping.ParameterIndex >= InInterface.Schema->GetParameters().size() || Mapping.Target.empty() ||
		    !InInterface.Definition->HasPass(Mapping.Usage))
		{
			throw std::invalid_argument("Invalid prepared material target mapping");
		}
	}
}

void ApplyOverrides(std::vector<std::optional<FMaterialValue>>& InValues, const FMaterialParameterSchema& InSchema,
                    const FMaterialParameterValues& InOverrides, EMaterialScope InScope)
{
	std::vector<bool> Seen(InValues.size());
	for (const FMaterialParameterEntry& Entry : InOverrides)
	{
		const FMaterialParameterHandle Handle = InSchema.Find(Entry.Name);
		if (Seen[Handle.Index])
		{
			throw std::invalid_argument("Duplicate override of material parameter: " + Entry.Name);
		}
		Seen[Handle.Index] = true;
		ValidateMaterialOverride(InSchema.Get(Handle), Entry.Value, InScope);
		InValues[Handle.Index] = Entry.Value;
	}
}
} // namespace

FMaterialInstance::FMaterialInstance(std::shared_ptr<const FMaterialDefinition> InDefinition)
    : Owner(std::this_thread::get_id())
{
	if (!InDefinition)
	{
		throw std::invalid_argument("Material instance requires a definition");
	}
	FMaterialSnapshot Initial;
	Initial.Identity = MaterialsPrivate::NextIdentity();
	Initial.Revision = 1;
	Initial.Schema = InDefinition->GetSchema();
	Initial.Definition = std::move(InDefinition);
	Snapshot = std::make_shared<const FMaterialSnapshot>(std::move(Initial));
}

FMaterialInstance::FMaterialInstance(const FPreparedMaterialInterface& InInterface)
    : FMaterialInstance(InInterface.Definition)
{
	ValidateInterface(InInterface);
	FMaterialSnapshot Initial = *Snapshot;
	Initial.Schema = InInterface.Schema;
	Snapshot = std::make_shared<const FMaterialSnapshot>(std::move(Initial));
}

FMaterialInstance::FMaterialInstance(std::shared_ptr<const FMaterialSnapshot> InSnapshot)
    : Owner(std::this_thread::get_id())
{
	if (!InSnapshot || !InSnapshot->Definition || !InSnapshot->Schema || !InSnapshot->Identity || !InSnapshot->Revision)
	{
		throw std::invalid_argument("Cannot clone an incomplete material snapshot");
	}
	for (const auto& Entry : InSnapshot->Overrides)
	{
		ValidateMaterialOverride(InSnapshot->Schema->Get(InSnapshot->Schema->Find(Entry.Name)), Entry.Value,
		                         EMaterialScope::Material);
	}
	auto Initial = *InSnapshot;
	Initial.Identity = MaterialsPrivate::NextIdentity();
	Initial.Revision = 1;
	Snapshot = std::make_shared<const FMaterialSnapshot>(std::move(Initial));
}

void FMaterialInstance::CheckOwner() const
{
	if (Owner != std::this_thread::get_id())
	{
		throw std::logic_error("Material instance is Main-owner only; use an immutable snapshot on other domains");
	}
}

std::uint64_t FMaterialInstance::GetIdentity() const
{
	CheckOwner();
	return Snapshot->Identity;
}

std::uint64_t FMaterialInstance::GetRevision() const
{
	CheckOwner();
	return Snapshot->Revision;
}

std::shared_ptr<const FMaterialSnapshot> FMaterialInstance::Freeze() const
{
	CheckOwner();
	return Snapshot;
}

FMaterialParameterHandle FMaterialInstance::Find(std::string_view InName) const
{
	CheckOwner();
	return Snapshot->Schema->Find(InName);
}

void FMaterialInstance::Publish(FMaterialSnapshot InSnapshot)
{
	if (Snapshot->Revision == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::overflow_error("Material revision exhausted");
	}
	InSnapshot.Revision = Snapshot->Revision + 1;
	Snapshot = std::make_shared<const FMaterialSnapshot>(std::move(InSnapshot));
}

EMaterialWriteResult FMaterialInstance::Set(std::string_view InName, FMaterialValue InValue)
{
	return Set(Find(InName), std::move(InValue));
}

EMaterialWriteResult FMaterialInstance::Set(FMaterialParameterHandle InHandle, FMaterialValue InValue)
{
	CheckOwner();
	const FMaterialParameterDeclaration& Parameter = Snapshot->Schema->Get(InHandle);
	ValidateMaterialOverride(Parameter, InValue, EMaterialScope::Material);
	const EMaterialWriteResult Result =
	    Parameter.bActive ? EMaterialWriteResult::Active : EMaterialWriteResult::Inactive;
	const auto Existing = std::find_if(Snapshot->Overrides.begin(), Snapshot->Overrides.end(),
	                                   [&Parameter](const FMaterialParameterEntry& InEntry)
	                                   {
		                                   return InEntry.Name == Parameter.Name;
	                                   });
	if (Existing != Snapshot->Overrides.end() && Existing->Value == InValue)
	{
		return Result;
	}
	FMaterialSnapshot Next = *Snapshot;
	if (Existing != Snapshot->Overrides.end())
	{
		Next.Overrides[static_cast<std::size_t>(Existing - Snapshot->Overrides.begin())].Value = std::move(InValue);
	}
	else
	{
		Next.Overrides.push_back({Parameter.Name, std::move(InValue)});
		std::sort(Next.Overrides.begin(), Next.Overrides.end(),
		          [](const FMaterialParameterEntry& InA, const FMaterialParameterEntry& InB)
		          {
			          return InA.Name < InB.Name;
		          });
	}
	Publish(std::move(Next));
	return Result;
}

EMaterialWriteResult FMaterialInstance::SetSemantic(std::string_view InSemantic, FMaterialValue InValue)
{
	CheckOwner();
	return Set(Snapshot->Schema->FindSemantic(Snapshot->Definition->GetSemantics().Normalize(InSemantic)),
	           std::move(InValue));
}

void FMaterialInstance::Clear(std::string_view InName)
{
	CheckOwner();
	const FMaterialParameterDeclaration& Parameter = Snapshot->Schema->Get(Find(InName));
	if (Parameter.OverridePolicy == EMaterialOverridePolicy::Locked ||
	    (Parameter.OverrideScopes & MaterialScopeBit(EMaterialScope::Material)) == 0)
	{
		throw std::invalid_argument("Cannot clear a locked material parameter: " + Parameter.Name);
	}
	const auto Existing = std::find_if(Snapshot->Overrides.begin(), Snapshot->Overrides.end(),
	                                   [&Parameter](const FMaterialParameterEntry& InEntry)
	                                   {
		                                   return InEntry.Name == Parameter.Name;
	                                   });
	if (Existing != Snapshot->Overrides.end())
	{
		FMaterialSnapshot Next = *Snapshot;
		Next.Overrides.erase(Next.Overrides.begin() + (Existing - Snapshot->Overrides.begin()));
		Publish(std::move(Next));
	}
}

void FMaterialInstance::ReplaceDefinition(const FPreparedMaterialInterface& InInterface)
{
	CheckOwner();
	ValidateInterface(InInterface);
	FMaterialSnapshot Next = *Snapshot;
	Next.Definition = InInterface.Definition;
	Next.Schema = InInterface.Schema;
	for (FMaterialParameterEntry& Entry : Next.Overrides)
	{
		const FMaterialParameterDeclaration& Parameter = Next.Schema->Get(Next.Schema->Find(Entry.Name));
		if (Parameter.Name != Entry.Name)
		{
			throw std::invalid_argument("Definition replacement must preserve logical parameter identity: " +
			                            Entry.Name);
		}
		ValidateMaterialOverride(Parameter, Entry.Value, EMaterialScope::Material);
	}
	Publish(std::move(Next));
}

FMaterialParameterValues ResolveMaterialParameters(const FMaterialSnapshot& InSnapshot,
                                                   const FMaterialParameterValues& InProviders,
                                                   const FMaterialParameterValues& InObject,
                                                   const FMaterialParameterValues& InDraw,
                                                   std::optional<std::span<const std::size_t>> InActiveParameters)
{
	if (!InSnapshot.Definition || !InSnapshot.Schema)
	{
		throw std::invalid_argument("Material snapshot is incomplete");
	}
	std::map<std::string, const FMaterialValue*> Providers;
	for (const FMaterialParameterEntry& Entry : InProviders)
	{
		const std::string Name = InSnapshot.Definition->GetSemantics().Normalize(Entry.Name);
		if (!Providers.emplace(Name, &Entry.Value).second)
		{
			throw std::invalid_argument("Conflicting material providers: " + Name);
		}
	}
	const std::vector<FMaterialParameterDeclaration>& Parameters = InSnapshot.Schema->GetParameters();
	std::vector<bool> Active(Parameters.size(), !InActiveParameters.has_value());
	if (InActiveParameters)
	{
		for (const std::size_t Index : *InActiveParameters)
		{
			if (Index >= Active.size())
			{
				throw std::invalid_argument("Invalid material active parameter index");
			}
			Active[Index] = true;
		}
	}
	std::vector<std::optional<FMaterialValue>> Values(Parameters.size());
	for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
	{
		const FMaterialParameterDeclaration& Parameter = Parameters[Index];
		Values[Index] = Parameter.Default;
		if (Parameter.Source == EMaterialParameterSource::Semantic && Parameter.bActive && Active[Index])
		{
			const auto Provider = Providers.find(Parameter.Semantic);
			if (Provider != Providers.end())
			{
				Provider->second->Validate();
				if (Provider->second->Type != Parameter.Type)
				{
					throw std::invalid_argument("Provider type mismatch: " + Parameter.Semantic);
				}
				Values[Index] = *Provider->second;
			}
		}
	}
	ApplyOverrides(Values, *InSnapshot.Schema, InSnapshot.Overrides, EMaterialScope::Material);
	ApplyOverrides(Values, *InSnapshot.Schema, InObject, EMaterialScope::Object);
	ApplyOverrides(Values, *InSnapshot.Schema, InDraw, EMaterialScope::Draw);
	FMaterialParameterValues Result;
	for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
	{
		const FMaterialParameterDeclaration& Parameter = Parameters[Index];
		if (!Values[Index] && Parameter.bActive && Active[Index] && Parameter.bRequired)
		{
			throw std::invalid_argument("Missing material input: " + Parameter.Name +
			                            " semantic=" + Parameter.Semantic);
		}
		if (Values[Index])
		{
			Result.push_back({Parameter.Name, *Values[Index]});
		}
	}
	return Result;
}
} // namespace Hyperion
