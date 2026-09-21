#include "Hyperion/Assets/AssetAuthoring.h"
#include "Hyperion/IO/Path.h"
#include <cctype>

namespace Hyperion
{
std::filesystem::path AssetProductPath(const FRecordDescriptor& InType, const void* InObject,
                                       const std::filesystem::path& InRoot, std::string_view InId)
{
	const std::map<std::string, std::string> Folders{{"hyperion.modelasset", "Models"},
	                                                 {"hyperion.materialasset", "Materials"},
	                                                 {"hyperion.textureasset", "Textures"},
	                                                 {"hyperion.skyasset", "Skies"},
	                                                 {"hyperion.scene", "Scenes"}};
	const auto Folder = Folders.find(InType.Id);
	const auto Record = WriteRecord(InType, InObject);
	const auto& Fields =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Record.Value).at("fields").Value);
	std::string Name = "Asset";
	if (const auto It = Fields.find("name");
	    It != Fields.end() && std::holds_alternative<std::string>(It->second.Value))
	{
		Name = ReadValue<std::string>(It->second);
	}
	for (auto& Character : Name)
	{
		if (!std::isalnum(static_cast<unsigned char>(Character)) && Character != '-' && Character != '_')
		{
			Character = '_';
		}
	}
	Name = Name.substr(0, 64);
	return InRoot / (Folder == Folders.end() ? "Assets" : Folder->second) /
	       (Name + "-" + std::string(InId) + ".hasset");
}

void PrepareAssetReferences(const FRecordDescriptor& InType, void* InObject, IFileSystem& InFiles,
                            const std::filesystem::path& InOutput)
{
	VisitRecord(InType, InObject,
	            [&](const FRecordDescriptor& InRecord, const void* InValue, std::string_view)
	            {
		            if (InRecord.CppType != typeid(FAssetRef))
		            {
			            return;
		            }
		            auto& Reference = *const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue));
		            Reference.Revision.clear();
		            if (!Reference.Path.empty())
		            {
			            const auto Hint = PathFromUtf8(Reference.Path);
			            const auto Target =
			                InFiles.Normalize(IsPackagePath(Hint) ? Hint : InOutput.parent_path() / Hint);
			            Reference.Path = PathToUtf8(
			                IsPackagePath(Target) ? Target : Target.lexically_relative(InOutput.parent_path()));
			            if (Reference.Path.empty())
			            {
				            throw std::runtime_error("Asset reference cannot be made portable");
			            }
		            }
	            });
}
} // namespace Hyperion
