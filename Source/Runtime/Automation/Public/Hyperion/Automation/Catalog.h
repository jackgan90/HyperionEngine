#pragma once
#include "Hyperion/Automation/Operation.h"
#include <thread>

namespace Hyperion
{
// Main-owned. Descriptors must outlive the catalog; no global locator or transport dependency.
class FOperationCatalog
{
public:
	FOperationCatalog();
	void Register(FOperationDescriptor InOperation);
	void RegisterType(const FRecordDescriptor& InType);
	// Plugin cleanup only: withdraw callable closures before their provider is destroyed.
	// Pure type metadata remains valid. Register remains forbidden after Seal.
	void UnregisterOwner(std::string_view InOwner);
	void Seal();
	void RequireOwner() const;
	const FOperationDescriptor& Find(std::string_view InId) const;
	FArchiveNode Search(std::string_view InQuery, std::size_t InOffset = 0, std::size_t InLimit = 12) const;
	FArchiveNode Describe(std::string_view InId) const;
	FArchiveNode DescribeType(std::string_view InId) const;
	std::size_t Size() const;
	bool IsSealed() const;

private:
	std::thread::id Owner;
	std::map<std::string, FOperationDescriptor, std::less<>> Operations;
	FRecordRegistry Types;
	bool bSealed{};
};
} // namespace Hyperion
