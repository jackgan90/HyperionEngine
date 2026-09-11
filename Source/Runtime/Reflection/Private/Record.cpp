#include "Hyperion/Reflection/Record.h"
#include <set>

namespace Hyperion
{
FRecordReadContext FRecordReadContext::Child(std::string_view InName) const
{
	return {Path + (Path.empty() || InName.starts_with('[') ? "" : ".") + std::string(InName), Diagnostics};
}

void FRecordReadContext::Warn(std::string InMessage) const
{
	if (Diagnostics)
	{
		Diagnostics->push_back(Path + ": " + std::move(InMessage));
	}
}

void ValidateRecordDescriptor(const FRecordDescriptor& InType)
{
	if (InType.Id.empty() || !InType.Version || !InType.MinimumVersion || InType.MinimumVersion > InType.Version ||
	    !InType.Create || !InType.Commit || !InType.Definition || InType.CppType == typeid(void))
	{
		throw std::logic_error("Invalid reflected type descriptor: " + InType.Id);
	}
	std::set<std::string> Names;
	for (const auto& Field : InType.Members)
	{
		if (Field.Id.empty() || !Names.insert(Field.Id).second || !Field.Read || !Field.Write || !Field.Visit)
		{
			throw std::logic_error("Invalid or duplicate reflected member: " + InType.Id + "." + Field.Id);
		}
		for (const auto& Alias : Field.Options.Aliases)
		{
			if (Alias.empty() || !Names.insert(Alias).second)
			{
				throw std::logic_error("Duplicate reflected alias: " + InType.Id + "." + Alias);
			}
		}
	}
	for (const auto& [Version, Migration] : InType.Migrations)
	{
		if (Version < InType.MinimumVersion || Version >= InType.Version || !Migration)
		{
			throw std::logic_error("Invalid reflected migration: " + InType.Id);
		}
	}
}

FArchiveNode WriteRecord(const FRecordDescriptor& InType, const void* InObject)
{
	ValidateRecordDescriptor(InType);
	if (InType.Validate)
	{
		InType.Validate(InObject);
	}
	FArchiveNode::FObject Fields;
	for (const auto& Field : InType.Members)
	{
		if (Field.Options.bPersistent)
		{
			Fields.emplace(Field.Id, Field.Write(InObject));
		}
	}
	return FArchiveNode(FArchiveNode::FObject{{"type", FArchiveNode(InType.Id)},
	                                          {"version", WriteValue(InType.Version)},
	                                          {"fields", FArchiveNode(std::move(Fields))}});
}

namespace
{
void BindFields(const FRecordDescriptor& InType, void* InObject, const FArchiveNode::FObject& InFields,
                const FRecordReadContext& InContext)
{
	std::set<std::string> Known;
	for (const auto& Field : InType.Members)
	{
		Known.insert(Field.Id);
		Known.insert(Field.Options.Aliases.begin(), Field.Options.Aliases.end());
		if (!Field.Options.bPersistent)
		{
			continue;
		}
		auto It = InFields.find(Field.Id);
		for (const auto& Alias : Field.Options.Aliases)
		{
			const auto Legacy = InFields.find(Alias);
			if (Legacy != InFields.end())
			{
				if (It != InFields.end())
				{
					throw std::runtime_error(InContext.Child(Field.Id).Path + ": conflicting field alias " + Alias);
				}
				It = Legacy;
			}
		}
		if (It != InFields.end())
		{
			Field.Read(InObject, It->second, InContext.Child(Field.Id));
		}
		else if (Field.Options.bRequired)
		{
			throw std::runtime_error(InContext.Child(Field.Id).Path + ": missing required field");
		}
	}
	for (const auto& [Name, Value] : InFields)
	{
		if (!Known.contains(Name))
		{
			InContext.Child(Name).Warn("unknown field ignored");
		}
	}
}
} // namespace

std::shared_ptr<void> ReadRecord(const FRecordDescriptor& InType, const FArchiveNode& InNode,
                                 const FRecordReadContext& InContext)
{
	ValidateRecordDescriptor(InType);
	const FRecordReadContext Context{InContext.Path.empty() ? InType.Id : InContext.Path, InContext.Diagnostics};
	try
	{
		const auto& Object = std::get<FArchiveNode::FObject>(InNode.Value);
		auto Version = ReadValue<std::uint32_t>(Object.at("version"));
		if (ReadValue<std::string>(Object.at("type")) != InType.Id || Version < InType.MinimumVersion ||
		    Version > InType.Version)
		{
			throw std::runtime_error("type or schema version mismatch (file " + std::to_string(Version) + ", current " +
			                         std::to_string(InType.Version) + ")");
		}
		const auto& Fields = std::get<FArchiveNode::FObject>(Object.at("fields").Value);
		auto Result = InType.Create();
		if (Version == InType.Version)
		{
			BindFields(InType, Result.get(), Fields, Context);
		}
		else
		{
			Context.Warn("migrated schema " + std::to_string(Version) + " to " + std::to_string(InType.Version));
			auto Migrated = Fields;
			while (Version < InType.Version)
			{
				const auto Step = InType.Migrations.find(Version);
				if (Step == InType.Migrations.end())
				{
					throw std::runtime_error("missing schema migration from version " + std::to_string(Version));
				}
				Step->second(Migrated);
				++Version;
			}
			BindFields(InType, Result.get(), Migrated, Context);
		}
		if (InType.Validate)
		{
			InType.Validate(Result.get());
		}
		return Result;
	}
	catch (const std::exception& Error)
	{
		const std::string Message = Error.what();
		if (Message.starts_with(Context.Path))
		{
			throw;
		}
		throw std::runtime_error(Context.Path + " (" + InType.Id + "): " + Message);
	}
}

void ReadRecordFields(const FRecordDescriptor& InType, void* InObject, const FArchiveNode& InNode,
                      const FRecordReadContext& InContext)
{
	auto Temporary = ReadRecord(InType, InNode, InContext);
	InType.Commit(InObject, Temporary.get());
}

void VisitRecord(const FRecordDescriptor& InType, const void* InObject, const FRecordVisitor& InVisitor,
                 std::string_view InPath)
{
	InVisitor(InType, InObject, InPath);
	for (const auto& Field : InType.Members)
	{
		if (Field.Options.bPersistent)
		{
			const auto Path = FRecordReadContext{std::string(InPath)}.Child(Field.Id).Path;
			Field.Visit(InObject, InVisitor, Path);
		}
	}
}

void FRecordRegistry::Register(const FRecordDescriptor& InType)
{
	ValidateRecordDescriptor(InType);
	std::lock_guard Lock(Mutex);
	const auto It = Types.find(InType.Id);
	if (It != Types.end())
	{
		const auto& Existing = *It->second;
		const bool bMembersMatch =
		    Existing.Members.size() == InType.Members.size() &&
		    std::equal(Existing.Members.begin(), Existing.Members.end(), InType.Members.begin(),
		               [](const FRecordMember& InLeft, const FRecordMember& InRight)
		               {
			               return InLeft.Id == InRight.Id && InLeft.Options.bRequired == InRight.Options.bRequired &&
			                      InLeft.Options.bPersistent == InRight.Options.bPersistent &&
			                      InLeft.Options.Aliases == InRight.Options.Aliases && InLeft.Write == InRight.Write &&
			                      InLeft.Read == InRight.Read && InLeft.Visit == InRight.Visit;
		               });
		if (Existing.CppType != InType.CppType || Existing.Version != InType.Version ||
		    Existing.Definition != InType.Definition || Existing.Create != InType.Create ||
		    Existing.Validate != InType.Validate || Existing.Commit != InType.Commit ||
		    Existing.MinimumVersion != InType.MinimumVersion || Existing.Migrations != InType.Migrations ||
		    !bMembersMatch)
		{
			throw std::logic_error("Conflicting reflected type registration: " + InType.Id);
		}
		return;
	}
	for (const auto& [Id, Type] : Types)
	{
		if (Type->CppType == InType.CppType)
		{
			throw std::logic_error("C++ type already registered as " + Id);
		}
	}
	Types.emplace(InType.Id, std::make_shared<const FRecordDescriptor>(InType));
}

std::shared_ptr<const FRecordDescriptor> FRecordRegistry::Find(std::string_view InId) const
{
	std::lock_guard Lock(Mutex);
	const auto It = Types.find(std::string(InId));
	if (It == Types.end())
	{
		throw std::runtime_error("Unregistered asset type: " + std::string(InId));
	}
	return It->second;
}
} // namespace Hyperion
