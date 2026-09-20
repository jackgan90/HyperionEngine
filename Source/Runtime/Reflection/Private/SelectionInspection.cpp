#include "Hyperion/Reflection/Record.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
template<class T> auto& ValueAt(T& InDraft, const FInspectionPath& InPath)
{
	auto* Value = &InDraft.GetValues().at(InPath.at(0));
	for (std::size_t Index = 1; Index < InPath.size(); ++Index)
	{
		if (auto* Object = std::get_if<FArchiveNode::FObject>(&Value->Value))
		{
			Value = &Object->at(InPath[Index]);
		}
		else
		{
			Value = &std::get<FArchiveNode::FArray>(Value->Value).at(std::stoull(InPath[Index]));
		}
	}
	return *Value;
}
} // namespace

bool EqualInspectionValue(const FArchiveNode& InLeft, const FArchiveNode& InRight)
{
	if (InLeft.Value.index() != InRight.Value.index())
	{
		return false;
	}
	return std::visit(
	    [&](const auto& InValue) -> bool
	    {
		    using FValue = std::decay_t<decltype(InValue)>;
		    const auto& Other = std::get<FValue>(InRight.Value);
		    if constexpr (std::is_same_v<FValue, FBulkData>)
		    {
			    return InValue.Element == Other.Element && std::ranges::equal(InValue.Data(), Other.Data());
		    }
		    else if constexpr (std::is_same_v<FValue, FArchiveNode::FArray>)
		    {
			    return std::ranges::equal(InValue, Other, EqualInspectionValue);
		    }
		    else if constexpr (std::is_same_v<FValue, FArchiveNode::FObject>)
		    {
			    return std::ranges::equal(InValue, Other,
			                              [](const auto& InA, const auto& InB)
			                              {
				                              return InA.first == InB.first &&
				                                     EqualInspectionValue(InA.second, InB.second);
			                              });
		    }
		    else
		    {
			    return InValue == Other;
		    }
	    },
	    InLeft.Value);
}

FRecordSelectionDraft::FRecordSelectionDraft(const FRecordDescriptor& InType, std::span<const void* const> InValues)
{
	if (InValues.empty())
	{
		throw std::invalid_argument("Inspection requires at least one target");
	}
	for (const auto* Value : InValues)
	{
		Drafts.emplace_back(InType, Value);
	}
}

const FRecordDescriptor& FRecordSelectionDraft::GetType() const
{
	return Drafts.front().GetType();
}

const FArchiveNode& FRecordSelectionDraft::GetValue(const FInspectionPath& InPath, std::size_t InTarget) const
{
	return ValueAt(Drafts.at(InTarget), InPath);
}

bool FRecordSelectionDraft::IsMixed(const FInspectionPath& InPath) const
{
	const auto& First = GetValue(InPath);
	return std::any_of(Drafts.begin() + 1, Drafts.end(),
	                   [&](const auto& InDraft)
	                   {
		                   return !EqualInspectionValue(First, ValueAt(InDraft, InPath));
	                   });
}

bool FRecordSelectionDraft::IsPresenceMixed(const FInspectionPath& InPath) const
{
	const bool bAbsent = std::holds_alternative<std::monostate>(GetValue(InPath).Value);
	return std::any_of(Drafts.begin() + 1, Drafts.end(),
	                   [&](const auto& InDraft)
	                   {
		                   return bAbsent != std::holds_alternative<std::monostate>(ValueAt(InDraft, InPath).Value);
	                   });
}

bool FRecordSelectionDraft::CanEditCollection(const FInspectionPath& InPath,
                                              const FPropertyPresentation& InPresentation) const
{
	const auto* First = std::get_if<FArchiveNode::FArray>(&GetValue(InPath).Value);
	if (!First)
	{
		return !IsMixed(InPath);
	}
	for (std::size_t Target = 1; Target < Drafts.size(); ++Target)
	{
		const auto* Other = std::get_if<FArchiveNode::FArray>(&GetValue(InPath, Target).Value);
		if (!Other || First->size() != Other->size())
		{
			return false;
		}
		for (std::size_t Index = 0; Index < First->size(); ++Index)
		{
			if (InPresentation.ElementIdentity.empty())
			{
				if (!EqualInspectionValue((*First)[Index], (*Other)[Index]))
				{
					return false;
				}
			}
			else
			{
				auto Path = InPath;
				Path.insert(Path.end(), {std::to_string(Index), "fields", InPresentation.ElementIdentity});
				if (!EqualInspectionValue(GetValue(Path), GetValue(Path, Target)))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void FRecordSelectionDraft::SetValue(const FInspectionPath& InPath, const FArchiveNode& InValue)
{
	for (auto& Draft : Drafts)
	{
		ValueAt(Draft, InPath) = InValue;
	}
}

void FRecordSelectionDraft::SetPresent(const FInspectionPath& InPath, const FRecordValueShape& InShape, bool bInPresent)
{
	for (auto& Draft : Drafts)
	{
		auto& Value = ValueAt(Draft, InPath);
		if (!bInPresent)
		{
			Value = FArchiveNode(std::monostate{});
		}
		else if (std::holds_alternative<std::monostate>(Value.Value))
		{
			Value = InShape.DefaultValue();
		}
	}
}

void FRecordSelectionDraft::ApplyToCandidate(std::size_t InTarget, void* InCandidate) const
{
	Drafts.at(InTarget).ApplyToCandidate(InCandidate);
}
} // namespace Hyperion
