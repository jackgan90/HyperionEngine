#include <algorithm>
#include <hyperion/Plugins.h>
#include <stdexcept>
#include <unordered_map>

namespace Hyperion
{
FPluginSet::~FPluginSet()
{
	Stop();
}

void FPluginSet::Stop() noexcept
{
	for (auto I = Instances.rbegin(); I != Instances.rend(); ++I)
	{
		(*I)->Stop();
	}
	Instances.clear();
	Order.clear();
}

void FPluginRegistry::Add(FPluginDescriptor InDescriptor)
{
	if (InDescriptor.Id.empty() || !InDescriptor.Create)
	{
		throw std::invalid_argument("Plugin requires an ID and factory");
	}
	for (const auto& Existing : Descriptors)
	{
		if (Existing.Id == InDescriptor.Id)
		{
			throw std::invalid_argument("Duplicate plugin: " + InDescriptor.Id);
		}
	}
	Descriptors.push_back(std::move(InDescriptor));
}

FPluginSet FPluginRegistry::Activate(std::span<const std::string> InRequested) const
{
	std::unordered_map<std::string, int> Visited;
	std::vector<const FPluginDescriptor*> Sorted;
	std::function<void(const std::string&)> Visit = [&](const std::string& InId)
	{
		const int Mark = Visited[InId];
		if (Mark == 2)
		{
			return;
		}
		if (Mark == 1)
		{
			throw std::runtime_error("Plugin dependency cycle at: " + InId);
		}
		auto Found = std::find_if(Descriptors.begin(), Descriptors.end(),
		                          [&](const auto& InD)
		                          {
			                          return InD.Id == InId;
		                          });
		if (Found == Descriptors.end())
		{
			throw std::runtime_error("Missing plugin: " + InId);
		}
		Visited[InId] = 1;
		for (const auto& Dependency : Found->Dependencies)
		{
			Visit(Dependency);
		}
		Visited[InId] = 2;
		Sorted.push_back(&*Found);
	};
	for (const auto& Id : InRequested)
	{
		Visit(Id);
	}
	FPluginSet Result;
	Result.Instances.reserve(Sorted.size());
	Result.Order.reserve(Sorted.size());
	for (const auto* Descriptor : Sorted)
	{
		auto Instance = Descriptor->Create();
		if (!Instance)
		{
			throw std::runtime_error("Null plugin factory result: " + Descriptor->Id);
		}
		Result.Order.push_back(Descriptor->Id);
		try
		{
			Instance->Start();
		}
		catch (...)
		{
			Instance->Stop();
			throw;
		}
		Result.Instances.push_back(std::move(Instance));
	}
	return Result;
}
} // namespace Hyperion
