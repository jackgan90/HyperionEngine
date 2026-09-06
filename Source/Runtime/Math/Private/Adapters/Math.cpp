#include "Hyperion/Math/Math.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Hyperion
{
namespace
{
FMat4 Owned(const glm::mat4& InV)
{
	FMat4 R;
	std::copy_n(glm::value_ptr(InV), 16, R.Values.begin());
	return R;
}

glm::mat4 Native(const FMat4& InV)
{
	return glm::make_mat4(InV.Values.data());
}
} // namespace

FMat4 Identity()
{
	return Owned(glm::mat4(1));
}

FMat4 Scale(FVec3 InV)
{
	return Owned(glm::scale(glm::mat4(1), glm::vec3(InV.X, InV.Y, InV.Z)));
}

FMat4 Translation(FVec3 InV)
{
	return Owned(glm::translate(glm::mat4(1), glm::vec3(InV.X, InV.Y, InV.Z)));
}

FMat4 Multiply(const FMat4& InA, const FMat4& InB)
{
	return Owned(Native(InA) * Native(InB));
}

FVec4 Transform(const FMat4& InM, FVec4 InV)
{
	auto R = Native(InM) * glm::vec4(InV.X, InV.Y, InV.Z, InV.W);
	return {R.x, R.y, R.z, R.w};
}
} // namespace Hyperion
