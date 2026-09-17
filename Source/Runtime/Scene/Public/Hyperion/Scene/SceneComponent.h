#pragma once
#include "Hyperion/Reflection/Record.h"
#include <any>

namespace Hyperion
{
struct FSceneComponentDescriptor
{
	std::string Id;
	std::string Label;
	std::type_index CppType{typeid(void)};
	const FRecordDescriptor* Record{};
	bool bRequired{};
	bool bUnique = true;
	std::vector<std::string> Requires;
	std::function<std::any()> CreateSlot;
	std::function<const void*(const std::any&)> Get;
	std::function<void*(std::any&)> Edit;
	std::function<bool(const std::any&, const std::any&)> Equal;
};

template<class T> FSceneComponentDescriptor MakeSceneComponent(std::string InLabel, bool bInRequired = false)
{
	return {RecordType<T>().Id,
	        std::move(InLabel),
	        typeid(T),
	        &RecordType<T>(),
	        bInRequired,
	        true,
	        {},
	        []
	        {
		        return std::any(std::optional<T>{});
	        },
	        [](const std::any& InSlot) -> const void*
	        {
		        const auto& Value = std::any_cast<const std::optional<T>&>(InSlot);
		        return Value ? &*Value : nullptr;
	        },
	        [](std::any& InSlot) -> void*
	        {
		        auto& Value = std::any_cast<std::optional<T>&>(InSlot);
		        if (!Value)
		        {
			        Value.emplace();
		        }
		        return &*Value;
	        },
	        [](const std::any& InLeft, const std::any& InRight)
	        {
		        return std::any_cast<const std::optional<T>&>(InLeft) ==
		               std::any_cast<const std::optional<T>&>(InRight);
	        }};
}

class FSceneComponentRegistry
{
public:
	void Register(FSceneComponentDescriptor InDescriptor);
	std::shared_ptr<const FSceneComponentDescriptor> Find(std::string_view InId) const;
	std::shared_ptr<const FSceneComponentDescriptor> TryFind(std::string_view InId) const;
	std::shared_ptr<const FSceneComponentDescriptor> Find(std::type_index InType) const;
	std::vector<std::shared_ptr<const FSceneComponentDescriptor>> All() const;

private:
	mutable std::mutex Mutex;
	std::vector<std::shared_ptr<const FSceneComponentDescriptor>> Types;
};

FSceneComponentRegistry& SceneComponentRegistry();

struct FSceneComponent
{
	std::string Id;
	std::shared_ptr<const FSceneComponentDescriptor> Type;
	std::any State;

	const void* Get() const
	{
		return Type->Get(State);
	}

	void* Edit()
	{
		return Type->Edit(State);
	}
};

// Values copy with the scene transaction. Shared resolved resources inside a component stay shared.
class FSceneComponents
{
public:
	template<class T> const std::optional<T>& Slot() const
	{
		// This is a per-property hot path. Indexed access keeps vector bounds checks without
		// repeatedly adopting MSVC Debug iterators under the process-wide iterator lock.
		for (std::size_t Index = 0; Index < Values.size(); ++Index)
		{
			const auto& Component = Values[Index];
			if (Component.Type->CppType == typeid(T))
			{
				return std::any_cast<const std::optional<T>&>(Component.State);
			}
		}
		static const std::optional<T> Empty;
		return Empty;
	}

	template<class T> std::optional<T>& Slot()
	{
		for (std::size_t Index = 0; Index < Values.size(); ++Index)
		{
			auto& Component = Values[Index];
			if (Component.Type->CppType == typeid(T))
			{
				return std::any_cast<std::optional<T>&>(Component.State);
			}
		}
		const auto Type = SceneComponentRegistry().Find(typeid(T));
		Values.push_back({Type->Id, Type, Type->CreateSlot()});
		return std::any_cast<std::optional<T>&>(Values.back().State);
	}

	std::span<const FSceneComponent> All() const
	{
		return Values;
	}

	const FSceneComponent* Find(std::string_view InId) const;
	FSceneComponent* Find(std::string_view InId);
	void Add(std::string InId, std::string_view InType);
	void Remove(std::string_view InId);
	// Archive DTO extraction only; scene validation still enforces required components.
	void Discard(std::string_view InId);
	void Rename(std::string_view InId, std::string InNewId);
	void AddOpaque(std::string InId, std::shared_ptr<const FArchiveNode> InEnvelope);

	const auto& Unknown() const
	{
		return Opaque;
	}

	void Validate() const;
	bool operator==(const FSceneComponents& InOther) const;

private:
	std::vector<FSceneComponent> Values;
	std::map<std::string, std::shared_ptr<const FArchiveNode>> Opaque;
};
} // namespace Hyperion
