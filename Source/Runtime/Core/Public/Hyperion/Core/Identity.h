#pragma once
#include <string>

namespace Hyperion
{
// Random 128-bit namespace for ephemeral process/document identities, not an authentication secret.
std::string CreateEphemeralIdentity();
} // namespace Hyperion
