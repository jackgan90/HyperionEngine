#pragma once
#include <cstdint>

namespace Hyperion
{
struct FScenePublicationToken
{
	std::uint64_t LogicalSceneIdentity{};
	std::uint64_t AttachmentEpoch{};
	std::uint64_t PublicationSerial{};
	std::uint64_t LogicalRevision{};
	bool operator==(const FScenePublicationToken&) const = default;
};
} // namespace Hyperion
