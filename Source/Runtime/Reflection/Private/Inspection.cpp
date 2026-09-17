#include "Hyperion/Reflection/Record.h"
#include <cmath>
#include <limits>
#include <utility>

namespace Hyperion
{
namespace
{
template<class T> bool IsIntegerWithinRange(T InValue, const FPropertyPresentation& InPresentation)
{
	// Check exact power-of-two bounds before converting limits: max() can round up as a double.
	constexpr double Lowest = static_cast<double>(std::numeric_limits<T>::lowest());
	const double UpperExclusive = std::ldexp(1.0, std::numeric_limits<T>::digits);
	if (InPresentation.Minimum)
	{
		const double Minimum = std::ceil(*InPresentation.Minimum);
		if (Minimum >= UpperExclusive || (Minimum > Lowest && InValue < static_cast<T>(Minimum)))
		{
			return false;
		}
	}
	if (InPresentation.Maximum)
	{
		const double Maximum = std::floor(*InPresentation.Maximum);
		if (Maximum < Lowest || (Maximum < UpperExclusive && InValue > static_cast<T>(Maximum)))
		{
			return false;
		}
	}
	return InPresentation.Choices.empty() ||
	       (std::cmp_greater_equal(InValue, 0) && std::cmp_less(InValue, InPresentation.Choices.size()));
}

void ValidateScalar(const FArchiveNode& InValue, const FPropertyPresentation& InPresentation, const std::string& InPath)
{
	bool bValid = true;
	if (const auto* Value = std::get_if<double>(&InValue.Value))
	{
		bValid = std::isfinite(*Value) && !(InPresentation.Minimum && *Value < *InPresentation.Minimum) &&
		         !(InPresentation.Maximum && *Value > *InPresentation.Maximum) &&
		         (InPresentation.Choices.empty() ||
		          (*Value >= 0 && *Value < static_cast<double>(InPresentation.Choices.size())));
	}
	if (const auto* Value = std::get_if<std::int64_t>(&InValue.Value))
	{
		bValid = IsIntegerWithinRange(*Value, InPresentation);
	}
	if (const auto* Value = std::get_if<std::uint64_t>(&InValue.Value))
	{
		bValid = IsIntegerWithinRange(*Value, InPresentation);
	}
	if (!bValid)
	{
		throw std::invalid_argument(InPath + ": value is outside the inspection range");
	}
}

FArchiveNode PrepareValue(FArchiveNode InValue, const FArchiveNode& InCurrent, const FRecordValueShape& InShape,
                          const FPropertyPresentation& InPresentation, const std::string& InPath)
{
	if (InPresentation.bReadOnly)
	{
		return InCurrent;
	}
	if (std::holds_alternative<std::monostate>(InValue.Value))
	{
		return InValue;
	}
	ValidateScalar(InValue, InPresentation, InPath);
	if (InShape.Kind == ERecordValueKind::Record)
	{
		auto& Fields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InValue.Value).at("fields").Value);
		const auto Current =
		    std::holds_alternative<std::monostate>(InCurrent.Value) ? InShape.DefaultValue() : InCurrent;
		const auto& CurrentFields =
		    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Current.Value).at("fields").Value);
		for (const auto& Member : InShape.Record().Members)
		{
			if (Member.Shape && Fields.contains(Member.Id) && CurrentFields.contains(Member.Id))
			{
				Fields.at(Member.Id) =
				    PrepareValue(std::move(Fields.at(Member.Id)), CurrentFields.at(Member.Id), Member.Shape(),
				                 Member.Options.Inspector.value_or(FPropertyPresentation{}), InPath + "." + Member.Id);
			}
		}
	}
	else if (auto* ArrayValues = std::get_if<FArchiveNode::FArray>(&InValue.Value); ArrayValues && InShape.Element)
	{
		const auto* Current = std::get_if<FArchiveNode::FArray>(&InCurrent.Value);
		if (!InPresentation.bAllowResize && (!Current || Current->size() != ArrayValues->size()))
		{
			throw std::invalid_argument(InPath + ": collection size is fixed");
		}
		for (std::size_t Index = 0; Index < ArrayValues->size(); ++Index)
		{
			const auto Original =
			    Current && Index < Current->size() ? (*Current)[Index] : InShape.Element->DefaultValue();
			(*ArrayValues)[Index] = PrepareValue(std::move((*ArrayValues)[Index]), Original, *InShape.Element, {},
			                                     InPath + "[" + std::to_string(Index) + "]");
		}
	}
	else if (InShape.Kind == ERecordValueKind::Map && InShape.Element)
	{
		auto& MapValues = std::get<FArchiveNode::FObject>(InValue.Value);
		const auto* Current = std::get_if<FArchiveNode::FObject>(&InCurrent.Value);
		for (auto& [Key, Value] : MapValues)
		{
			const auto Original =
			    Current && Current->contains(Key) ? Current->at(Key) : InShape.Element->DefaultValue();
			Value = PrepareValue(std::move(Value), Original, *InShape.Element, {}, InPath + "[" + Key + "]");
		}
	}
	return InValue;
}
} // namespace

FRecordDraft::FRecordDraft(const FRecordDescriptor& InType, const void* InValue) : SourceType(&InType), Type(&InType)
{
	ValidateRecordDescriptor(InType);
	if (InType.DisplayLayout)
	{
		Type = &InType.DisplayLayout->Record();
		if (Type == SourceType || Type->DisplayLayout)
		{
			throw std::logic_error("Nested or recursive reflected display layout: " + InType.Id);
		}
		ValidateRecordDescriptor(*Type);
		OriginalDisplay = InType.DisplayLayout->Project(InValue);
		if (!OriginalDisplay)
		{
			throw std::logic_error("Display projection returned no value: " + InType.Id);
		}
		InValue = OriginalDisplay.get();
	}
	for (const auto& Member : Type->Members)
	{
		if (Member.Options.Inspector)
		{
			Values.emplace(Member.Id, Member.Write(InValue));
		}
	}
}

void FRecordDraft::ApplyToCandidate(void* InCandidate) const
{
	const auto Display = SourceType->DisplayLayout ? SourceType->DisplayLayout->Project(InCandidate) : nullptr;
	void* Target = Display ? Display.get() : InCandidate;
	if (SourceType->DisplayLayout && !Display)
	{
		throw std::logic_error("Display projection returned no candidate");
	}
	for (const auto& Member : Type->Members)
	{
		if (Member.Options.Inspector && !Member.Options.Inspector->bReadOnly)
		{
			const auto Path = Type->Id + "." + Member.Id;
			const auto Value = PrepareValue(Values.at(Member.Id), Member.Write(Target), Member.Shape(),
			                                *Member.Options.Inspector, Path);
			Member.Read(Target, Value, {Path});
		}
	}
	if (Type->Validate)
	{
		Type->Validate(Target);
	}
	if (SourceType->DisplayLayout)
	{
		SourceType->DisplayLayout->Apply(InCandidate, Target, OriginalDisplay.get());
		if (SourceType->Validate)
		{
			SourceType->Validate(InCandidate);
		}
	}
}
} // namespace Hyperion
