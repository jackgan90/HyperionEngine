#include "WireInternal.h"
#include <cmath>
#include <set>

namespace Hyperion::WirePrivate
{
namespace
{
using FSchemaStack = std::map<std::string, std::string>;
FArchiveNode ValueSchema(const FRecordValueShape& InShape, unsigned InDepth, const std::string& InPath,
                         FSchemaStack& InStack);

std::string PointerToken(std::string_view InValue)
{
	std::string Result;
	for (const char Character : InValue)
	{
		Result += Character == '~' ? "~0" : Character == '/' ? "~1" : std::string(1, Character);
	}
	return Result;
}

FArchiveNode RecordSchema(const FRecordDescriptor& InType, unsigned InDepth, const std::string& InPath,
                          FSchemaStack& InStack)
{
	if (const auto Existing = InStack.find(InType.Id); Existing != InStack.end())
	{
		return FArchiveNode(FArchiveNode::FObject{{"$ref", WriteValue(Existing->second)}});
	}
	CheckDepth(InDepth);
	ValidateRecordDescriptor(InType);
	InStack.emplace(InType.Id, InPath);
	FArchiveNode::FObject Properties;
	FArchiveNode::FArray Required;
	const auto Defaults = InType.Create();
	for (const auto& Member : InType.Members)
	{
		if (!Member.Options.bPersistent)
		{
			continue;
		}
		if (!Member.Shape)
		{
			throw std::invalid_argument("Missing wire shape: " + InType.Id + "." + Member.Id);
		}
		auto Property =
		    ValueSchema(Member.Shape(), InDepth + 1, InPath + "/properties/" + PointerToken(Member.Id), InStack);
		auto& Values = std::get<FArchiveNode::FObject>(Property.Value);
		const auto& Presentation = Member.Options.Inspector;
		const auto Description = !Member.Options.Description.empty() ? Member.Options.Description
		                         : Presentation                      ? Presentation->Tooltip
		                                                             : std::string{};
		if (!Description.empty())
		{
			const auto Existing = Values.find("description");
			const auto Suffix =
			    Existing == Values.end() ? std::string{} : " " + ReadValue<std::string>(Existing->second);
			Values.insert_or_assign("description", WriteValue(Description + Suffix));
		}
		if (Presentation)
		{
			Values.emplace("title", WriteValue(Presentation->Label));
			Values.emplace("x-hyperion-unit", WriteValue(Presentation->Unit));
			Values.emplace("x-hyperion-inspector-readonly", WriteValue(Presentation->bReadOnly));
		}
		if (Member.Options.bRequired)
		{
			Required.push_back(WriteValue(Member.Id));
		}
		else
		{
			try
			{
				auto Default = Encode(Member.Shape(), Member.Write(Defaults.get()), InDepth + 1);
				// Avoid expanding bulk/default records into the catalog.
				if (WriteJson(Default, {2048, 128, 16}).size() < 2048)
				{
					Values.emplace("default", std::move(Default));
				}
			}
			catch (const std::exception&)
			{
				// Some types have deliberately incomplete defaults; the validator remains authoritative.
			}
		}
		Properties.emplace(Member.Id, std::move(Property));
	}
	InStack.erase(InType.Id);
	return FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(std::string("object"))},
	                                          {"additionalProperties", WriteValue(false)},
	                                          {"properties", FArchiveNode(std::move(Properties))},
	                                          {"required", FArchiveNode(std::move(Required))},
	                                          {"x-hyperion-type", WriteValue(InType.Id)},
	                                          {"x-hyperion-version", WriteValue(InType.Version)}});
}

void AddEnumSchema(const FRecordValueShape& InShape, FArchiveNode::FObject& OutSchema)
{
	FArchiveNode::FArray Allowed;
	FArchiveNode::FArray Labels;
	std::set<std::string> Seen;
	std::string Description = "Allowed values: ";
	for (const auto& RawValue : InShape.EnumValues)
	{
		const auto Value = Encode(InShape, RawValue);
		const auto Key = WriteJson(Value);
		if (!Seen.insert(Key).second)
		{
			continue;
		}
		Allowed.push_back(Value);
		if (InShape.EnumLabels.empty())
		{
			continue;
		}
		std::string Names;
		std::string Details;
		for (const auto& Label : InShape.EnumLabels)
		{
			if (WriteJson(Encode(InShape, Label.Value)) == Key)
			{
				Names += (Names.empty() ? "" : " / ") + Label.Name;
				if (!Label.Description.empty())
				{
					Details += (Details.empty() ? "" : "; ") + Label.Description;
				}
			}
		}
		FArchiveNode::FObject Alternative{{"const", Value}};
		if (!Names.empty())
		{
			Alternative.emplace("title", WriteValue(Names));
			Alternative.emplace("description", WriteValue(Details));
		}
		Labels.emplace_back(std::move(Alternative));
		Description += Key + (Names.empty() ? "" : "=" + Names) + (Details.empty() ? "" : " (" + Details + ")") + "; ";
	}
	OutSchema.emplace("enum", FArchiveNode(std::move(Allowed)));
	if (!Labels.empty())
	{
		OutSchema.emplace("oneOf", FArchiveNode(std::move(Labels)));
		OutSchema.emplace("description", WriteValue(Description));
	}
}

FArchiveNode ScalarSchema(const FRecordValueShape& InShape)
{
	FArchiveNode::FObject Values;
	const bool bInteger =
	    InShape.Kind == ERecordValueKind::Integer || InShape.Kind == ERecordValueKind::UnsignedInteger;
	const bool bWide = bInteger && InShape.ElementBytes == 8;
	const std::string Type = InShape.Kind == ERecordValueKind::Boolean           ? "boolean"
	                         : InShape.Kind == ERecordValueKind::String || bWide ? "string"
	                         : bInteger                                          ? "integer"
	                                                                             : "number";
	Values.emplace("type", WriteValue(Type));
	if (bWide)
	{
		Values.emplace("pattern",
		               WriteValue(std::string(InShape.Kind == ERecordValueKind::Integer ? "^(0|-?[1-9][0-9]*)$"
		                                                                                : "^(0|[1-9][0-9]*)$")));
		Values.emplace("format",
		               WriteValue(std::string(InShape.Kind == ERecordValueKind::Integer ? "int64" : "uint64")));
	}
	else if (bInteger)
	{
		const bool bSigned = InShape.Kind == ERecordValueKind::Integer;
		const unsigned Bits = static_cast<unsigned>(InShape.ElementBytes * 8);
		Values.emplace("minimum", WriteValue(bSigned ? -std::ldexp(1.0, Bits - 1) : 0.0));
		Values.emplace("maximum", WriteValue(std::ldexp(1.0, bSigned ? Bits - 1 : Bits) - 1));
	}
	if (!InShape.EnumValues.empty())
	{
		AddEnumSchema(InShape, Values);
	}
	return FArchiveNode(std::move(Values));
}

FArchiveNode ValueSchema(const FRecordValueShape& InShape, unsigned InDepth, const std::string& InPath,
                         FSchemaStack& InStack)
{
	CheckDepth(InDepth);
	if (InShape.bOptional)
	{
		auto Required = InShape;
		Required.bOptional = false;
		return FArchiveNode(FArchiveNode::FObject{
		    {"anyOf", FArchiveNode(FArchiveNode::FArray{
		                  ValueSchema(Required, InDepth + 1, InPath + "/anyOf/0", InStack),
		                  FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(std::string("null"))}})})}});
	}
	if (InShape.Kind == ERecordValueKind::Record)
	{
		if (!InShape.Record)
		{
			throw std::invalid_argument("Missing record shape");
		}
		return RecordSchema(InShape.Record(), InDepth + 1, InPath, InStack);
	}
	if (InShape.Kind == ERecordValueKind::Sequence || InShape.Kind == ERecordValueKind::Map)
	{
		if (!InShape.Element)
		{
			throw std::invalid_argument("Missing element shape");
		}
		const bool bSequence = InShape.Kind == ERecordValueKind::Sequence;
		FArchiveNode::FObject Result{{"type", WriteValue(std::string(bSequence ? "array" : "object"))},
		                             {bSequence ? "items" : "additionalProperties",
		                              ValueSchema(*InShape.Element, InDepth + 1,
		                                          InPath + (bSequence ? "/items" : "/additionalProperties"), InStack)}};
		if (InShape.FixedSize)
		{
			Result.emplace("minItems", WriteValue(*InShape.FixedSize));
			Result.emplace("maxItems", WriteValue(*InShape.FixedSize));
		}
		return FArchiveNode(std::move(Result));
	}
	return ScalarSchema(InShape);
}
} // namespace

FArchiveNode Schema(const FRecordValueShape& InShape, unsigned InDepth)
{
	FSchemaStack Stack;
	return ValueSchema(InShape, InDepth, "#", Stack);
}
} // namespace Hyperion::WirePrivate

namespace Hyperion
{
FArchiveNode RecordWireSchema(const FRecordDescriptor& InType)
{
	WirePrivate::FSchemaStack Stack;
	auto Result = WirePrivate::RecordSchema(InType, 0, "#", Stack);
	std::get<FArchiveNode::FObject>(Result.Value)
	    .emplace("$schema", WriteValue(std::string("https://json-schema.org/draft/2020-12/schema")));
	return Result;
}
} // namespace Hyperion
