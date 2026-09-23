#include "Hyperion/AssetEditing/AssetProperties.h"
#include "AssetPropertyWidgets.h"
#include "AssetWorkspace.h"
#include "Hyperion/AssetEditing/ModelProperties.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Math/AffineTransform.h"
#include <algorithm>
#include <sstream>

namespace Hyperion
{
namespace
{
FArchiveNode::FObject& Fields(FArchiveNode& InNode)
{
	return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InNode.Value).at("fields").Value);
}

std::string VectorText(FVec3 InValue)
{
	std::ostringstream Text;
	Text << InValue.X << ", " << InValue.Y << ", " << InValue.Z;
	return Text.str();
}

void ShowBounds(FGui& InGui, const FBounds& InBounds)
{
	if (InBounds.bValid)
	{
		AssetInfo(InGui, "Bounds min", VectorText(InBounds.Minimum));
		AssetInfo(InGui, "Bounds max", VectorText(InBounds.Maximum));
	}
}

void ModelTree(FGui& InGui, const std::vector<FModelNode>& InNodes, std::uint32_t InIndex, std::size_t& OutSelected)
{
	const auto& Node = InNodes.at(InIndex);
	bool bClicked{};
	const bool bOpen = InGui.TreeItem(("model-node/" + Node.Id).c_str(), Node.Name.c_str(), Node.Children.empty(),
	                                  OutSelected == InIndex, bClicked);
	if (bClicked)
	{
		OutSelected = InIndex;
	}
	if (bOpen)
	{
		for (const auto Child : Node.Children)
		{
			ModelTree(InGui, InNodes, Child, OutSelected);
		}
		InGui.EndTree();
	}
}
} // namespace

void FAssetWorkspace::EditField(FGui& InGui, FEntry& InEntry, const char* InField,
                                const std::function<bool()>& InWidget, const std::function<FArchiveNode()>& InValue,
                                bool bInAffectsPreview)
{
	FAssetLiveEditScope Editing(InGui);
	const bool bChanged = InWidget();
	const auto Interaction = Editing.Finish();
	if (Interaction.ActiveInteraction)
	{
		InEntry.GuiInteraction = Interaction.ActiveInteraction;
	}
	if (bChanged)
	{
		if (std::string_view(InField) == "primitives")
		{
			CommitModelPrimitiveFields(*InEntry.Document, InValue(), Interaction.ChangedInteraction);
		}
		else
		{
			CommitAssetField(*InEntry.Document, InField, InValue(), Interaction.ChangedInteraction, bInAffectsPreview);
		}
	}
}

void FAssetWorkspace::RevealProperty(std::string InId)
{
	RevealControl = std::move(InId);
}

void FAssetWorkspace::ObserveProperty(FGui& InGui, const std::string& InId)
{
	Bounds[InId] = InGui.LastItemBounds();
	if (RevealControl == InId)
	{
		InGui.RevealLastItem();
		RevealControl.clear();
	}
}

void FAssetWorkspace::DrawProperties(FGui& InGui)
{
	if (!Active || !Active->Document)
	{
		InGui.Text("Loading asset details...");
		return;
	}
	auto& Entry = *Active;
	auto& Document = *Entry.Document;
	const auto& Header = Document.Loaded().Header;
	const auto PreviousInteraction = std::exchange(Entry.GuiInteraction, 0);
	InGui.BeginDisabled(Entry.bReadOnly || Entry.HasPendingEdit());
	try
	{
		InGui.Text("Asset properties");
		InGui.TextWrapped("Editable values use input controls. Information rows are read-only.");
		auto Name = ReadValue<std::string>(Document.Get("name"));
		EditField(
		    InGui, Entry, "name",
		    [&]
		    {
			    return AssetText(InGui, "Name", Name, false);
		    },
		    [&]
		    {
			    return WriteValue(Name);
		    });
		Bounds["field/name"] = InGui.LastItemBounds();
		if (Header.TypeId == RecordType<FTextureAsset>().Id)
		{
			DrawTextureProperties(InGui, Entry);
		}
		else if (Header.TypeId == RecordType<FModelAsset>().Id)
		{
			DrawModelProperties(InGui, Entry);
		}
		else if (Header.TypeId == RecordType<FSkyAsset>().Id)
		{
			DrawSkyProperties(InGui, Entry);
		}
		else
		{
			DrawMaterialProperties(InGui, Entry);
		}
		if (InGui.Section("Asset information", false))
		{
			AssetInfo(InGui, "Type", Header.TypeId);
			AssetInfo(InGui, "ID", Header.Id);
			AssetInfo(InGui, "Path", PathToUtf8(Entry.Path));
			AssetInfo(InGui, "Schema", std::to_string(Header.SchemaVersion));
			AssetInfo(InGui, "Saved revision", Header.Revision);
			AssetInfo(InGui, "Memory bytes", std::to_string(Document.Loaded().RetainedBytes));
			AssetInfo(InGui, "Preview", Entry.Pending ? "Preparing" : Entry.Preview ? "Ready" : "Unavailable");
			if (Header.Import)
			{
				AssetInfo(InGui, "Importer", Header.Import->Importer);
			}
			for (const auto& Diagnostic : Document.Loaded().Diagnostics)
			{
				InGui.TextWrapped(Diagnostic);
			}
		}
		if (InGui.Section("Dependencies", false))
		{
			const auto& Dependencies = Entry.Preview ? Entry.Preview->Header.Dependencies : Header.Dependencies;
			for (const auto& Dependency : Dependencies)
			{
				AssetInfo(InGui, Dependency.Field.c_str(), Dependency.Reference.Path);
			}
		}
	}
	catch (const std::exception& Failure)
	{
		Document.Error = Failure.what();
	}
	InGui.EndDisabled();
	if (Entry.bReadOnly)
	{
		InGui.TextWrapped("This content mount is read-only.");
	}
	if (Entry.EncodingEdit)
	{
		InGui.Text("Rebuilding texture mips...");
	}
	if (PreviousInteraction && InGui.PointerState().bCancel)
	{
		Document.CancelInteraction(PreviousInteraction);
		InGui.FinishEditing();
	}
	if (!Document.Error.empty())
	{
		InGui.TextWrapped(Document.Error);
	}
}

void FAssetWorkspace::DrawModelProperties(FGui& InGui, FEntry& InEntry)
{
	auto& Document = *InEntry.Document;
	const auto Model = InEntry.Preview ? InEntry.Preview->As<FModelAsset>() : Document.Loaded().As<FModelAsset>();
	std::size_t Vertices{};
	std::size_t Indices{};
	for (const auto& Primitive : Model->Primitives)
	{
		Vertices += Primitive.Positions.size() / 3;
		Indices += Primitive.Indices.size();
	}
	AssetInfo(InGui, "Geometry",
	          std::to_string(Vertices) + " vertices | " + std::to_string(Indices / 3) + " triangles");
	AssetInfo(InGui, "Nodes / primitives",
	          std::to_string(Model->Nodes.size()) + " / " + std::to_string(Model->Primitives.size()));
	if (InEntry.PreviewModel)
	{
		ShowBounds(InGui, InEntry.PreviewModel->Bounds);
	}
	auto Slots = ReadValue<std::vector<FAssetRef>>(Document.Get("materialSlots"));
	if (InGui.Section("Material slots"))
	{
		for (std::size_t I = 0; I < Slots.size(); ++I)
		{
			if (EditReference(InGui, ("Slot " + std::to_string(I)).c_str(), Slots[I], RecordType<FMaterialAsset>().Id))
			{
				CommitReferenceEdit(InEntry, "materialSlots", WriteValue(Slots));
			}
		}
	}
	DrawModelNodes(InGui, InEntry);
	DrawModelPrimitives(InGui, InEntry, *Model);
}

void FAssetWorkspace::DrawModelNodes(FGui& InGui, FEntry& InEntry)
{
	auto& Document = *InEntry.Document;
	auto Nodes = ReadValue<std::vector<FModelNode>>(Document.Get("nodes"));
	if (!Nodes.empty() && InGui.Section("Nodes"))
	{
		for (const auto Root : ReadValue<std::vector<std::uint32_t>>(Document.Get("roots")))
		{
			ModelTree(InGui, Nodes, Root, InEntry.SelectedNode);
		}
		InEntry.SelectedNode = std::min(InEntry.SelectedNode, Nodes.size() - 1);
		auto& Node = Nodes[InEntry.SelectedNode];
		AssetInfo(InGui, "Stable node ID", Node.Id);
		AssetInfo(InGui, "Children / primitives",
		          std::to_string(Node.Children.size()) + " / " + std::to_string(Node.Primitives.size()));
		EditField(
		    InGui, InEntry, "nodes",
		    [&]
		    {
			    return AssetText(InGui, "Node name", Node.Name, false);
		    },
		    [&]
		    {
			    ValidateNodeHierarchy(Nodes);
			    return WriteValue(Nodes);
		    },
		    false);
		ObserveProperty(InGui, "node/name");
		auto Transform = DecomposeAffine(Node.Local);
		EditField(
		    InGui, InEntry, "nodes",
		    [&]
		    {
			    std::array<FVec4, 3> ComponentBounds;
			    bool bChanged =
			        InGui.InputVectorRow("Position", Transform.Position, "", "Local position", ComponentBounds);
			    Bounds["node/position/x"] = ComponentBounds[0];
			    ObserveProperty(InGui, "node/position");
			    FVec3 Degrees{Transform.Rotation.X * 57.2957795f, Transform.Rotation.Y * 57.2957795f,
			                  Transform.Rotation.Z * 57.2957795f};
			    if (InGui.InputVectorRow("Rotation", Degrees, "deg", "Local rotation", ComponentBounds))
			    {
				    Transform.Rotation = {Degrees.X / 57.2957795f, Degrees.Y / 57.2957795f, Degrees.Z / 57.2957795f};
				    bChanged = true;
			    }
			    bChanged |= InGui.InputVectorRow("Scale", Transform.Scale, "", "Local scale", ComponentBounds);
			    return bChanged;
		    },
		    [&]
		    {
			    Node.Local = ComposeAffine(Transform);
			    ValidateNodeHierarchy(Nodes);
			    return WriteValue(Nodes);
		    });
		AssetInfo(InGui, "Shear (preserved)",
		          std::to_string(Transform.Shear.X) + ", " + std::to_string(Transform.Shear.Y) + ", " +
		              std::to_string(Transform.Shear.Z));
	}
}

void FAssetWorkspace::DrawModelPrimitives(FGui& InGui, FEntry& InEntry, const FModelAsset& InModel)
{
	auto& Document = *InEntry.Document;
	const auto* Model = &InModel;
	const auto Slots = ReadValue<std::vector<FAssetRef>>(Document.Get("materialSlots"));
	if (!Model->Primitives.empty() && InGui.Section("Primitives"))
	{
		auto Primitives = Document.Get("primitives");
		auto& Array = std::get<FArchiveNode::FArray>(Primitives.Value);
		std::vector<std::string> Names;
		for (auto& Primitive : Array)
		{
			const auto& Values = Fields(Primitive);
			Names.push_back(ReadValue<std::string>(Values.at("name")) + " [" + ReadValue<std::string>(Values.at("id")) +
			                "]");
		}
		InEntry.SelectedPrimitive = std::min(InEntry.SelectedPrimitive, Names.size() - 1);
		AssetCombo(InGui, "Selected primitive", Names, InEntry.SelectedPrimitive);
		auto& Primitive = Fields(Array.at(InEntry.SelectedPrimitive));
		auto Name = ReadValue<std::string>(Primitive.at("name"));
		EditField(
		    InGui, InEntry, "primitives",
		    [&]
		    {
			    return AssetText(InGui, "Primitive name", Name, false);
		    },
		    [&]
		    {
			    Primitive["name"] = WriteValue(Name);
			    return Primitives;
		    },
		    false);
		const auto& Geometry = Model->Primitives[InEntry.SelectedPrimitive];
		AssetInfo(InGui, "Vertices", std::to_string(Geometry.Positions.size() / 3));
		AssetInfo(InGui, "Indices", std::to_string(Geometry.Indices.size()));
		AssetInfo(InGui, "Triangles", std::to_string(Geometry.Indices.size() / 3));
		if (InEntry.PreviewModel)
		{
			ShowBounds(InGui, InEntry.PreviewModel->PrimitiveBounds.at(InEntry.SelectedPrimitive));
		}
		AssetInfo(InGui, "Stable primitive ID", Geometry.Id);
		AssetInfo(InGui, "Vertex attributes",
		          std::string("Position") + (Geometry.Normals.empty() ? "" : ", Normal") +
		              (Geometry.Tangents.empty() ? "" : ", Tangent") + (Geometry.Colors.empty() ? "" : ", Color") +
		              (Geometry.TexCoords0.empty() ? "" : ", UV0") + (Geometry.TexCoords1.empty() ? "" : ", UV1"));
		std::vector<std::string> Choices;
		for (std::size_t I = 0; I < Slots.size(); ++I)
		{
			Choices.push_back(std::to_string(I) + ": " + Slots[I].Path);
		}
		std::size_t Slot = static_cast<std::size_t>(ReadValue<std::int32_t>(Primitive.at("material")));
		if (AssetCombo(InGui, "Material slot", Choices, Slot))
		{
			Primitive["material"] = WriteValue(static_cast<std::int32_t>(Slot));
			CommitModelPrimitiveFields(Document, Primitives);
		}
	}
}

void FAssetWorkspace::DrawSkyProperties(FGui& InGui, FEntry& InEntry)
{
	const auto Sky = ReadValue<FSkyAsset>(InEntry.Document->Snapshot());
	AssetInfo(InGui, "Radiance cube", Sky.Radiance.Path);
	AssetInfo(InGui, "Specular cube", Sky.Specular.Path);
	AssetInfo(InGui, "BRDF LUT", Sky.Brdf.Path);
	AssetInfo(InGui, "Bake convention", std::to_string(Sky.Convention));
	constexpr std::array ProductNames{"Radiance", "Specular", "BRDF"};
	constexpr std::array FormatNames{"RGBA8", "RGBA16F", "RGBA32F"};
	for (std::size_t Index = 0; Index < InEntry.SkyProducts.size(); ++Index)
	{
		if (const auto& Texture = InEntry.SkyProducts[Index])
		{
			AssetInfo(InGui, ProductNames[Index],
			          std::to_string(Texture->Mips.front().Width) + " x " +
			              std::to_string(Texture->Mips.front().Height) + ", " + std::to_string(Texture->Mips.size()) +
			              " mips, " + FormatNames.at(static_cast<std::size_t>(Texture->Format)));
		}
	}
	InGui.TextWrapped("Baked products are read-only. Preview spheres show diffuse and reflective lighting.");
	if (InGui.Section("Irradiance SH9", false))
	{
		for (std::size_t I = 0; I < Sky.Irradiance.size(); ++I)
		{
			const auto& C = Sky.Irradiance[I];
			AssetInfo(InGui, std::to_string(I).c_str(),
			          std::to_string(C[0]) + ", " + std::to_string(C[1]) + ", " + std::to_string(C[2]));
		}
	}
}
} // namespace Hyperion
