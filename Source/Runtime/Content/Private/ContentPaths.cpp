#include "Hyperion/Content/ContentPaths.h"

namespace Hyperion
{
namespace
{
bool IsRootedPath(std::string_view InPath, std::string_view InRoot)
{
	return InPath == InRoot ||
	       (InPath.starts_with(InRoot) && InPath.size() > InRoot.size() && InPath[InRoot.size()] == '/');
}
} // namespace

bool IsGameContentPath(std::string_view InPath)
{
	return IsRootedPath(InPath, GameContentRoot);
}

bool IsEngineContentPath(std::string_view InPath)
{
	return IsRootedPath(InPath, EngineContentRoot);
}
} // namespace Hyperion
