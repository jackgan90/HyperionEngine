#include "Hyperion/Gui/Gui.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
using FObserver = std::function<void(std::string_view, FVec4)>;

std::string PathId(const FInspectionPath& InPath)
{
	std::string Result;
	for (const auto& Part : InPath)
	{
		if (Part != "fields")
		{
			Result += (Result.empty() ? "" : "/") + Part;
		}
	}
	return Result;
}

bool EditSelectionValue(FGui& InGui, FRecordSelectionDraft& InDraft, const FInspectionPath& InPath,
                        const FRecordValueShape& InShape, const FPropertyPresentation& InPresentation,
                        const std::string& InIdentity, const FObserver& InObserve, unsigned InDepth);

bool EditSelectionVector(FGui& InGui, FRecordSelectionDraft& InDraft, const FInspectionPath& InPath,
                         const FPropertyPresentation& InPresentation, const std::string& InLabel,
                         const FObserver& InObserve)
{
	auto Value = ReadValue<FVec3>(InDraft.GetValue(InPath));
	std::array<FInspectionPath, 3> Paths;
	std::array<bool, 3> Mixed;
	std::array<bool, 3> Edited;
	std::array<FVec4, 3> Bounds;
	for (unsigned Axis = 0; Axis < 3; ++Axis)
	{
		Paths[Axis] = InPath;
		Paths[Axis].insert(Paths[Axis].end(), {"fields", std::string(1, char('x' + Axis))});
		Mixed[Axis] = InDraft.IsMixed(Paths[Axis]);
	}
	const bool bChanged = InPresentation.Widget == EPropertyWidget::Color3
	                          ? InGui.InputColor(InLabel.c_str(), Value, {}, Mixed, &Edited)
	                          : InGui.InputVectorRow(InLabel.c_str(), Value, InPresentation.Unit,
	                                                 InPresentation.Tooltip, Bounds, Mixed, &Edited);
	const std::array Values{Value.X, Value.Y, Value.Z};
	for (unsigned Axis = 0; Axis < 3; ++Axis)
	{
		if (Edited[Axis])
		{
			InDraft.SetValue(Paths[Axis], WriteValue(Values[Axis]));
		}
		if (InObserve && InPresentation.Widget != EPropertyWidget::Color3)
		{
			InObserve(PathId(Paths[Axis]), Bounds[Axis]);
		}
	}
	return bChanged;
}

bool EditSelectionChildren(FGui& InGui, FRecordSelectionDraft& InDraft, const FInspectionPath& InPath,
                           const FRecordValueShape& InShape, const FPropertyPresentation& InPresentation,
                           const std::string& InIdentity, const FObserver& InObserve, unsigned InDepth)
{
	bool bChanged{};
	if (InShape.Kind == ERecordValueKind::Record)
	{
		for (const auto& Member : InShape.Record().Members)
		{
			if (!Member.Shape)
			{
				continue;
			}
			auto Path = InPath;
			Path.insert(Path.end(), {"fields", Member.Id});
			bChanged |= EditSelectionValue(InGui, InDraft, Path, Member.Shape(),
			                               Member.Options.Inspector.value_or(FPropertyPresentation{Member.Id}),
			                               InIdentity, InObserve, InDepth + 1);
		}
		return bChanged;
	}
	if (!InDraft.CanEditCollection(InPath, InPresentation))
	{
		InGui.TextWrapped("Multiple Values - collection elements do not correspond.");
		return false;
	}
	const auto& Value = InDraft.GetValue(InPath);
	std::vector<std::string> Keys;
	if (const auto* Items = std::get_if<FArchiveNode::FArray>(&Value.Value))
	{
		for (std::size_t Index = 0; Index < Items->size(); ++Index)
		{
			Keys.push_back(std::to_string(Index));
		}
	}
	else if (const auto* MapItems = std::get_if<FArchiveNode::FObject>(&Value.Value))
	{
		for (const auto& [Key, Item] : *MapItems)
		{
			Keys.push_back(Key);
		}
	}
	else
	{
		InGui.TextWrapped("Collection editing requires a single selection.");
	}
	for (const auto& Key : Keys)
	{
		auto Path = InPath;
		Path.push_back(Key);
		bChanged |= EditSelectionValue(InGui, InDraft, Path, *InShape.Element, FPropertyPresentation{Key}, InIdentity,
		                               InObserve, InDepth + 1);
	}
	return bChanged;
}

bool EditSelectionPresentValue(FGui& InGui, FRecordSelectionDraft& InDraft, const FInspectionPath& InPath,
                               const FRecordValueShape& InShape, const FPropertyPresentation& InPresentation,
                               const std::string& InIdentity, const FObserver& InObserve, unsigned InDepth)
{
	const auto Label = InPresentation.Label + "##" + InIdentity + "/" + PathId(InPath);
	if (InPresentation.Widget == EPropertyWidget::Vector3 || InPresentation.Widget == EPropertyWidget::Color3)
	{
		return EditSelectionVector(InGui, InDraft, InPath, InPresentation, Label, InObserve);
	}
	if (InShape.Kind == ERecordValueKind::Record || InShape.Kind == ERecordValueKind::Sequence ||
	    InShape.Kind == ERecordValueKind::Map)
	{
		const auto Title =
		    (InDraft.IsMixed(InPath) ? InPresentation.Label + " (Multiple Values)" : InPresentation.Label) + "###" +
		    InIdentity + "/" + PathId(InPath);
		return InGui.Section(Title.c_str(), false) &&
		       EditSelectionChildren(InGui, InDraft, InPath, InShape, InPresentation, InIdentity, InObserve, InDepth);
	}
	InGui.BeginPropertyRow(Label.c_str());
	auto Value = InDraft.GetValue(InPath);
	const bool bChanged = InGui.EditMixedScalar(Value, InShape, InPresentation, InDraft.IsMixed(InPath));
	if (InObserve)
	{
		InObserve(PathId(InPath), InGui.LastItemBounds());
	}
	InGui.EndPropertyRow();
	if (bChanged)
	{
		InDraft.SetValue(InPath, Value);
	}
	return bChanged;
}

bool EditSelectionValue(FGui& InGui, FRecordSelectionDraft& InDraft, const FInspectionPath& InPath,
                        const FRecordValueShape& InShape, const FPropertyPresentation& InPresentation,
                        const std::string& InIdentity, const FObserver& InObserve, unsigned InDepth)
{
	if (InDepth > 32)
	{
		InGui.Text("Inspection nesting limit reached");
		return false;
	}
	InGui.BeginDisabled(InPresentation.bReadOnly);
	bool bChanged{};
	if (InShape.bOptional)
	{
		InGui.BeginPropertyRow(("Override " + InPresentation.Label + "##" + InIdentity + "/" + PathId(InPath)).c_str());
		auto Presence = WriteValue(!std::holds_alternative<std::monostate>(InDraft.GetValue(InPath).Value));
		const FRecordValueShape Shape{.Kind = ERecordValueKind::Boolean};
		if (InGui.EditMixedScalar(Presence, Shape, {}, InDraft.IsPresenceMixed(InPath)))
		{
			InDraft.SetPresent(InPath, InShape, ReadValue<bool>(Presence));
			bChanged = true;
		}
		InGui.EndPropertyRow();
	}
	if (!InDraft.IsPresenceMixed(InPath) && !std::holds_alternative<std::monostate>(InDraft.GetValue(InPath).Value))
	{
		bChanged |=
		    EditSelectionPresentValue(InGui, InDraft, InPath, InShape, InPresentation, InIdentity, InObserve, InDepth);
	}
	InGui.EndDisabled();
	return bChanged && !InPresentation.bReadOnly;
}
} // namespace

bool FGui::EditRecord(FRecordSelectionDraft& InDraft, std::string_view InIdentity, const FObserver& InObserve,
                      std::span<const std::string> InReadOnlyFields)
{
	bool bChanged{};
	for (const auto& Member : InDraft.GetType().Members)
	{
		if (Member.Options.Inspector)
		{
			auto Presentation = *Member.Options.Inspector;
			const bool bBlocked = std::ranges::find(InReadOnlyFields, Member.Id) != InReadOnlyFields.end();
			if (bBlocked)
			{
				BeginPropertyRow((Presentation.Label + "##" + std::string(InIdentity) + "/" + Member.Id).c_str());
				TextWrapped("Multiple Values - collection elements do not correspond.");
				EndPropertyRow();
				continue;
			}
			bChanged |= EditSelectionValue(*this, InDraft, {Member.Id}, Member.Shape(), Presentation,
			                               std::string(InIdentity), InObserve, 0);
		}
	}
	return bChanged;
}
} // namespace Hyperion
