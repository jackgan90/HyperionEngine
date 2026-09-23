#include "Hyperion/Core/Identity.h"
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

namespace Hyperion
{
std::string CreateEphemeralIdentity()
{
	std::random_device Random;
	std::ostringstream Text;
	Text << std::hex << std::setfill('0');
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		Text << std::setw(8) << static_cast<std::uint32_t>(Random());
	}
	return Text.str();
}
} // namespace Hyperion
