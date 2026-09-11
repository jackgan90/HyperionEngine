#pragma once
#include "Hyperion/Reflection/ArchiveNode.h"
#include "Hyperion/Reflection/RecordCallback.h"
#include <functional>
#include <mutex>
#include <stdexcept>
#include <typeindex>

namespace Hyperion
{
struct FRecordDescriptor;
using FRecordVisitor = std::function<void(const FRecordDescriptor&, const void*, std::string_view)>;

struct FRecordReadContext
{
	std::string Path;
	std::vector<std::string>* Diagnostics{};

	FRecordReadContext Child(std::string_view InName) const;
	void Warn(std::string InMessage) const;
};

struct FRecordMemberOptions
{
	bool bRequired{};
	bool bPersistent = true;
	std::vector<std::string> Aliases;
};

struct FRecordMember
{
	std::string Id;
	TRecordCallback<FArchiveNode(const void*)> Write;
	TRecordCallback<void(void*, const FArchiveNode&, const FRecordReadContext&)> Read;
	TRecordCallback<void(const void*, const FRecordVisitor&, std::string_view)> Visit;
	FRecordMemberOptions Options;
};

struct FRecordDescriptor
{
	std::string Id;
	std::uint32_t Version = 1;
	std::vector<FRecordMember> Members;
	TRecordCallback<std::shared_ptr<void>()> Create;
	TRecordCallback<void(const void*)> Validate;
	std::type_index CppType{typeid(void)};
	TRecordCallback<void(void*, void*)> Commit;
	std::uint32_t MinimumVersion = 1;
	std::map<std::uint32_t, TRecordCallback<void(FArchiveNode::FObject&)>> Migrations;
	// Copies retain their definition identity; independently built descriptors cannot replace a registered contract.
	std::shared_ptr<const void> Definition = std::make_shared<const int>(0);
};

template<class T> const FRecordDescriptor& RecordType();

template<class T> std::span<const T> RecordEnumValues()
{
	return {};
}

void ValidateRecordDescriptor(const FRecordDescriptor& InType);
FArchiveNode WriteRecord(const FRecordDescriptor& InType, const void* InObject);
std::shared_ptr<void> ReadRecord(const FRecordDescriptor& InType, const FArchiveNode& InNode,
                                 const FRecordReadContext& InContext = {});
void ReadRecordFields(const FRecordDescriptor& InType, void* InObject, const FArchiveNode& InNode,
                      const FRecordReadContext& InContext = {});
void VisitRecord(const FRecordDescriptor& InType, const void* InObject, const FRecordVisitor& InVisitor,
                 std::string_view InPath = {});

class FRecordRegistry
{
public:
	void Register(const FRecordDescriptor& InType);
	std::shared_ptr<const FRecordDescriptor> Find(std::string_view InId) const;

	template<class T> void Register()
	{
		Register(RecordType<T>());
	}

private:
	mutable std::mutex Mutex;
	std::map<std::string, std::shared_ptr<const FRecordDescriptor>> Types;
};
} // namespace Hyperion

#include "Hyperion/Reflection/RecordValue.h"
