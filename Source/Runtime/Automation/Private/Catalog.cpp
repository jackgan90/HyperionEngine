#include "Hyperion/Automation/Catalog.h"
#include <cctype>
#include <set>
#include <sstream>

namespace Hyperion
{
namespace
{
std::string Fold(std::string InText)
{
	std::transform(InText.begin(), InText.end(), InText.begin(),
	               [](unsigned char InCharacter)
	               {
		               return static_cast<char>(std::tolower(InCharacter));
	               });
	return InText;
}

FArchiveNode Summary(const FOperationDescriptor& InOperation)
{
	const auto& Info = InOperation.Info;
	return FArchiveNode(FArchiveNode::FObject{{"id", WriteValue(Info.Id)},
	                                          {"summary", WriteValue(Info.Summary)},
	                                          {"version", WriteValue(Info.Version)},
	                                          {"owner", WriteValue(Info.Owner)},
	                                          {"available", WriteValue(Info.Unavailable.empty())},
	                                          {"unavailable", WriteValue(Info.Unavailable)},
	                                          {"readOnly", WriteValue(Info.bReadOnly)},
	                                          {"asynchronous", WriteValue(InOperation.bAsynchronous)}});
}
} // namespace

FOperationCatalog::FOperationCatalog() : Owner(std::this_thread::get_id())
{
}

void FOperationCatalog::RequireOwner() const
{
	if (Owner != std::this_thread::get_id())
	{
		throw std::logic_error("Automation requires its Main owner thread");
	}
}

void FOperationCatalog::Register(FOperationDescriptor InOperation)
{
	RequireOwner();
	const auto& Info = InOperation.Info;
	if (bSealed || Info.Id.empty() || Info.Summary.empty() || Info.Description.empty() || Info.Owner.empty() ||
	    Info.Effects.empty() || Info.Completion.empty() || !Info.Version || !InOperation.Request ||
	    !InOperation.Result || !InOperation.Invoke || Operations.contains(Info.Id))
	{
		throw std::invalid_argument("Incomplete, duplicate or late operation: " + Info.Id);
	}
	if (Info.Id.size() > 128 ||
	    Info.Id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") != std::string::npos)
	{
		throw std::invalid_argument("Invalid stable operation ID: " + Info.Id);
	}
	(void)ReadRecordWire(*InOperation.Request, Info.Example);
	RegisterType(*InOperation.Request);
	RegisterType(*InOperation.Result);
	const auto Id = Info.Id;
	Operations.emplace(Id, std::move(InOperation));
}

void FOperationCatalog::RegisterType(const FRecordDescriptor& InType)
{
	RequireOwner();
	if (bSealed)
	{
		throw std::logic_error("Catalog is sealed");
	}
	std::set<std::string> Visited;
	const auto RegisterRecord = [&](const auto& InSelf, const FRecordDescriptor& InRecord) -> void
	{
		Types.Register(InRecord);
		if (!Visited.insert(InRecord.Id).second)
		{
			return;
		}
		for (const auto& Member : InRecord.Members)
		{
			if (!Member.Options.bPersistent || !Member.Shape)
			{
				continue;
			}
			const auto* Shape = &Member.Shape();
			while (Shape->Element)
			{
				Shape = Shape->Element.get();
			}
			if (Shape->Record)
			{
				InSelf(InSelf, Shape->Record());
			}
		}
	};
	RegisterRecord(RegisterRecord, InType);
}

void FOperationCatalog::Seal()
{
	RequireOwner();
	bSealed = true;
}

void FOperationCatalog::UnregisterOwner(std::string_view InOwner)
{
	RequireOwner();
	std::erase_if(Operations,
	              [&](const auto& InEntry)
	              {
		              return InEntry.second.Info.Owner == InOwner;
	              });
}

const FOperationDescriptor& FOperationCatalog::Find(std::string_view InId) const
{
	RequireOwner();
	const auto It = Operations.find(InId);
	if (It == Operations.end())
	{
		throw FAutomationError("not_found", "Unknown operation: " + std::string(InId), "operation");
	}
	return It->second;
}

FArchiveNode FOperationCatalog::Search(std::string_view InQuery, std::size_t InOffset, std::size_t InLimit) const
{
	RequireOwner();
	if (!InLimit || InLimit > 50 || InQuery.size() > 256)
	{
		throw FAutomationError("invalid_arguments", "Search limit must be 1..50 and query at most 256 bytes");
	}
	FArchiveNode::FArray Items;
	std::size_t Matched{};
	for (const auto& [Id, Operation] : Operations)
	{
		std::string Haystack =
		    Id + " " + Operation.Info.Summary + " " + Operation.Info.Owner + " " + Operation.Info.Description;
		for (const auto& Keyword : Operation.Info.Keywords)
		{
			Haystack += " " + Keyword;
		}
		Haystack = Fold(std::move(Haystack));
		std::istringstream Words(Fold(std::string(InQuery)));
		std::string Word;
		bool bMatch = true;
		while (Words >> Word)
		{
			bMatch &= Haystack.find(Word) != std::string::npos;
		}
		if (!bMatch)
		{
			continue;
		}
		if (Matched++ >= InOffset && Items.size() < InLimit)
		{
			Items.push_back(Summary(Operation));
		}
	}
	const auto Next = InOffset + Items.size();
	return FArchiveNode(
	    FArchiveNode::FObject{{"items", FArchiveNode(std::move(Items))},
	                          {"nextOffset", Next < Matched ? WriteValue(Next) : FArchiveNode(std::monostate{})},
	                          {"total", WriteValue(Matched)}});
}

FArchiveNode FOperationCatalog::Describe(std::string_view InId) const
{
	const auto& Operation = Find(InId);
	auto Result = Summary(Operation);
	auto& Fields = std::get<FArchiveNode::FObject>(Result.Value);
	Fields.emplace("description", WriteValue(Operation.Info.Description));
	Fields.emplace("effects", WriteValue(Operation.Info.Effects));
	Fields.emplace("completion", WriteValue(Operation.Info.Completion));
	Fields.emplace("example", Operation.Info.Example);
	Fields.emplace("inputSchema", RecordWireSchema(*Operation.Request));
	Fields.emplace("outputSchema", RecordWireSchema(*Operation.Result));
	Fields.emplace("executionDomain", WriteValue(std::string("Main")));
	return Result;
}

FArchiveNode FOperationCatalog::DescribeType(std::string_view InId) const
{
	RequireOwner();
	std::shared_ptr<const FRecordDescriptor> Type;
	try
	{
		Type = Types.Find(InId);
	}
	catch (const std::runtime_error&)
	{
		throw FAutomationError("not_found", "Unknown reflected type: " + std::string(InId), "type");
	}
	return RecordWireSchema(*Type);
}

std::size_t FOperationCatalog::Size() const
{
	RequireOwner();
	return Operations.size();
}

bool FOperationCatalog::IsSealed() const
{
	RequireOwner();
	return bSealed;
}
} // namespace Hyperion
