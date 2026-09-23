#include "WireInternal.h"
#include <cmath>

namespace Hyperion::WirePrivate
{
namespace
{
FArchiveNode RecordSchema(const FRecordDescriptor& InType, unsigned InDepth)
{
	CheckDepth(InDepth);
	ValidateRecordDescriptor(InType);
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
		auto Property = Schema(Member.Shape(), InDepth + 1);
		auto& Values = std::get<FArchiveNode::FObject>(Property.Value);
		const auto& Presentation = Member.Options.Inspector;
		const auto Description = !Member.Options.Description.empty() ? Member.Options.Description
		                         : Presentation                      ? Presentation->Tooltip
		                                                             : std::string{};
		if (!Description.empty())
		{
			Values.emplace("description", WriteValue(Description));
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
	return FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(std::string("object"))},
	                                          {"additionalProperties", WriteValue(false)},
	                                          {"properties", FArchiveNode(std::move(Properties))},
	                                          {"required", FArchiveNode(std::move(Required))},
	                                          {"x-hyperion-type", WriteValue(InType.Id)},
	                                          {"x-hyperion-version", WriteValue(InType.Version)}});
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
		FArchiveNode::FArray Allowed;
		for (const auto& Value : InShape.EnumValues)
		{
			Allowed.push_back(Encode(InShape, Value));
		}
		Values.emplace("enum", FArchiveNode(std::move(Allowed)));
	}
	return FArchiveNode(std::move(Values));
}
} // namespace

FArchiveNode Schema(const FRecordValueShape& InShape, unsigned InDepth)
{
	CheckDepth(InDepth);
	if (InShape.bOptional)
	{
		auto Required = InShape;
		Required.bOptional = false;
		return FArchiveNode(FArchiveNode::FObject{
		    {"anyOf", FArchiveNode(FArchiveNode::FArray{
		                  Schema(Required, InDepth + 1),
		                  FArchiveNode(FArchiveNode::FObject{{"type", WriteValue(std::string("null"))}})})}});
	}
	if (InShape.Kind == ERecordValueKind::Record)
	{
		if (!InShape.Record)
		{
			throw std::invalid_argument("Missing record shape");
		}
		return RecordSchema(InShape.Record(), InDepth + 1);
	}
	if (InShape.Kind == ERecordValueKind::Sequence || InShape.Kind == ERecordValueKind::Map)
	{
		if (!InShape.Element)
		{
			throw std::invalid_argument("Missing element shape");
		}
		const bool bSequence = InShape.Kind == ERecordValueKind::Sequence;
		FArchiveNode::FObject Result{
		    {"type", WriteValue(std::string(bSequence ? "array" : "object"))},
		    {bSequence ? "items" : "additionalProperties", Schema(*InShape.Element, InDepth + 1)}};
		if (InShape.FixedSize)
		{
			Result.emplace("minItems", WriteValue(*InShape.FixedSize));
			Result.emplace("maxItems", WriteValue(*InShape.FixedSize));
		}
		return FArchiveNode(std::move(Result));
	}
	return ScalarSchema(InShape);
}
} // namespace Hyperion::WirePrivate

namespace Hyperion
{
FArchiveNode RecordWireSchema(const FRecordDescriptor& InType)
{
	auto Result = WirePrivate::RecordSchema(InType, 0);
	std::get<FArchiveNode::FObject>(Result.Value)
	    .emplace("$schema", WriteValue(std::string("https://json-schema.org/draft/2020-12/schema")));
	return Result;
}
} // namespace Hyperion
