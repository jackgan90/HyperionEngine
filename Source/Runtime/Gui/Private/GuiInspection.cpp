#include "Hyperion/Gui/Gui.h"
#include <cstring>

namespace Hyperion
{
namespace
{
bool EditValue(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape,
               const FPropertyPresentation& InPresentation, const std::string& InId, unsigned InDepth);

std::string Label(const FPropertyPresentation& InPresentation, const std::string& InId)
{
	return InPresentation.Label + "##" + InId;
}

bool EditVectorValue(FGui& InGui, FArchiveNode& InValue, const FPropertyPresentation& InPresentation,
                     const std::string& InId, std::array<FVec4, 3>& OutBounds)
{
	auto& Fields = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InValue.Value).at("fields").Value);
	FVec3 Vector{ReadValue<float>(Fields.at("x")), ReadValue<float>(Fields.at("y")), ReadValue<float>(Fields.at("z"))};
	if (InGui.InputVectorRow(Label(InPresentation, InId).c_str(), Vector, InPresentation.Unit, InPresentation.Tooltip,
	                         OutBounds) &&
	    !InPresentation.bReadOnly)
	{
		Fields.at("x") = WriteValue(Vector.X);
		Fields.at("y") = WriteValue(Vector.Y);
		Fields.at("z") = WriteValue(Vector.Z);
		return true;
	}
	return false;
}

bool EditColorValue(FGui& InGui, FArchiveNode& InValue, const FPropertyPresentation& InPresentation,
                    const std::string& InId)
{
	auto Color = ReadValue<FVec3>(InValue);
	if (InGui.InputColor(Label(InPresentation, InId).c_str(), Color))
	{
		InValue = WriteValue(Color);
		return true;
	}
	return false;
}

bool EditScalar(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape,
                const FPropertyPresentation& InPresentation)
{
	const std::string Name = "##value";
	if (!InPresentation.Choices.empty())
	{
		auto Index = ReadInteger<std::size_t>(InValue);
		if (InGui.Combo(Name.c_str(), InPresentation.Choices, Index))
		{
			InValue = InShape.Kind == ERecordValueKind::Integer ? WriteValue(static_cast<std::int64_t>(Index))
			                                                    : WriteValue(static_cast<std::uint64_t>(Index));
			return true;
		}
		return false;
	}
	switch (InShape.Kind)
	{
		case ERecordValueKind::Boolean:
			return InGui.Checkbox(Name.c_str(), std::get<bool>(InValue.Value));
		case ERecordValueKind::String:
			return InGui.InputText(Name.c_str(), std::get<std::string>(InValue.Value), true,
			                       InPresentation.Widget == EPropertyWidget::Path);
		case ERecordValueKind::Integer:
			return InGui.InputInteger(Name.c_str(), std::get<std::int64_t>(InValue.Value));
		case ERecordValueKind::UnsignedInteger:
			return InGui.InputInteger(Name.c_str(), std::get<std::uint64_t>(InValue.Value));
		case ERecordValueKind::Number:
			return InGui.InputNumber(Name.c_str(), std::get<double>(InValue.Value));
		default:
			return false;
	}
}

template<class T>
bool EditBulk(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape, const std::string& InId,
              unsigned InDepth)
{
	auto Values = ReadBulk<std::vector<T>>(InValue);
	bool bChanged{};
	for (std::size_t Index = 0; Index < Values.size(); ++Index)
	{
		auto Value = WriteValue(Values[Index]);
		const auto Name = std::to_string(Index);
		if (EditValue(InGui, Value, *InShape.Element, FPropertyPresentation{Name}, InId + "/" + Name, InDepth + 1))
		{
			Values[Index] = ReadValue<T>(Value);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		InValue = WriteBulk(Values);
	}
	return bChanged;
}

bool EditBulkValue(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape, const std::string& InId,
                   unsigned InDepth)
{
	const auto& Element = std::get<FBulkData>(InValue.Value).Element;
	if (Element == "f32")
	{
		return EditBulk<float>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "f64")
	{
		return EditBulk<double>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "i32")
	{
		return EditBulk<std::int32_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "u32")
	{
		return EditBulk<std::uint32_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "i64")
	{
		return EditBulk<std::int64_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "u64")
	{
		return EditBulk<std::uint64_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "i16")
	{
		return EditBulk<std::int16_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "u16")
	{
		return EditBulk<std::uint16_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "i8")
	{
		return EditBulk<std::int8_t>(InGui, InValue, InShape, InId, InDepth);
	}
	if (Element == "u8")
	{
		return EditBulk<std::uint8_t>(InGui, InValue, InShape, InId, InDepth);
	}
	InGui.Text("Unsupported bulk element: " + Element);
	return false;
}

bool EditRecordValue(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape,
                     const FPropertyPresentation& InPresentation, const std::string& InId, unsigned InDepth)
{
	const auto& Type = InShape.Record();
	auto& Fields = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InValue.Value).at("fields").Value);
	if (Type.Id == "hyperion.mat4")
	{
		FMat4 Matrix{ReadValue<std::array<float, 16>>(Fields.at("values"))};
		if (InGui.InputMatrix(Label(InPresentation, InId).c_str(), Matrix))
		{
			Fields.at("values") = WriteValue(Matrix.Values);
			return true;
		}
		return false;
	}
	bool bChanged{};
	if (InGui.Section(Label(InPresentation, InId).c_str(), false))
	{
		for (const auto& Member : Type.Members)
		{
			const auto It = Fields.find(Member.Id);
			if (It != Fields.end() && Member.Shape)
			{
				const auto Presentation = Member.Options.Inspector.value_or(FPropertyPresentation{Member.Id});
				bChanged |=
				    EditValue(InGui, It->second, Member.Shape(), Presentation, InId + "/" + Member.Id, InDepth + 1);
			}
		}
	}
	return bChanged;
}

bool EditContainer(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape,
                   const FPropertyPresentation& InPresentation, const std::string& InId, unsigned InDepth)
{
	if (!InGui.Section(Label(InPresentation, InId).c_str(), false))
	{
		return false;
	}
	if (std::holds_alternative<FBulkData>(InValue.Value))
	{
		return EditBulkValue(InGui, InValue, InShape, InId, InDepth);
	}
	bool bChanged{};
	if (InShape.Kind == ERecordValueKind::Map)
	{
		for (auto& [Key, Value] : std::get<FArchiveNode::FObject>(InValue.Value))
		{
			bChanged |=
			    EditValue(InGui, Value, *InShape.Element, FPropertyPresentation{Key}, InId + "/" + Key, InDepth + 1);
		}
		return bChanged;
	}
	auto& Values = std::get<FArchiveNode::FArray>(InValue.Value);
	for (std::size_t Index = 0; Index < Values.size(); ++Index)
	{
		const auto Name = std::to_string(Index);
		bChanged |= EditValue(InGui, Values[Index], *InShape.Element, FPropertyPresentation{Name}, InId + "/" + Name,
		                      InDepth + 1);
	}
	if (InPresentation.bAllowResize && InGui.Button(("Add element##" + InId).c_str()))
	{
		Values.push_back(InShape.Element->DefaultValue());
		bChanged = true;
	}
	if (InPresentation.bAllowResize)
	{
		InGui.SameLine();
	}
	if (InPresentation.bAllowResize && InGui.Button(("Remove last##" + InId).c_str(), !Values.empty()))
	{
		Values.pop_back();
		bChanged = true;
	}
	return bChanged;
}

bool EditValue(FGui& InGui, FArchiveNode& InValue, const FRecordValueShape& InShape,
               const FPropertyPresentation& InPresentation, const std::string& InId, unsigned InDepth)
{
	if (InDepth > 32)
	{
		InGui.Text("Inspection nesting limit reached");
		return false;
	}
	InGui.BeginDisabled(InPresentation.bReadOnly);
	bool bChanged{};
	bool bPresent = !std::holds_alternative<std::monostate>(InValue.Value);
	if (InShape.bOptional)
	{
		InGui.BeginPropertyRow(("Override " + Label(InPresentation, InId)).c_str());
		if (InGui.Checkbox("##value", bPresent))
		{
			InValue = bPresent ? InShape.DefaultValue() : FArchiveNode(std::monostate{});
			bChanged = true;
		}
		InGui.EndPropertyRow();
	}
	if (bPresent)
	{
		if (InPresentation.Widget == EPropertyWidget::Vector3)
		{
			std::array<FVec4, 3> Bounds;
			bChanged |= EditVectorValue(InGui, InValue, InPresentation, InId, Bounds);
		}
		else if (InPresentation.Widget == EPropertyWidget::Color3)
		{
			bChanged |= EditColorValue(InGui, InValue, InPresentation, InId);
		}
		else if (InShape.Kind == ERecordValueKind::Record)
		{
			bChanged |= EditRecordValue(InGui, InValue, InShape, InPresentation, InId, InDepth);
		}
		else if (InShape.Kind == ERecordValueKind::Sequence || InShape.Kind == ERecordValueKind::Map)
		{
			bChanged |= EditContainer(InGui, InValue, InShape, InPresentation, InId, InDepth);
		}
		else
		{
			InGui.BeginPropertyRow(Label(InPresentation, InId).c_str());
			bChanged |= EditScalar(InGui, InValue, InShape, InPresentation);
			InGui.EndPropertyRow();
		}
	}
	InGui.EndDisabled();
	return bChanged && !InPresentation.bReadOnly;
}
} // namespace

bool FGui::EditRecord(FRecordDraft& InDraft, std::string_view InIdentity,
                      const std::function<void(std::string_view, FVec4)>& InObserve)
{
	bool bChanged{};
	for (const auto& Member : InDraft.GetType().Members)
	{
		if (Member.Options.Inspector)
		{
			const auto& Presentation = *Member.Options.Inspector;
			if (!Presentation.Group.empty())
			{
				Text(Presentation.Group);
			}
			auto& Value = InDraft.GetValues().at(Member.Id);
			const auto Id = std::string(InIdentity) + "/" + Member.Id;
			if (Presentation.Widget == EPropertyWidget::Vector3 && !Member.Shape().bOptional)
			{
				std::array<FVec4, 3> Bounds;
				BeginDisabled(Presentation.bReadOnly);
				bChanged |= EditVectorValue(*this, Value, Presentation, Id, Bounds);
				EndDisabled();
				if (InObserve)
				{
					for (unsigned Axis = 0; Axis < 3; ++Axis)
					{
						InObserve(Member.Id + "/" + char('x' + Axis), Bounds[Axis]);
					}
				}
			}
			else
			{
				bChanged |= EditValue(*this, Value, Member.Shape(), Presentation, Id, 0);
			}
			if (InObserve)
			{
				InObserve(Member.Id, LastItemBounds());
			}
		}
	}
	return bChanged;
}
} // namespace Hyperion
