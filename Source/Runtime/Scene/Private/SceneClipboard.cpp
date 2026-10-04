#include "Hyperion/Scene/SceneClipboard.h"

namespace Hyperion
{
namespace
{
std::size_t ArchiveBytes(const FArchiveNode& InArchive)
{
	std::size_t Bytes{};
	std::vector<const FArchiveNode*> Pending{&InArchive};
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		const auto& Value = Pending[Index]->Value;
		Bytes += sizeof(FArchiveNode);
		if (const auto Text = std::get_if<std::string>(&Value))
		{
			Bytes += Text->size();
		}
		else if (const auto Bulk = std::get_if<FBulkData>(&Value))
		{
			Bytes += Bulk->Data().size() + GetBulkElementInfo(Bulk->Element).WireName.size();
		}
		else if (const auto Array = std::get_if<FArchiveNode::FArray>(&Value))
		{
			for (const auto& Item : *Array)
			{
				Pending.push_back(&Item);
			}
		}
		else if (const auto Object = std::get_if<FArchiveNode::FObject>(&Value))
		{
			for (const auto& [Key, Item] : *Object)
			{
				Bytes += Key.size();
				Pending.push_back(&Item);
			}
		}
		if (Bytes > SceneClipboardMaxBytes || Pending.size() > SceneClipboardMaxBytes / sizeof(FArchiveNode))
		{
			throw std::invalid_argument("Scene clipboard exceeds the authored-data budget");
		}
	}
	return Bytes;
}

std::size_t MaterialValueBytes(const FMaterialValue& InValue)
{
	std::size_t Bytes = sizeof(InValue) + InValue.Words.size() * sizeof(std::uint32_t);
	for (const auto& Element : InValue.Elements)
	{
		Bytes += MaterialValueBytes(Element);
	}
	std::vector<const FMaterialParameterType*> Types{&InValue.Type};
	for (std::size_t Index = 0; Index < Types.size(); ++Index)
	{
		const auto& Type = *Types[Index];
		Bytes += sizeof(Type);
		for (const auto& Name : Type.MemberNames)
		{
			Bytes += sizeof(Name) + Name.size();
		}
		for (const auto& Member : Type.Members)
		{
			Types.push_back(&Member);
		}
	}
	return Bytes;
}

void FreezeMaterial(FSceneMaterialSelection& InSelection, std::size_t& OutBytes)
{
	if (InSelection.Instance)
	{
		InSelection.Snapshot = InSelection.Instance->Freeze();
		InSelection.Instance.reset();
	}
	OutBytes += sizeof(InSelection);
	const auto Count = [&](const FMaterialParameterValues& InValues)
	{
		for (const auto& Value : InValues)
		{
			OutBytes +=
			    sizeof(Value) + Value.Name.size() + Value.Semantic.GetStorageBytes() + MaterialValueBytes(Value.Value);
		}
	};
	Count(InSelection.Overrides);
	if (InSelection.Snapshot)
	{
		Count(InSelection.Snapshot->Overrides);
	}
	for (const auto& [Reference, Texture] : InSelection.TextureAssets)
	{
		OutBytes += sizeof(Reference) + Reference.Id.size() + Reference.Path.size() + Reference.Revision.size();
	}
	if (InSelection.Reference)
	{
		OutBytes += InSelection.Reference->Id.size() + InSelection.Reference->Path.size() +
		            InSelection.Reference->Revision.size();
	}
}
} // namespace

void ConfigureSceneClipboardComponent(FSceneComponentDescriptor& InDescriptor)
{
	const auto* Record = InDescriptor.Record;
	const auto Get = InDescriptor.Get;
	InDescriptor.ClipboardClone = [Record, Get](const std::any& InState, std::size_t& OutBytes)
	{
		auto State = InState;
		OutBytes = sizeof(State) + ArchiveBytes(WriteRecord(*Record, Get(State)));
		if (Record->CppType == typeid(FSceneModelComponent))
		{
			auto& Model = *std::any_cast<std::optional<FSceneModelComponent>&>(State);
			FreezeMaterial(Model.Surface, OutBytes);
			for (auto& [Section, Surface] : Model.SectionSurfaces)
			{
				FreezeMaterial(Surface, OutBytes);
			}
		}
		return State;
	};
	if (Record->CppType == typeid(FSceneTransform))
	{
		InDescriptor.ClipboardReferences = [](std::any& InState, const auto& InVisitor)
		{
			InVisitor(std::any_cast<std::optional<FSceneTransform>&>(InState)->Parent);
		};
	}
	else if (Record->CppType == typeid(FSceneModelSource))
	{
		InDescriptor.ClipboardReferences = [](std::any& InState, const auto& InVisitor)
		{
			InVisitor(std::any_cast<std::optional<FSceneModelSource>&>(InState)->InstanceRoot);
		};
	}
}

FSceneNode CloneSceneClipboardNode(const FSceneNode& InNode, std::size_t& OutBytes)
{
	if (!InNode.Components.Unknown().empty())
	{
		throw std::invalid_argument("Cannot copy objects with unregistered components");
	}
	auto Node = InNode;
	OutBytes += sizeof(Node) + Node.Id.size() + Node.Name.size();
	for (const auto& Component : InNode.Components.All())
	{
		if (!Component.Get())
		{
			continue;
		}
		if (!Component.Type->ClipboardClone)
		{
			throw std::invalid_argument("Component does not support clipboard snapshots: " + Component.Type->Id);
		}
		std::size_t Bytes{};
		Node.Components.Find(Component.Id)->State = Component.Type->ClipboardClone(Component.State, Bytes);
		if (Bytes > SceneClipboardMaxBytes || OutBytes > SceneClipboardMaxBytes - Bytes)
		{
			throw std::invalid_argument("Scene clipboard exceeds the 64 MiB authored-data budget");
		}
		OutBytes += Bytes + Component.Id.size();
	}
	if (OutBytes > SceneClipboardMaxBytes)
	{
		throw std::invalid_argument("Scene clipboard exceeds the 64 MiB authored-data budget");
	}
	return Node;
}

void VisitSceneClipboardReferences(FSceneNode& InNode, const std::function<void(std::string&)>& InVisitor)
{
	for (const auto& Component : InNode.Components.All())
	{
		if (Component.Get() && Component.Type->ClipboardReferences)
		{
			Component.Type->ClipboardReferences(InNode.Components.Find(Component.Id)->State, InVisitor);
		}
	}
}
} // namespace Hyperion
