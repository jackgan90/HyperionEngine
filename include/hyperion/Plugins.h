#pragma once
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Hyperion
{
class FPlugin
{
public:
	virtual ~FPlugin() = default;

	virtual void Start()
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
};

class FPluginSet
{
public:
	FPluginSet() = default;
	~FPluginSet();
	FPluginSet(FPluginSet&&) noexcept = default;
	FPluginSet& operator=(FPluginSet&&) = delete;
	FPluginSet(const FPluginSet&) = delete;

	std::span<const std::unique_ptr<FPlugin>> GetInstances() const
	{
		return Instances;
	}

	const std::vector<std::string>& GetOrder() const
	{
		return Order;
	}

	void Stop() noexcept;

private:
	std::vector<std::unique_ptr<FPlugin>> Instances;
	std::vector<std::string> Order;
	friend class FPluginRegistry;
};

class FPluginRegistry
{
public:
	void Add(FPluginDescriptor InDescriptor);
	FPluginSet Activate(std::span<const std::string> InRequested) const;

private:
	std::vector<FPluginDescriptor> Descriptors;
};
} // namespace Hyperion
