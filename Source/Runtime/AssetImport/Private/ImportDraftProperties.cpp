#include "Hyperion/AssetImport/ImportDraft.h"
#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
namespace
{
void CheckName(const std::string& InName)
{
	if (InName.size() > 1024 || InName.find('\0') != std::string::npos)
	{
		throw std::invalid_argument("Property names must be at most 1024 bytes without NUL");
	}
}

void EditModel(FModelAsset& OutModel, const FImportPropertyEdits& InEdits)
{
	std::set<std::string> Ids;
	for (const auto& Edit : InEdits.Nodes)
	{
		const auto Node = std::find_if(
		    OutModel.Nodes.begin(), OutModel.Nodes.end(),
		    [&](const auto& InNode)
		    {
			    return ModelNodeId(OutModel, static_cast<std::size_t>(&InNode - OutModel.Nodes.data())) == Edit.Id;
		    });
		if (Node == OutModel.Nodes.end() || !Ids.insert(Edit.Id).second)
		{
			throw std::invalid_argument("Missing or duplicate model node ID: " + Edit.Id);
		}
		if (Edit.Name)
		{
			CheckName(*Edit.Name);
			Node->Name = *Edit.Name;
		}
		if (Edit.Local)
		{
			Node->Local = *Edit.Local;
		}
	}
	Ids.clear();
	for (const auto& Edit : InEdits.Primitives)
	{
		const auto Primitive = std::find_if(
		    OutModel.Primitives.begin(), OutModel.Primitives.end(),
		    [&](const auto& InPrimitive)
		    {
			    return ModelPrimitiveId(OutModel,
			                            static_cast<std::size_t>(&InPrimitive - OutModel.Primitives.data())) == Edit.Id;
		    });
		if (Primitive == OutModel.Primitives.end() || !Ids.insert(Edit.Id).second)
		{
			throw std::invalid_argument("Missing or duplicate primitive ID: " + Edit.Id);
		}
		if (Edit.Name)
		{
			CheckName(*Edit.Name);
			Primitive->Name = *Edit.Name;
		}
		if (Edit.Material)
		{
			Primitive->Material = *Edit.Material;
		}
	}
	ValidateModel(OutModel);
}
} // namespace

FConvertedAsset ApplyImportProperties(const FConvertedAsset& InSource, const FImportPropertyEdits& InEdits)
{
	(void)ImportPropertiesKey(InEdits);
	auto Result = InSource;
	if (!InEdits.Name && InEdits.Nodes.empty() && InEdits.Primitives.empty() && InEdits.Material.empty())
	{
		return Result;
	}
	if ((!InEdits.Nodes.empty() || !InEdits.Primitives.empty()) && InSource.Type->CppType != typeid(FModelAsset))
	{
		throw std::invalid_argument("Node and primitive edits require a model");
	}
	if (!InEdits.Material.empty() && InSource.Type->CppType != typeid(FMaterialAsset))
	{
		throw std::invalid_argument("Numeric material edits require a material root");
	}
	auto Record = WriteRecord(*InSource.Type, InSource.Object.get());
	auto& Fields = std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Record.Value).at("fields").Value);
	if (InEdits.Name)
	{
		CheckName(*InEdits.Name);
		if (!Fields.contains("name"))
		{
			throw std::invalid_argument("This asset has no editable display name");
		}
		Fields.at("name") = WriteValue(*InEdits.Name);
	}
	auto Object = ReadRecord(*InSource.Type, Record);
	if (InSource.Type->CppType == typeid(FModelAsset))
	{
		EditModel(*static_cast<FModelAsset*>(Object.get()), InEdits);
	}
	if (!InEdits.Material.empty())
	{
		auto& Material = *static_cast<FMaterialAsset*>(Object.get());
		Material.Values = PrepareMaterialNumeric(Material, InEdits.Material);
	}
	if (InSource.Type->Validate)
	{
		InSource.Type->Validate(Object.get());
	}
	Result.Object = std::move(Object);
	return Result;
}

std::string ImportPropertiesKey(const FImportPropertyEdits& InEdits)
{
	if (InEdits.Nodes.size() > 256 || InEdits.Primitives.size() > 256 || InEdits.Material.size() > 100)
	{
		throw std::invalid_argument("Draft edits exceed the 256 node/primitive or 100 material parameter limit");
	}
	if (!InEdits.Name && InEdits.Nodes.empty() && InEdits.Primitives.empty() && InEdits.Material.empty())
	{
		return {};
	}
	auto Sorted = InEdits;
	const auto ById = [](const auto& InA, const auto& InB)
	{
		return InA.Id < InB.Id;
	};
	std::sort(Sorted.Nodes.begin(), Sorted.Nodes.end(), ById);
	std::sort(Sorted.Primitives.begin(), Sorted.Primitives.end(), ById);
	std::sort(Sorted.Material.begin(), Sorted.Material.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Name < InB.Name;
	          });
	auto Key = WriteJson(WriteRecordWire(RecordType<FImportPropertyEdits>(), &Sorted));
	if (Key.size() > 131072)
	{
		throw std::invalid_argument("Draft overrides exceed 128 KiB");
	}
	return Key;
}
} // namespace Hyperion
