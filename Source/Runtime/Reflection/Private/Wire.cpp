#include "WireInternal.h"
#include <charconv>
#include <cmath>
#include <set>

namespace Hyperion
{
FWireError::FWireError(std::string InPath, std::string InMessage)
    : std::invalid_argument(InPath + ": " + InMessage), Path(std::move(InPath))
{
}

namespace WirePrivate
{
void CheckDepth(unsigned InDepth)
{
	if (InDepth > 32)
	{
		throw std::invalid_argument("Wire type exceeds nesting limit");
	}
}

namespace
{
template<class T> FArchiveNode ParseInteger(const FArchiveNode& InValue)
{
	const auto& Text = std::get<std::string>(InValue.Value);
	T Value{};
	const auto Parsed = std::from_chars(Text.data(), Text.data() + Text.size(), Value);
	if (Parsed.ec != std::errc{} || Parsed.ptr != Text.data() + Text.size() || std::to_string(Value) != Text)
	{
		throw std::invalid_argument("Expected canonical decimal integer string");
	}
	return FArchiveNode(Value);
}

FArchiveNode Number(const FRecordValueShape& InShape, const FArchiveNode& InValue)
{
	if (InShape.Kind == ERecordValueKind::Integer || InShape.Kind == ERecordValueKind::UnsignedInteger)
	{
		auto Result = InShape.ElementBytes == 8
		                  ? (InShape.Kind == ERecordValueKind::Integer ? ParseInteger<std::int64_t>(InValue)
		                                                               : ParseInteger<std::uint64_t>(InValue))
		                  : InValue;
		if (const auto* Number = std::get_if<double>(&Result.Value))
		{
			// JSON Schema integer describes integral numbers, including JSON tokens such as 1.0.
			if (!std::isfinite(*Number) || std::trunc(*Number) != *Number || *Number < INT32_MIN ||
			    *Number > UINT32_MAX)
			{
				throw std::invalid_argument("Expected an integer in the finite type range");
			}
			Result = FArchiveNode(static_cast<std::int64_t>(*Number));
		}
		const auto Encoded = WriteJson(Result);
		if (!InShape.EnumValues.empty() && std::none_of(InShape.EnumValues.begin(), InShape.EnumValues.end(),
		                                                [&](const FArchiveNode& InAllowed)
		                                                {
			                                                return WriteJson(InAllowed) == Encoded;
		                                                }))
		{
			throw std::invalid_argument("Unknown enum value");
		}
		// Validate width before a custom field reader can narrow the value.
		(void)Pack(InShape, {Result});
		return Result;
	}
	double Value{};
	if (const auto* Signed = std::get_if<std::int64_t>(&InValue.Value))
	{
		Value = static_cast<double>(*Signed);
	}
	else if (const auto* Unsigned = std::get_if<std::uint64_t>(&InValue.Value))
	{
		Value = static_cast<double>(*Unsigned);
	}
	else
	{
		Value = std::get<double>(InValue.Value);
	}
	if (!std::isfinite(Value) || (InShape.ElementBytes == 4 && std::abs(Value) > std::numeric_limits<float>::max()))
	{
		throw std::invalid_argument("Number is outside the finite type range");
	}
	return FArchiveNode(Value);
}

FArchiveNode DecodeRecord(const FRecordDescriptor& InType, const FArchiveNode& InValue, const std::string& InPath,
                          unsigned InDepth)
{
	const auto& Values = std::get<FArchiveNode::FObject>(InValue.Value);
	FArchiveNode::FObject Fields;
	for (const auto& [Key, Value] : Values)
	{
		const auto It = std::find_if(InType.Members.begin(), InType.Members.end(),
		                             [&](const FRecordMember& InMember)
		                             {
			                             return InMember.Id == Key && InMember.Options.bPersistent;
		                             });
		if (It == InType.Members.end() || !It->Shape)
		{
			throw FWireError(InPath + "." + Key, "Unknown field");
		}
		Fields.emplace(Key, Decode(It->Shape(), Value, InPath + "." + Key, InDepth + 1));
	}
	for (const auto& Member : InType.Members)
	{
		if (Member.Options.bPersistent && Member.Options.bRequired && !Values.contains(Member.Id))
		{
			throw FWireError(InPath + "." + Member.Id, "Missing required field");
		}
	}
	return FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(InType.Id)},
	                                          {"version", WriteValue(InType.Version)},
	                                          {"fields", FArchiveNode(std::move(Fields))}});
}
} // namespace

FArchiveNode Decode(const FRecordValueShape& InShape, const FArchiveNode& InValue, const std::string& InPath,
                    unsigned InDepth)
{
	CheckDepth(InDepth);
	try
	{
		if (InShape.bOptional && std::holds_alternative<std::monostate>(InValue.Value))
		{
			return InValue;
		}
		switch (InShape.Kind)
		{
			case ERecordValueKind::Boolean:
				return FArchiveNode(std::get<bool>(InValue.Value));
			case ERecordValueKind::String:
				return FArchiveNode(std::get<std::string>(InValue.Value));
			case ERecordValueKind::Integer:
			case ERecordValueKind::UnsignedInteger:
			case ERecordValueKind::Number:
				return Number(InShape, InValue);
			case ERecordValueKind::Record:
				return DecodeRecord(InShape.Record(), InValue, InPath, InDepth);
			case ERecordValueKind::Sequence:
			{
				const auto& Values = std::get<FArchiveNode::FArray>(InValue.Value);
				if (InShape.FixedSize && Values.size() != *InShape.FixedSize)
				{
					throw FWireError(InPath, "Incorrect fixed-array length");
				}
				FArchiveNode::FArray Result;
				for (std::size_t Index = 0; Index < Values.size(); ++Index)
				{
					Result.push_back(Decode(*InShape.Element, Values[Index], InPath + "[" + std::to_string(Index) + "]",
					                        InDepth + 1));
				}
				return IsBulk(InShape) ? FArchiveNode(Pack(*InShape.Element, Result)) : FArchiveNode(std::move(Result));
			}
			case ERecordValueKind::Map:
			{
				FArchiveNode::FObject Result;
				for (const auto& [Key, Value] : std::get<FArchiveNode::FObject>(InValue.Value))
				{
					Result.emplace(Key, Decode(*InShape.Element, Value, InPath + "." + Key, InDepth + 1));
				}
				return FArchiveNode(std::move(Result));
			}
		}
	}
	catch (const FWireError&)
	{
		throw;
	}
	catch (const std::exception& Error)
	{
		throw FWireError(InPath, Error.what());
	}
	throw FWireError(InPath, "Unsupported shape");
}

FArchiveNode Encode(const FRecordValueShape& InShape, const FArchiveNode& InValue, unsigned InDepth)
{
	CheckDepth(InDepth);
	if (InShape.bOptional && std::holds_alternative<std::monostate>(InValue.Value))
	{
		return InValue;
	}
	if (InShape.Kind == ERecordValueKind::Record)
	{
		const auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InValue.Value).at("fields").Value);
		FArchiveNode::FObject Result;
		for (const auto& Member : InShape.Record().Members)
		{
			if (Member.Options.bPersistent)
			{
				if (!Member.Shape)
				{
					throw std::invalid_argument("Missing wire shape: " + Member.Id);
				}
				Result.emplace(Member.Id, Encode(Member.Shape(), Fields.at(Member.Id), InDepth + 1));
			}
		}
		return FArchiveNode(std::move(Result));
	}
	if (InShape.Kind == ERecordValueKind::Sequence)
	{
		const auto Values = IsBulk(InShape) ? Unpack(std::get<FBulkData>(InValue.Value))
		                                    : std::get<FArchiveNode::FArray>(InValue.Value);
		FArchiveNode::FArray Result;
		for (const auto& Value : Values)
		{
			Result.push_back(Encode(*InShape.Element, Value, InDepth + 1));
		}
		return FArchiveNode(std::move(Result));
	}
	if (InShape.Kind == ERecordValueKind::Map)
	{
		FArchiveNode::FObject Result;
		for (const auto& [Key, Value] : std::get<FArchiveNode::FObject>(InValue.Value))
		{
			Result.emplace(Key, Encode(*InShape.Element, Value, InDepth + 1));
		}
		return FArchiveNode(std::move(Result));
	}
	if (InShape.ElementBytes == 8 && InShape.Kind == ERecordValueKind::Integer)
	{
		return FArchiveNode(std::to_string(ReadInteger<std::int64_t>(InValue)));
	}
	if (InShape.ElementBytes == 8 && InShape.Kind == ERecordValueKind::UnsignedInteger)
	{
		return FArchiveNode(std::to_string(ReadInteger<std::uint64_t>(InValue)));
	}
	return InValue;
}
} // namespace WirePrivate

FArchiveNode WriteValueWire(const FRecordValueShape& InShape, const FArchiveNode& InArchive)
{
	return WirePrivate::Encode(InShape, InArchive);
}

FArchiveNode ReadValueWire(const FRecordValueShape& InShape, const FArchiveNode& InValue, std::string_view InPath)
{
	return WirePrivate::Decode(InShape, InValue, std::string(InPath));
}

FArchiveNode WriteRecordWire(const FRecordDescriptor& InType, const void* InObject)
{
	// The record callback cannot capture a descriptor; project fields directly.
	const auto Archive = WriteRecord(InType, InObject);
	const auto& Fields =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Archive.Value).at("fields").Value);
	FArchiveNode::FObject Result;
	for (const auto& Member : InType.Members)
	{
		if (Member.Options.bPersistent)
		{
			if (!Member.Shape)
			{
				throw std::invalid_argument("Missing wire shape: " + Member.Id);
			}
			Result.emplace(Member.Id, WriteValueWire(Member.Shape(), Fields.at(Member.Id)));
		}
	}
	return FArchiveNode(std::move(Result));
}

std::shared_ptr<void> ReadRecordWire(const FRecordDescriptor& InType, const FArchiveNode& InValue)
{
	ValidateRecordDescriptor(InType);
	try
	{
		const auto Archive = WirePrivate::DecodeRecord(InType, InValue, "arguments", 0);
		return ReadRecord(InType, Archive);
	}
	catch (const FWireError&)
	{
		throw;
	}
	catch (const std::exception& Error)
	{
		throw FWireError("arguments", Error.what());
	}
}
} // namespace Hyperion
