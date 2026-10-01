#include "Hyperion/Materials/Material.h"
#include "MaterialIdentity.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>

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
		if (!Parameter.Semantic.IsEmpty() &&
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
		const FMaterialParameterHandle Handle = Entry.Resolve(InSchema);
		if (Seen[Handle.Index])
		{
			throw std::invalid_argument("Duplicate override of material parameter: " + InSchema.Get(Handle).Name);
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
		ValidateMaterialOverride(InSnapshot->Schema->Get(Entry.Resolve(*InSnapshot->Schema)), Entry.Value,
		                         EMaterialScope::Material);
	}
	auto Initial = *InSnapshot;
	Initial.Overrides.clear();
	std::set<std::size_t> Seen;
	for (const auto& Entry : InSnapshot->Overrides)
	{
		const auto Handle = Entry.Resolve(*InSnapshot->Schema);
		if (!Seen.insert(Handle.Index).second)
		{
			throw std::invalid_argument("Duplicate material snapshot override");
		}
		Initial.Overrides.push_back({Handle, Entry.Value});
	}
	std::sort(Initial.Overrides.begin(), Initial.Overrides.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Handle.Index < InB.Handle.Index;
	          });
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
	                                   [InHandle](const FMaterialParameterEntry& InEntry)
	                                   {
		                                   return InEntry.Handle == InHandle;
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
		Next.Overrides.push_back({InHandle, std::move(InValue)});
		std::sort(Next.Overrides.begin(), Next.Overrides.end(),
		          [](const FMaterialParameterEntry& InA, const FMaterialParameterEntry& InB)
		          {
			          return InA.Handle.Index < InB.Handle.Index;
		          });
	}
	Publish(std::move(Next));
	return Result;
}

EMaterialWriteResult FMaterialInstance::SetSemantic(FMaterialSemanticId InSemantic, FMaterialValue InValue)
{
	CheckOwner();
	return Set(Snapshot->Schema->FindSemantic(Snapshot->Definition->GetSemantics().Normalize(InSemantic)),
	           std::move(InValue));
}

void FMaterialInstance::Clear(std::string_view InName)
{
	Clear(Find(InName));
}

void FMaterialInstance::SetParameters(const FMaterialParameterValues& InValues)
{
	CheckOwner();
	FMaterialParameterValues Updates;
	std::set<std::size_t> Seen;
	for (const auto& Entry : InValues)
	{
		const auto Handles =
		    !Entry.Semantic.IsEmpty()
		        ? Snapshot->Schema->FindSemantics(Snapshot->Definition->GetSemantics().Normalize(Entry.Semantic))
		        : std::vector{Entry.Resolve(*Snapshot->Schema)};
		if (Handles.empty())
		{
			throw std::invalid_argument("Unknown typed material semantic");
		}
		for (const auto Handle : Handles)
		{
			if (!Seen.insert(Handle.Index).second)
			{
				throw std::invalid_argument("Duplicate material parameter update");
			}
			ValidateMaterialOverride(Snapshot->Schema->Get(Handle), Entry.Value, EMaterialScope::Material);
			Updates.push_back({Handle, Entry.Value});
		}
	}
	auto Next = *Snapshot;
	for (auto& Entry : Updates)
	{
		const auto Existing = std::find_if(Next.Overrides.begin(), Next.Overrides.end(),
		                                   [&](const auto& InEntry)
		                                   {
			                                   return InEntry.Handle == Entry.Handle;
		                                   });
		if (Existing == Next.Overrides.end())
		{
			Next.Overrides.push_back(std::move(Entry));
		}
		else
		{
			Existing->Value = std::move(Entry.Value);
		}
	}
	std::sort(Next.Overrides.begin(), Next.Overrides.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Handle.Index < InB.Handle.Index;
	          });
	if (Next.Overrides != Snapshot->Overrides)
	{
		Publish(std::move(Next));
	}
}

void FMaterialInstance::Clear(FMaterialParameterHandle InHandle)
{
	CheckOwner();
	const FMaterialParameterDeclaration& Parameter = Snapshot->Schema->Get(InHandle);
	if (Parameter.OverridePolicy == EMaterialOverridePolicy::Locked ||
	    (Parameter.OverrideScopes & MaterialScopeBit(EMaterialScope::Material)) == 0)
	{
		throw std::invalid_argument("Cannot clear a locked material parameter: " + Parameter.Name);
	}
	const auto Existing = std::find_if(Snapshot->Overrides.begin(), Snapshot->Overrides.end(),
	                                   [InHandle](const FMaterialParameterEntry& InEntry)
	                                   {
		                                   return InEntry.Handle == InHandle;
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
		const auto Name = Entry.GetName(*Snapshot->Schema);
		const auto Handle = Next.Schema->Find(Name);
		const FMaterialParameterDeclaration& Parameter = Next.Schema->Get(Handle);
		if (Parameter.Name != Name)
		{
			throw std::invalid_argument("Definition replacement must preserve logical parameter identity: " +
			                            std::string(Name));
		}
		ValidateMaterialOverride(Parameter, Entry.Value, EMaterialScope::Material);
		Entry = {Handle, std::move(Entry.Value)};
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
	std::map<FMaterialSemanticId, const FMaterialValue*> Providers;
	for (const FMaterialParameterEntry& Entry : InProviders)
	{
		const auto Name = InSnapshot.Definition->GetSemantics().Normalize(Entry.GetSemantic());
		if (!Providers.emplace(Name, &Entry.Value).second)
		{
			throw std::invalid_argument("Conflicting material providers: " + std::string(Name.GetName()));
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
					throw std::invalid_argument("Provider type mismatch: " + std::string(Parameter.Semantic.GetName()));
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
			                            " semantic=" + std::string(Parameter.Semantic.GetName()));
		}
		if (Values[Index])
		{
			Result.push_back({InSnapshot.Schema->GetHandle(Index), *Values[Index]});
		}
	}
	return Result;
}

FMaterialParameterValues RebindMaterialParameters(const FMaterialParameterValues& InValues,
                                                  const FMaterialParameterSchema& InPrevious,
                                                  const FMaterialParameterSchema& InNext)
{
	FMaterialParameterValues Result;
	for (const auto& Entry : InValues)
	{
		const auto Previous = Entry.Resolve(InPrevious);
		const auto Handle = InNext.FindAuthorIdentity(InPrevious.GetParameterIdentity(Previous.Index).AuthorIdentity);
		if (InNext.Get(Handle).Type != Entry.Value.Type)
		{
			throw std::invalid_argument("Material schema rebinding changed parameter identity or type");
		}
		Result.emplace_back(Handle, Entry.Value);
	}
	return Result;
}

void RebindMaterialSnapshot(FMaterialSnapshot& InSnapshot, std::shared_ptr<const FMaterialParameterSchema> InSchema)
{
	if (!InSnapshot.Schema || !InSchema)
	{
		throw std::invalid_argument("Material snapshot rebinding requires schemas");
	}
	if (InSnapshot.Schema == InSchema)
	{
		return;
	}
	auto Overrides = RebindMaterialParameters(InSnapshot.Overrides, *InSnapshot.Schema, *InSchema);
	InSnapshot.Overrides = std::move(Overrides);
	InSnapshot.Schema = std::move(InSchema);
}
} // namespace Hyperion
