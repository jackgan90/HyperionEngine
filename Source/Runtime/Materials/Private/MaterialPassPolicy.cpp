#include "MaterialPassPolicy.h"
#include <stdexcept>

namespace Hyperion::MaterialsPrivate
{
void NormalizeSilhouettePolicy(FMaterialPass& InPass)
{
	if (!InPass.SilhouettePolicy)
	{
		// Exact version-one eligibility, including its intentional lack of a pixel-define restriction.
		const bool bLegacyCoverage =
		    (InPass.Vertex.Path == "Model.hlsl" || InPass.Vertex.Path == "/Engine/Shaders/Model.hlsl") &&
		    InPass.Vertex.Entry == "VSMain" && InPass.Pixel.Path == InPass.Vertex.Path &&
		    InPass.Pixel.Entry == "PSMain" && InPass.Vertex.Defines.empty();
		InPass.SilhouettePolicy =
		    bLegacyCoverage ? EMaterialSilhouettePolicy::ModelShader : EMaterialSilhouettePolicy::Disabled;
	}
	if (*InPass.SilhouettePolicy != EMaterialSilhouettePolicy::Disabled &&
	    *InPass.SilhouettePolicy != EMaterialSilhouettePolicy::ModelShader)
	{
		throw std::invalid_argument("Invalid material silhouette policy");
	}
	if (*InPass.SilhouettePolicy == EMaterialSilhouettePolicy::ModelShader && InPass.Pixel.Path.empty())
	{
		throw std::invalid_argument("Model shader silhouette coverage requires a pixel shader");
	}
}
} // namespace Hyperion::MaterialsPrivate
