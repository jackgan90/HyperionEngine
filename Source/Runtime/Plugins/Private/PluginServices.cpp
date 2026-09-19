#include "Hyperion/Plugins/PluginRuntime.h"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace Hyperion
{
struct FPluginServices::FImpl
{
	struct FService
	{
		void* Object{};
		std::string Owner;
	};

	struct FSubscription
	{
		std::uint64_t Id{};
		std::type_index Type{typeid(void)};
		std::string Owner;
		std::shared_ptr<std::function<void(const void*)>> Callback;
	};

	std::thread::id Owner = std::this_thread::get_id();
	std::map<std::type_index, FService> Services;
	std::vector<FSubscription> Subscriptions;
	std::uint64_t NextSubscription{};
};

FPluginServices::FPluginServices() : Impl(std::make_unique<FImpl>())
{
}

FPluginServices::~FPluginServices() = default;

void FPluginServices::RequireOwner() const
{
	if (std::this_thread::get_id() != Impl->Owner)
	{
		throw std::logic_error("Plugin services and lifecycle require their owner thread");
	}
}

bool FPluginServices::Contains(std::type_index InType) const
{
	return Find(InType) != nullptr;
}

void* FPluginServices::Find(std::type_index InType) const
{
	RequireOwner();
	const auto Found = Impl->Services.find(InType);
	return Found == Impl->Services.end() ? nullptr : Found->second.Object;
}

void* FPluginServices::Get(std::type_index InType) const
{
	if (auto* Object = Find(InType))
	{
		return Object;
	}
	throw std::runtime_error(std::string("Unavailable plugin service: ") + InType.name());
}

void FPluginServices::Add(std::type_index InType, void* InService, const std::string& InOwner)
{
	RequireOwner();
	if (!InService || !Impl->Services.emplace(InType, FImpl::FService{InService, InOwner}).second)
	{
		throw std::invalid_argument(std::string("Duplicate or null plugin service: ") + InType.name());
	}
}

void FPluginServices::Remove(const std::string& InOwner) noexcept
{
	std::erase_if(Impl->Services,
	              [&](const auto& InEntry)
	              {
		              return InEntry.second.Owner == InOwner;
	              });
	Unsubscribe(InOwner);
}

void FPluginServices::Unsubscribe(const std::string& InOwner) noexcept
{
	std::erase_if(Impl->Subscriptions,
	              [&](const auto& InEntry)
	              {
		              return InEntry.Owner == InOwner;
	              });
}

void FPluginServices::Subscribe(std::type_index InType, const std::string& InOwner,
                                std::function<void(const void*)> InCallback)
{
	RequireOwner();
	if (!InCallback)
	{
		throw std::invalid_argument("Empty plugin event subscription");
	}
	Impl->Subscriptions.push_back({++Impl->NextSubscription, InType, InOwner,
	                               std::make_shared<std::function<void(const void*)>>(std::move(InCallback))});
}

void FPluginServices::Publish(std::type_index InType, const void* InEvent)
{
	RequireOwner();
	// IDs survive callback additions/removals; additions observe the next publication.
	std::vector<std::uint64_t> Pending;
	for (const auto& Subscription : Impl->Subscriptions)
	{
		if (Subscription.Type == InType)
		{
			Pending.push_back(Subscription.Id);
		}
	}
	for (const auto Id : Pending)
	{
		const auto Found = std::find_if(Impl->Subscriptions.begin(), Impl->Subscriptions.end(),
		                                [Id](const auto& InEntry)
		                                {
			                                return InEntry.Id == Id;
		                                });
		if (Found != Impl->Subscriptions.end())
		{
			// Keep the same callable alive across reentrant publication and subscription removal.
			auto Callback = Found->Callback;
			(*Callback)(InEvent);
		}
	}
}

FPluginContext::FPluginContext(FPluginServices& InServices, std::string InId, std::vector<std::type_index> InProvides,
                               std::vector<std::type_index> InRequires, std::vector<std::type_index> InOptional)
    : Services(InServices), Id(std::move(InId)), Provides(std::move(InProvides)), Requires(std::move(InRequires)),
      Optional(std::move(InOptional))
{
}

FPluginContext::~FPluginContext()
{
	Close();
}

void FPluginContext::CheckProvided(std::type_index InType) const
{
	Services.RequireOwner();
	if (bClosed || std::find(Provides.begin(), Provides.end(), InType) == Provides.end())
	{
		throw std::logic_error(Id + ": undeclared service publication");
	}
}

void FPluginContext::CheckRequested(std::type_index InType, bool bInRequired) const
{
	Services.RequireOwner();
	const bool bRequired = std::find(Requires.begin(), Requires.end(), InType) != Requires.end();
	const bool bOptional = std::find(Optional.begin(), Optional.end(), InType) != Optional.end();
	if (bClosed || (!bRequired && (bInRequired || !bOptional)))
	{
		throw std::logic_error(Id + ": undeclared service request");
	}
}

void FPluginContext::Defer(std::function<void()> InCleanup)
{
	Services.RequireOwner();
	if (bClosed || bDisconnected || !InCleanup)
	{
		throw std::logic_error("Cannot register cleanup on a closed plugin scope");
	}
	Cleanup.push_back(std::move(InCleanup));
}

void FPluginContext::Disconnect() noexcept
{
	bDisconnected = true;
	Services.Unsubscribe(Id);
}

void FPluginContext::CheckEvents() const
{
	Services.RequireOwner();
	if (bClosed || bDisconnected)
	{
		throw std::logic_error("Plugin events are disconnected");
	}
}

std::exception_ptr FPluginContext::GetCleanupFailure() const
{
	return CleanupFailure;
}

void FPluginContext::Drain() noexcept
{
	Disconnect();
	for (auto It = Cleanup.rbegin(); It != Cleanup.rend(); ++It)
	{
		try
		{
			(*It)();
		}
		catch (...)
		{
			if (!CleanupFailure)
			{
				CleanupFailure = std::current_exception();
			}
		}
	}
	Cleanup.clear();
}

void FPluginContext::Close() noexcept
{
	if (bClosed)
	{
		return;
	}
	Drain();
	Services.Remove(Id);
	bClosed = true;
}
} // namespace Hyperion
