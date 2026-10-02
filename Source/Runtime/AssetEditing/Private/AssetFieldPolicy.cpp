#include "AssetFieldPolicy.h"
#include "Hyperion/Environment/SkyAsset.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
enum class EPolicyKind
{
	ReadOnly,
	Name,
	Nodes,
	MaterialSlots,
	MaterialValues,
	Primitives,
	Encoding
};

struct FFieldPolicy
{
	FRecordMemberIdentity Identity;
	std::type_index CppType;
	EPolicyKind Kind{};
	std::function<FRecordMemberIdentity(const FRecordDescriptor&)> Resolve;
};

template<class T, class M> FFieldPolicy Policy(M T::* InMember, EPolicyKind InKind = EPolicyKind::ReadOnly)
{
	return {ResolveRecordMember(RecordType<T>(), InMember), typeid(T), InKind,
	        [InMember](const FRecordDescriptor& InType)
	        {
		        return ResolveRecordMember(InType, InMember);
	        }};
}

const std::vector<FFieldPolicy>& Policies()
{
	static const std::vector Entries{Policy(&FModelAsset::Name, EPolicyKind::Name),
	                                 Policy(&FModelAsset::Nodes, EPolicyKind::Nodes),
	                                 Policy(&FModelAsset::MaterialSlots, EPolicyKind::MaterialSlots),
	                                 Policy(&FModelAsset::Primitives, EPolicyKind::Primitives),
	                                 Policy(&FModelAsset::Roots),
	                                 Policy(&FModelAsset::Diagnostics),
	                                 Policy(&FMaterialAsset::Name, EPolicyKind::Name),
	                                 Policy(&FMaterialAsset::Values, EPolicyKind::MaterialValues),
	                                 Policy(&FMaterialAsset::Parameters),
	                                 Policy(&FMaterialAsset::Passes),
	                                 Policy(&FMaterialAsset::Version),
	                                 Policy(&FTextureAsset::Name, EPolicyKind::Name),
	                                 Policy(&FTextureAsset::Encoding, EPolicyKind::Encoding),
	                                 Policy(&FTextureAsset::Dimension),
	                                 Policy(&FTextureAsset::Format),
	                                 Policy(&FTextureAsset::Mips),
	                                 Policy(&FSkyAsset::Name, EPolicyKind::Name),
	                                 Policy(&FSkyAsset::Radiance),
	                                 Policy(&FSkyAsset::Specular),
	                                 Policy(&FSkyAsset::Brdf),
	                                 Policy(&FSkyAsset::Irradiance),
	                                 Policy(&FSkyAsset::Convention)};
	return Entries;
}

const FFieldPolicy& FindPolicy(const FRecordDescriptor& InType, const FRecordMemberIdentity& InField)
{
	for (const auto& Entry : Policies())
	{
		if (Entry.CppType == InType.CppType && Entry.Identity == InField)
		{
			if (Entry.Resolve(InType) != InField)
			{
				throw std::invalid_argument("Asset field does not match its canonical record definition");
			}
			return Entry;
		}
	}
	throw std::invalid_argument("Unsupported asset field policy");
}

EAssetFieldRoute Route(EPolicyKind InKind)
{
	switch (InKind)
	{
		case EPolicyKind::Name:
		case EPolicyKind::Nodes:
		case EPolicyKind::MaterialSlots:
		case EPolicyKind::MaterialValues:
			return EAssetFieldRoute::Field;
		case EPolicyKind::Primitives:
			return EAssetFieldRoute::ModelPrimitives;
		case EPolicyKind::Encoding:
			return EAssetFieldRoute::TextureEncoding;
		default:
			return EAssetFieldRoute::ReadOnly;
	}
}

void CollectTextureRequirements(const FMaterialAssetValue& InValue, const FMaterialAssetValue* InBefore,
                                std::vector<FAssetReferenceRequirement>& OutReferences)
{
	if (InValue.Texture && (!InBefore || InBefore->Texture != InValue.Texture))
	{
		OutReferences.push_back({*InValue.Texture, InValue.Type.Kind == EMaterialValueKind::TextureCube
		                                               ? ETextureDimension::Cube
		                                               : ETextureDimension::Texture2D});
	}
	for (std::size_t Index = 0; Index < InValue.Elements.size(); ++Index)
	{
		CollectTextureRequirements(InValue.Elements[Index],
		                           InBefore && Index < InBefore->Elements.size() ? &InBefore->Elements[Index] : nullptr,
		                           OutReferences);
	}
}

FPreparedAssetField PrepareMaterialValues(const FAssetEditDocument& InDocument, FArchiveNode InValue)
{
	auto Material = ReadValue<FMaterialAsset>(InDocument.Snapshot());
	auto Values = ReadValue<FMaterialAssetValues>(InValue);
	FPreparedAssetField Result;
	for (const auto& Parameter : Material.Parameters)
	{
		const auto Before = std::find_if(Material.Values.begin(), Material.Values.end(),
		                                 [&](const auto& InEntry)
		                                 {
			                                 return InEntry.Name == Parameter.Name;
		                                 });
		const auto After = std::find_if(Values.begin(), Values.end(),
		                                [&](const auto& InEntry)
		                                {
			                                return InEntry.Name == Parameter.Name;
		                                });
		const bool bChanged =
		    (Before == Material.Values.end()) != (After == Values.end()) ||
		    (Before != Material.Values.end() && After != Values.end() && Before->Value != After->Value);
		if (bChanged && !CanEditMaterialParameter(Parameter))
		{
			throw std::invalid_argument("Material parameter is read-only: " + Parameter.Name);
		}
		if (bChanged && After != Values.end())
		{
			ClampEditableMaterialParameter(Parameter.Semantic, After->Value);
			CollectTextureRequirements(After->Value, Before == Material.Values.end() ? nullptr : &Before->Value,
			                           Result.References);
		}
	}
	Material.Values = std::move(Values);
	ValidateMaterialAsset(Material);
	Result.Value = WriteValue(Material.Values);
	return Result;
}

FPreparedAssetField PrepareNodes(const FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField,
                                 FArchiveNode InValue)
{
	const auto Nodes = ReadValue<std::vector<FModelNode>>(InValue);
	const auto Before = ReadValue<std::vector<FModelNode>>(InDocument.Get(InField.FieldId));
	if (Nodes.size() != Before.size())
	{
		throw std::invalid_argument("Model node editing preserves topology");
	}
	for (std::size_t Index = 0; Index < Nodes.size(); ++Index)
	{
		if (Nodes[Index].Id != Before[Index].Id || Nodes[Index].Children != Before[Index].Children ||
		    Nodes[Index].Primitives != Before[Index].Primitives)
		{
			throw std::invalid_argument("Model node identity and topology are read-only");
		}
	}
	ValidateNodeHierarchy(Nodes);
	return {std::move(InValue)};
}

FPreparedAssetField PrepareMaterialSlots(const FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField,
                                         FArchiveNode InValue)
{
	const auto Slots = ReadValue<std::vector<FAssetRef>>(InValue);
	const auto Before = ReadValue<std::vector<FAssetRef>>(InDocument.Get(InField.FieldId));
	if (Slots.size() != Before.size())
	{
		throw std::invalid_argument("Material slot count is fixed");
	}
	FPreparedAssetField Result{std::move(InValue)};
	for (std::size_t Index = 0; Index < Slots.size(); ++Index)
	{
		ValidateAssetRef(Slots[Index]);
		if (Slots[Index].TypeId != RecordType<FMaterialAsset>().Id)
		{
			throw std::invalid_argument("Model slots require material references");
		}
		if (Slots[Index] != Before[Index])
		{
			Result.References.push_back({Slots[Index], {}});
		}
	}
	return Result;
}

bool NodeTransformsChanged(const FArchiveNode& InBefore, const FArchiveNode& InAfter)
{
	const auto Before = ReadValue<std::vector<FModelNode>>(InBefore);
	const auto After = ReadValue<std::vector<FModelNode>>(InAfter);
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		if (Before[Index].Local.Values != After.at(Index).Local.Values)
		{
			return true;
		}
	}
	return false;
}

bool PrimitiveMaterialsChanged(const FArchiveNode& InBefore, const FArchiveNode& InAfter)
{
	const auto& Before = std::get<FArchiveNode::FArray>(InBefore.Value);
	const auto& After = std::get<FArchiveNode::FArray>(InAfter.Value);
	const auto Material = ResolveRecordMember(RecordType<FModelPrimitive>(), &FModelPrimitive::Material);
	for (std::size_t Index = 0; Index < Before.size(); ++Index)
	{
		const auto Get = [&](const FArchiveNode& InNode) -> const FArchiveNode&
		{
			return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InNode.Value).at("fields").Value)
			    .at(Material.FieldId);
		};
		if (!EqualInspectionValue(Get(Before[Index]), Get(After.at(Index))))
		{
			return true;
		}
	}
	return false;
}
} // namespace

FAssetFieldPolicy ResolveAssetFieldPolicy(const FRecordDescriptor& InType, const FRecordMemberIdentity& InField)
{
	const auto& Entry = FindPolicy(InType, InField);
	return {Entry.Identity, Route(Entry.Kind)};
}

FAssetFieldPolicy ResolveAssetFieldPolicy(const FRecordDescriptor& InType, std::string_view InField)
{
	for (const auto& Entry : Policies())
	{
		if (Entry.CppType == InType.CppType && Entry.Identity.FieldId == InField)
		{
			return ResolveAssetFieldPolicy(InType, Entry.Identity);
		}
	}
	throw std::invalid_argument("Unsupported asset field: " + std::string(InField));
}

FAssetFieldPolicy AssetNamePolicy(const FRecordDescriptor& InType)
{
	for (const auto& Entry : Policies())
	{
		if (Entry.CppType == InType.CppType && Entry.Kind == EPolicyKind::Name)
		{
			return ResolveAssetFieldPolicy(InType, Entry.Identity);
		}
	}
	throw std::invalid_argument("Unsupported asset name policy");
}

FPreparedAssetField PrepareAssetField(const FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField,
                                      FArchiveNode InValue)
{
	switch (FindPolicy(*InDocument.Loaded().Type, InField).Kind)
	{
		case EPolicyKind::Name:
			(void)ReadValue<std::string>(InValue);
			return {std::move(InValue)};
		case EPolicyKind::Nodes:
			return PrepareNodes(InDocument, InField, std::move(InValue));
		case EPolicyKind::MaterialSlots:
			return PrepareMaterialSlots(InDocument, InField, std::move(InValue));
		case EPolicyKind::MaterialValues:
			return PrepareMaterialValues(InDocument, std::move(InValue));
		default:
			throw std::invalid_argument("Field is read-only or requires its dedicated asset operation");
	}
}

bool AssetFieldAffectsPreview(const FAssetEditDocument& InDocument, const FRecordMemberIdentity& InField,
                              const FArchiveNode& InValue)
{
	switch (FindPolicy(*InDocument.Loaded().Type, InField).Kind)
	{
		case EPolicyKind::Name:
			return false;
		case EPolicyKind::Nodes:
			return NodeTransformsChanged(InDocument.Get(InField.FieldId), InValue);
		case EPolicyKind::Primitives:
			return PrimitiveMaterialsChanged(InDocument.Get(InField.FieldId), InValue);
		case EPolicyKind::MaterialSlots:
		case EPolicyKind::MaterialValues:
			return !EqualInspectionValue(InDocument.Get(InField.FieldId), InValue);
		case EPolicyKind::Encoding:
			return !EqualInspectionValue(InDocument.Snapshot(), InValue);
		default:
			throw std::invalid_argument("Read-only asset fields have no edit effect");
	}
}
} // namespace Hyperion
