#include "Hyperion/IO/MountedFileSystem.h"
#include <nlohmann/json.hpp>

namespace Hyperion
{
std::shared_ptr<FMountedFileSystem> LoadContentMounts(const std::filesystem::path& InConfiguration, bool bInAuthoring)
{
	FLocalFileSystem Local;
	const auto Configuration = std::filesystem::absolute(InConfiguration).lexically_normal();
	const auto Bytes = Local.Read(Configuration, 1024 * 1024);
	const auto Json = nlohmann::json::parse(reinterpret_cast<const char*>(Bytes.data()),
	                                        reinterpret_cast<const char*>(Bytes.data() + Bytes.size()));
	if (Json.at("version") != 1)
	{
		throw std::invalid_argument("Unsupported content mount configuration version");
	}
	std::vector<FContentMount> Mounts;
	for (const auto& Mount : Json.at("mounts"))
	{
		Mounts.push_back({PathFromUtf8(Mount.at("root").get<std::string>()),
		                  Configuration.parent_path() / PathFromUtf8(Mount.at("directory").get<std::string>()),
		                  !bInAuthoring && Mount.value("read_only", true)});
	}
	return std::make_shared<FMountedFileSystem>(std::move(Mounts));
}
} // namespace Hyperion
