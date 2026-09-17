#include "Hyperion/Scene/SceneComponent.h"
#include <algorithm>
#include <set>
#include <utility>

namespace Hyperion
{
void FSceneComponentRegistry::Register(FSceneComponentDescriptor InDescriptor)
{
	if (InDescriptor.Id.empty() || !InDescriptor.Record || InDescriptor.Id != InDescriptor.Record->Id ||
	    InDescriptor.CppType != InDescriptor.Record->CppType || !InDescriptor.CreateSlot || !InDescriptor.Get ||
	    !InDescriptor.Edit || !InDescriptor.Equal)
	{
		throw std::invalid_argument("Invalid scene component descriptor");
	}
	ValidateRecordDescriptor(*InDescriptor.Record);
	std::lock_guard Lock(Mutex);
	for (const auto& Type : Types)
	{
		if (Type->Id == InDescriptor.Id || Type->CppType == InDescriptor.CppType)
		{
			throw std::invalid_argument("Duplicate scene component registration: " + InDescriptor.Id);
		}
	}
	Types.push_back(std::make_shared<const FSceneComponentDescriptor>(std::move(InDescriptor)));
}

std::shared_ptr<const FSceneComponentDescriptor> FSceneComponentRegistry::Find(std::string_view InId) const
{
	const auto Type = TryFind(InId);
	if (!Type)
	{
		throw std::invalid_argument("Unregistered scene component: " + std::string(InId));
	}
	return Type;
}

std::shared_ptr<const FSceneComponentDescriptor> FSceneComponentRegistry::TryFind(std::string_view InId) const
{
	std::lock_guard Lock(Mutex);
	for (const auto& Type : Types)
	{
		if (Type->Id == InId)
		{
			return Type;
		}
	}
	return {};
}

std::shared_ptr<const FSceneComponentDescriptor> FSceneComponentRegistry::Find(std::type_index InType) const
{
	std::lock_guard Lock(Mutex);
	for (const auto& Type : Types)
	{
		if (Type->CppType == InType)
		{
			return Type;
		}
	}
	throw std::invalid_argument("Unregistered scene component C++ type");
}

std::vector<std::shared_ptr<const FSceneComponentDescriptor>> FSceneComponentRegistry::All() const
{
	std::lock_guard Lock(Mutex);
	return Types;
}

const FSceneComponent* FSceneComponents::Find(std::string_view InId) const
{
	const auto It = std::find_if(Values.begin(), Values.end(),
	                             [&](const auto& InValue)
	                             {
		                             return InValue.Id == InId && InValue.Get();
	                             });
	return It == Values.end() ? nullptr : &*It;
}

FSceneComponent* FSceneComponents::Find(std::string_view InId)
{
	return const_cast<FSceneComponent*>(std::as_const(*this).Find(InId));
}

void FSceneComponents::Add(std::string InId, std::string_view InType)
{
	const auto Type = SceneComponentRegistry().Find(InType);
	const bool bReserved = std::any_of(Values.begin(), Values.end(),
	                                   [&](const auto& InValue)
	                                   {
		                                   return InValue.Id == InId && (InValue.Get() || InValue.Type != Type);
	                                   });
	if (InId.empty() || bReserved || Opaque.contains(InId))
	{
		throw std::invalid_argument("Empty or duplicate component ID");
	}
	for (auto& Component : Values)
	{
		if (Type->bUnique && Component.Type == Type)
		{
			if (Component.Get())
			{
				throw std::invalid_argument("Duplicate unique component");
			}
			Component.Id = std::move(InId);
			Component.Edit();
			return;
		}
	}
	FSceneComponent Value{std::move(InId), Type, Type->CreateSlot()};
	Value.Edit();
	Values.push_back(std::move(Value));
}

void FSceneComponents::Remove(std::string_view InId)
{
	const auto* Component = Find(InId);
	if (!Component || Component->Type->bRequired)
	{
		throw std::invalid_argument("Missing or required component cannot be removed");
	}
	std::erase_if(Values,
	              [&](const auto& InValue)
	              {
		              return InValue.Id == InId;
	              });
}

void FSceneComponents::Discard(std::string_view InId)
{
	std::erase_if(Values,
	              [&](const auto& InValue)
	              {
		              return InValue.Id == InId;
	              });
}

void FSceneComponents::Validate() const
{
	std::set<std::string> Ids;
	for (const auto& [Id, Envelope] : Opaque)
	{
		if (Id.empty() || !Envelope)
		{
			throw std::invalid_argument("Invalid opaque component");
		}
		Ids.insert(Id);
	}
	std::map<std::string, std::size_t> Counts;
	for (const auto& Component : Values)
	{
		if (!Component.Get())
		{
			continue;
		}
		if (Component.Id.empty() || !Ids.insert(Component.Id).second ||
		    (++Counts[Component.Type->Id] > 1 && Component.Type->bUnique))
		{
			throw std::invalid_argument("Invalid component identity or multiplicity");
		}
		Component.Type->Record->Validate(Component.Get());
	}
	for (const auto& Type : SceneComponentRegistry().All())
	{
		if (Type->bRequired && !Counts.contains(Type->Id))
		{
			throw std::invalid_argument("Missing required component: " + Type->Id);
		}
		if (Counts.contains(Type->Id))
		{
			for (const auto& Required : Type->Requires)
			{
				if (!Counts.contains(Required))
				{
					throw std::invalid_argument("Missing component dependency: " + Required);
				}
			}
		}
	}
}

bool FSceneComponents::operator==(const FSceneComponents& InOther) const
{
	if (Opaque != InOther.Opaque)
	{
		return false;
	}
	std::size_t Count{};
	for (const auto& Value : Values)
	{
		if (Value.Get())
		{
			++Count;
			const auto* Other = InOther.Find(Value.Id);
			if (!Other || Other->Type != Value.Type || !Value.Type->Equal(Value.State, Other->State))
			{
				return false;
			}
		}
	}
	const auto OtherCount = std::count_if(InOther.Values.begin(), InOther.Values.end(),
	                                      [](const auto& InValue)
	                                      {
		                                      return InValue.Get() != nullptr;
	                                      });
	return std::cmp_equal(Count, OtherCount);
}

void FSceneComponents::Rename(std::string_view InId, std::string InNewId)
{
	if (InNewId == InId)
	{
		return;
	}
	auto* Value = Find(InId);
	if (!Value || InNewId.empty() || Find(InNewId) || Opaque.contains(InNewId))
	{
		throw std::invalid_argument("Invalid component identity change");
	}
	Value->Id = std::move(InNewId);
}

void FSceneComponents::AddOpaque(std::string InId, std::shared_ptr<const FArchiveNode> InEnvelope)
{
	if (InId.empty() || !InEnvelope || Find(InId) || Opaque.contains(InId))
	{
		throw std::invalid_argument("Invalid opaque component identity");
	}
	Opaque.emplace(std::move(InId), std::move(InEnvelope));
}
} // namespace Hyperion
