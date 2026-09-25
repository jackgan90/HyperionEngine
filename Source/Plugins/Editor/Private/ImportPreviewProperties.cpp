#include "AssetImportPanel.h"
#include "AssetPropertyWidgets.h"
#include "Hyperion/Math/AffineTransform.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
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
	InGui.TextWrapped("Type: " + InInfo.Type);
	for (const auto& Detail : InInfo.Details)
	{
		InGui.TextWrapped(Detail);
	}
	if (InInfo.SourceWidth)
	{
		InGui.Text("Source panorama: " + std::to_string(InInfo.SourceWidth) + " x " +
		           std::to_string(InInfo.SourceHeight));
	}
	if (InInfo.Sky)
	{
		InGui.TextWrapped("Bake: radiance " + std::to_string(InInfo.Sky->RadianceSize) + " | specular " +
		                  std::to_string(InInfo.Sky->SpecularSize) + " | samples " +
		                  std::to_string(InInfo.Sky->Samples));
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
		InGui.TextWrapped(std::to_string(InInfo.Width) + " x " + std::to_string(InInfo.Height) + " | " + InInfo.Format +
		                  " | " + InInfo.Encoding + " | mips: " + std::to_string(InInfo.Mips));
		InGui.TextWrapped(
		    "Dimensions/format are read-only. Image encoding is changed in Import Asset, then Update preview.");
	}
	if (InInfo.TotalNodes || InInfo.TotalPrimitives)
	{
		InGui.TextWrapped(
		    "Nodes: " + std::to_string(InInfo.TotalNodes) + " | Primitives: " + std::to_string(InInfo.TotalPrimitives) +
		    " | Vertices: " + std::to_string(InInfo.Vertices) + " | Triangles: " + std::to_string(InInfo.Triangles));
	}
	DrawDraftModel(InGui, InInfo);
	DrawDraftMaterial(InGui, InInfo);
	for (const auto& Dependency : InInfo.Dependencies)
	{
		InGui.TextWrapped(Dependency.Field + ": " + Dependency.Reference.Path);
	}
	for (const auto& Product : InInfo.Products)
	{
		InGui.TextWrapped("Dependency: " + Product.Name + " | " + Product.Type +
		                  (Product.Width
		                       ? " | " + std::to_string(Product.Width) + " x " + std::to_string(Product.Height) +
		                             " | " + Product.Format + " | mips: " + std::to_string(Product.Mips)
		                       : ""));
	}
	for (const auto& Diagnostic : InInfo.Diagnostics)
	{
		InGui.TextWrapped(Diagnostic);
	}
	const auto Total = std::max({InInfo.TotalNodes, InInfo.TotalPrimitives, InInfo.TotalProducts, InInfo.TotalMaterial,
	                             InInfo.TotalDependencies, InInfo.TotalMaterialSlots, InInfo.TotalDiagnostics});
	if (Total > 32)
	{
		if (InGui.Button("Previous properties", PreviewOffset > 0))
		{
			PreviewOffset -= 32;
			SelectedNode = SelectedPrimitive = 0;
		}
		InGui.SameLine();
		if (InGui.Button("Next properties", PreviewOffset + 32 < Total))
		{
			PreviewOffset += 32;
			SelectedNode = SelectedPrimitive = 0;
		}
		InGui.Text("Property page " + std::to_string(PreviewOffset / 32 + 1));
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
		InGui.TextWrapped("Bounds: [" + Vector(InInfo.BoundsMin) + "] to [" + Vector(InInfo.BoundsMax) + "]");
	}
	if (!InInfo.Nodes.empty())
	{
		SelectedNode = std::min(SelectedNode, InInfo.Nodes.size() - 1);
		DrawNodePage(InGui, InInfo, PreviewOffset, SelectedNode);
		auto Node = InInfo.Nodes[SelectedNode];
		InGui.TextWrapped("Node ID: " + Node.Id);
		InGui.Text("Children: " + std::to_string(Node.Children.size()) +
		           " | Primitives: " + std::to_string(Node.Primitives.size()));
		InGui.BeginDisabled(InInfo.Type != RecordType<FModelAsset>().Id);
		auto Transform = DecomposeAffine(Node.Local);
		bool bChanged = AssetText(InGui, "Node name", Node.Name);
		bool bTransformChanged = InGui.InputVector("Position", Transform.Position);
		FVec3 Degrees{Transform.Rotation.X * 57.2957795f, Transform.Rotation.Y * 57.2957795f,
		              Transform.Rotation.Z * 57.2957795f};
		if (InGui.InputVector("Rotation (degrees)", Degrees))
		{
			Transform.Rotation = {Degrees.X / 57.2957795f, Degrees.Y / 57.2957795f, Degrees.Z / 57.2957795f};
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
			Names.push_back(Primitive.Name + " [" + Primitive.Id + "]");
		}
		SelectedPrimitive = std::min(SelectedPrimitive, Names.size() - 1);
		InGui.Combo("Primitive", Names, SelectedPrimitive);
		auto Primitive = InInfo.Primitives[SelectedPrimitive];
		bool bChanged = AssetText(InGui, "Primitive name", Primitive.Name);
		std::int64_t Slot = Primitive.Material;
		bChanged |= InGui.InputInteger("Existing material slot", Slot);
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

void FAssetImportPanel::DrawDraftMaterial(FGui& InGui, FImportDraftInfo& InInfo)
{
	// Copy the page: applying an edit replaces PreviewInfo and its member vectors.
	const auto Parameters = InInfo.Material;
	for (const auto& Parameter : Parameters)
	{
		InGui.TextWrapped(Parameter.Name);
		if (!Parameter.bEditable)
		{
			InGui.Text(Parameter.Values.empty() ? "Runtime-provided value (read-only)" : "Read-only parameter");
		}
		InGui.BeginDisabled(!Parameter.bEditable);
		auto Values = Parameter.Values;
		bool bChanged{};
		for (std::size_t Index = 0; Index < Values.size(); ++Index)
		{
			bChanged |=
			    InGui.InputNumber(("Value " + std::to_string(Index) + "##" + Parameter.Name).c_str(), Values[Index]);
		}
		InGui.EndDisabled();
		if (bChanged)
		{
			auto Edits = InInfo.Properties;
			std::erase_if(Edits.Material,
			              [&](const auto& InEdit)
			              {
				              return InEdit.Name == Parameter.Name;
			              });
			Edits.Material.push_back({Parameter.Name, std::move(Values)});
			ApplyDraftEdit(InInfo, std::move(Edits));
		}
	}
}
} // namespace Hyperion
