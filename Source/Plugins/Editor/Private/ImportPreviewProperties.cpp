#include "AssetImportPanel.h"
#include "AssetPropertyWidgets.h"
#include "Hyperion/Math/AffineTransform.h"
#include "Hyperion/Math/Angle.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
void ReadOnlyProperty(FGui& InGui, const char* InLabel, std::string InValue)
{
	InGui.BeginDisabled(true);
	AssetText(InGui, InLabel, InValue);
	InGui.EndDisabled();
}

template<class T> void ReplaceById(std::vector<T>& OutValues, T InValue)
{
	const auto Found = std::find_if(OutValues.begin(), OutValues.end(),
	                                [&](const auto& InEntry)
	                                {
		                                return InEntry.Id == InValue.Id;
	                                });
	if (Found == OutValues.end())
	{
		OutValues.push_back(std::move(InValue));
	}
	else
	{
		*Found = std::move(InValue);
	}
}

void DrawNodePage(FGui& InGui, const FImportDraftInfo& InInfo, std::uint32_t InOffset, std::size_t& OutSelected)
{
	std::vector<bool> Children(InInfo.Nodes.size());
	for (const auto& Node : InInfo.Nodes)
	{
		for (const auto Child : Node.Children)
		{
			if (Child >= InOffset && Child - InOffset < Children.size())
			{
				Children[Child - InOffset] = true;
			}
		}
	}
	const auto Draw = [&](auto&& InDraw, std::size_t InIndex) -> void
	{
		const auto& Node = InInfo.Nodes[InIndex];
		bool bClicked{};
		const bool bOpen =
		    InGui.TreeItem(Node.Id.c_str(), Node.Name.c_str(), Node.Children.empty(), OutSelected == InIndex, bClicked);
		if (bClicked)
		{
			OutSelected = InIndex;
		}
		if (bOpen)
		{
			for (const auto Child : Node.Children)
			{
				if (Child >= InOffset && Child - InOffset < InInfo.Nodes.size())
				{
					InDraw(InDraw, Child - InOffset);
				}
				else
				{
					InGui.Text("Child node " + std::to_string(Child) + " (another property page)");
				}
			}
			InGui.EndTree();
		}
	};
	for (std::size_t Index = 0; Index < Children.size(); ++Index)
	{
		if (!Children[Index])
		{
			Draw(Draw, Index);
		}
	}
}
} // namespace

void FAssetImportPanel::DrawDraftProperties(FGui& InGui, FImportDraftInfo& InInfo)
{
	if (InInfo.SourceWidth)
	{
		ReadOnlyProperty(InGui, "Source width", std::to_string(InInfo.SourceWidth));
		ReadOnlyProperty(InGui, "Source height", std::to_string(InInfo.SourceHeight));
	}
	if (InInfo.bNameEditable)
	{
		auto Name = InInfo.Name;
		if (AssetText(InGui, "Name", Name))
		{
			auto Edits = InInfo.Properties;
			Edits.Name = Name;
			ApplyDraftEdit(InInfo, std::move(Edits));
		}
		PreviewNameBounds = InGui.LastItemBounds();
	}
	if (InInfo.Width)
	{
		ReadOnlyProperty(InGui, "Texture type", InInfo.Dimension == ETextureDimension::Cube ? "Cube" : "Texture2D");
		ReadOnlyProperty(InGui, "Pixel bytes", std::to_string(InInfo.PixelBytes));
		ReadOnlyProperty(InGui, "Width", std::to_string(InInfo.Width));
		ReadOnlyProperty(InGui, "Height", std::to_string(InInfo.Height));
		ReadOnlyProperty(InGui, "Format", InInfo.Format);
		ReadOnlyProperty(InGui, "Color encoding", InInfo.Encoding);
		ReadOnlyProperty(InGui, "Mip levels", std::to_string(InInfo.Mips));
	}
	if (InInfo.TotalNodes || InInfo.TotalPrimitives)
	{
		ReadOnlyProperty(InGui, "Nodes", std::to_string(InInfo.TotalNodes));
		ReadOnlyProperty(InGui, "Mesh sections", std::to_string(InInfo.TotalPrimitives));
		ReadOnlyProperty(InGui, "Vertices", std::to_string(InInfo.Vertices));
		ReadOnlyProperty(InGui, "Triangles", std::to_string(InInfo.Triangles));
	}
	DrawDraftModel(InGui, InInfo);
	for (const auto& Diagnostic : InInfo.Diagnostics)
	{
		InGui.TextWrapped(Diagnostic);
	}
	const auto Total =
	    std::max({InInfo.TotalNodes, InInfo.TotalPrimitives, InInfo.TotalMaterialSlots, InInfo.TotalDiagnostics});
	if (Total > ImportDraftPreviewPageLimit)
	{
		if (InGui.Button("Previous properties", PreviewOffset > 0))
		{
			PreviewOffset -= ImportDraftPreviewPageLimit;
			SelectedNode = SelectedPrimitive = 0;
		}
		InGui.SameLine();
		if (InGui.Button("Next properties", PreviewOffset + ImportDraftPreviewPageLimit < Total))
		{
			PreviewOffset += ImportDraftPreviewPageLimit;
			SelectedNode = SelectedPrimitive = 0;
		}
		InGui.Text("Property page " + std::to_string(PreviewOffset / ImportDraftPreviewPageLimit + 1));
	}
}

void FAssetImportPanel::DrawDraftModel(FGui& InGui, FImportDraftInfo& InInfo)
{
	if (InInfo.Type == RecordType<FModelAsset>().Id)
	{
		const auto Vector = [](FVec3 InValue)
		{
			return std::to_string(InValue.X) + ", " + std::to_string(InValue.Y) + ", " + std::to_string(InValue.Z);
		};
		ReadOnlyProperty(InGui, "Bounds min", Vector(InInfo.BoundsMin));
		ReadOnlyProperty(InGui, "Bounds max", Vector(InInfo.BoundsMax));
	}
	if (!InInfo.Nodes.empty())
	{
		SelectedNode = std::min(SelectedNode, InInfo.Nodes.size() - 1);
		DrawNodePage(InGui, InInfo, PreviewOffset, SelectedNode);
		auto Node = InInfo.Nodes[SelectedNode];
		InGui.Text("Children: " + std::to_string(Node.Children.size()) +
		           " | Mesh sections: " + std::to_string(Node.Primitives.size()));
		InGui.BeginDisabled(InInfo.Type != RecordType<FModelAsset>().Id);
		auto Transform = DecomposeAffine(Node.Local);
		bool bChanged = AssetText(InGui, "Node name", Node.Name);
		bool bTransformChanged = InGui.InputVector("Position", Transform.Position);
		auto Degrees = RadiansToDegrees(Transform.Rotation);
		if (InGui.InputVector("Rotation (degrees)", Degrees))
		{
			Transform.Rotation = DegreesToRadians(Degrees);
			bTransformChanged = true;
		}
		bTransformChanged |= InGui.InputVector("Scale", Transform.Scale);
		InGui.EndDisabled();
		if (bChanged || bTransformChanged)
		{
			auto Edits = InInfo.Properties;
			ReplaceById(Edits.Nodes,
			            FImportNodeEdit{Node.Id, Node.Name, bTransformChanged ? ComposeAffine(Transform) : Node.Local});
			ApplyDraftEdit(InInfo, std::move(Edits));
		}
	}
	if (!InInfo.Primitives.empty())
	{
		std::vector<std::string> Names;
		for (const auto& Primitive : InInfo.Primitives)
		{
			Names.push_back(Primitive.Name.empty() ? "Mesh section " + std::to_string(Names.size() + 1)
			                                       : Primitive.Name);
		}
		SelectedPrimitive = std::min(SelectedPrimitive, Names.size() - 1);
		InGui.Combo("Mesh section", Names, SelectedPrimitive);
		auto Primitive = InInfo.Primitives[SelectedPrimitive];
		bool bChanged = AssetText(InGui, "Section name", Primitive.Name);
		std::int64_t Slot = Primitive.Material;
		bChanged |= InGui.InputInteger("Material slot", Slot);
		if (bChanged)
		{
			if (Slot < 0 || Slot > INT32_MAX)
			{
				throw std::invalid_argument("Material slot is outside the supported range");
			}
			auto Edits = InInfo.Properties;
			ReplaceById(Edits.Primitives,
			            FImportPrimitiveEdit{Primitive.Id, Primitive.Name, static_cast<std::int32_t>(Slot)});
			ApplyDraftEdit(InInfo, std::move(Edits));
		}
	}
	for (std::size_t Index = 0; Index < InInfo.MaterialSlots.size(); ++Index)
	{
		InGui.TextWrapped("Slot " + std::to_string(PreviewOffset + Index) + ": " + InInfo.MaterialSlots[Index].Path);
	}
}

} // namespace Hyperion
