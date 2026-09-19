#pragma once
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeindex>
#include <vector>

namespace Hyperion
{
struct FPluginUpdate
{
	std::uint64_t Frame{};
	double ElapsedSeconds{};
	float DeltaSeconds{};
};

// Host-local, Main-owned storage. Registrations never own service objects.
class FPluginServices
{
public:
	FPluginServices();
	~FPluginServices();
	FPluginServices(const FPluginServices&) = delete;
	FPluginServices& operator=(const FPluginServices&) = delete;
	void RequireOwner() const;
	bool Contains(std::type_index InType) const;

	template<class T> void AddExternal(T& InService)
	{
		Add(typeid(T), &InService, "");
	}

	template<class T> T* Find() const
	{
		return static_cast<T*>(Find(typeid(T)));
	}

	template<class T> T& Require() const
	{
		return *static_cast<T*>(Get(typeid(T)));
	}

	template<class T> void Publish(const T& InEvent)
	{
		Publish(typeid(T), &InEvent);
	}

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
	void Add(std::type_index InType, void* InService, const std::string& InOwner);
	void* Find(std::type_index InType) const;
	void* Get(std::type_index InType) const;
	void Remove(const std::string& InOwner) noexcept;
	void Unsubscribe(const std::string& InOwner) noexcept;
	void Subscribe(std::type_index InType, const std::string& InOwner, std::function<void(const void*)> InCallback);
	void Publish(std::type_index InType, const void* InEvent);
	friend class FPluginContext;
};

class FPluginContext
{
public:
	FPluginContext(FPluginServices& InServices, std::string InId, std::vector<std::type_index> InProvides,
	               std::vector<std::type_index> InRequires, std::vector<std::type_index> InOptional);
	~FPluginContext();
	FPluginContext(const FPluginContext&) = delete;
	FPluginContext& operator=(const FPluginContext&) = delete;

	template<class T> void Provide(T& InService)
	{
		CheckProvided(typeid(T));
		Services.Add(typeid(T), &InService, Id);
	}

	template<class T> T& Require() const
	{
		CheckRequested(typeid(T), true);
		return Services.Require<T>();
	}

	template<class T> T* Find() const
	{
		CheckRequested(typeid(T), false);
		return Services.Find<T>();
	}

	template<class T> void Subscribe(std::function<void(const T&)> InCallback)
	{
		CheckEvents();
		if (!InCallback)
		{
			throw std::invalid_argument("Empty plugin event subscription");
		}
		Services.Subscribe(typeid(T), Id,
		                   [Callback = std::move(InCallback)](const void* InEvent)
		                   {
			                   Callback(*static_cast<const T*>(InEvent));
		                   });
	}

	template<class T> void Publish(const T& InEvent)
	{
		CheckEvents();
		Services.Publish(InEvent);
	}

	// Cleanup runs in reverse order while the object and its dependencies still exist.
	void Defer(std::function<void()> InCleanup);
	void Disconnect() noexcept;
	void Drain() noexcept;
	void Close() noexcept;
	std::exception_ptr GetCleanupFailure() const;

private:
	void CheckEvents() const;
	void CheckProvided(std::type_index InType) const;
	void CheckRequested(std::type_index InType, bool bInRequired) const;
	FPluginServices& Services;
	std::string Id;
	std::vector<std::type_index> Provides;
	std::vector<std::type_index> Requires;
	std::vector<std::type_index> Optional;
	std::vector<std::function<void()>> Cleanup;
	bool bClosed{};
	bool bDisconnected{};
	std::exception_ptr CleanupFailure;
};

class FPlugin
{
public:
	virtual ~FPlugin() = default;

	virtual void Start()
	{
	}

	virtual void Start(FPluginContext&)
	{
		Start();
	}

	virtual void Update(const FPluginUpdate&)
	{
	}

	virtual void Quiesce() noexcept
	{
	}

	virtual void Stop() noexcept
	{
	}
};

struct FPluginDescriptor
{
	std::string Id;
	std::vector<std::string> Dependencies;
	std::function<std::unique_ptr<FPlugin>()> Create;
	std::vector<std::string> Before;
	std::vector<std::string> After;
	std::vector<std::string> Conflicts;
	std::vector<std::type_index> Provides;
	std::vector<std::type_index> Requires;
	std::vector<std::type_index> Optional;
	std::function<std::unique_ptr<FPlugin>(FPluginContext&)> CreateWithContext;
};

enum class EPluginFailurePolicy
{
	Strict,
	Continue
};

struct FPluginSelection
{
	std::vector<std::string> Requested;
	std::vector<std::string> Disabled;
	EPluginFailurePolicy FailurePolicy = EPluginFailurePolicy::Continue;
};

struct FPluginDiagnostic
{
	std::string Id;
	std::string Message;
	bool bStartupFailure{};
};

struct FPluginPlanEntry
{
	std::string Id;
	std::vector<std::string> RequiredPlugins;
	std::string Unavailable;
};

struct FPluginPlan
{
	std::vector<FPluginPlanEntry> Entries;
};

class FPluginSet
{
public:
	FPluginSet() = default;
	~FPluginSet();
	FPluginSet(FPluginSet&&) noexcept;
	FPluginSet& operator=(FPluginSet&&) = delete;
	FPluginSet(const FPluginSet&) = delete;
	std::span<const std::unique_ptr<FPlugin>> GetInstances() const;
	const std::vector<std::string>& GetOrder() const;
	const std::vector<FPluginDiagnostic>& GetDiagnostics() const;
	bool IsActive(const std::string& InId) const;
	void Update(const FPluginUpdate& InUpdate);
	void Quiesce() noexcept;
	void Stop() noexcept;
	std::exception_ptr GetStopFailure() const;

private:
	void RequireOwner() const;
	std::shared_ptr<FPluginServices> Services;
	std::vector<std::unique_ptr<FPlugin>> Instances;
	std::vector<std::unique_ptr<FPluginContext>> Contexts;
	std::vector<std::string> Order;
	std::vector<FPluginDiagnostic> Diagnostics;
	bool bQuiesced{};
	std::exception_ptr StopFailure;
	friend class FPluginRegistry;
};

class FPluginRegistry
{
public:
	void Add(FPluginDescriptor InDescriptor);
	FPluginPlan Plan(const FPluginSelection& InSelection, const FPluginServices& InServices) const;
	FPluginSet Activate(const FPluginSelection& InSelection, std::shared_ptr<FPluginServices> InServices) const;
	// Existing isolated users retain strict all-or-nothing activation.
	FPluginSet Activate(std::span<const std::string> InRequested) const;

private:
	const FPluginDescriptor* Find(const std::string& InId) const;
	std::vector<FPluginDescriptor> Descriptors;
};
} // namespace Hyperion
