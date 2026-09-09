#include "Hyperion/Materials/MaterialSemantics.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
FMaterialSemanticRegistry::FMaterialSemanticRegistry()
{
	auto Numeric = [this](std::string InName, EMaterialScalar InScalar, std::uint32_t InColumns, EMaterialScope InScope,
	                      std::string InConvention, std::uint32_t InRows = 1)
	{
		Add({std::move(InName), FMaterialParameterType::Numeric(InScalar, InColumns, InRows), InScope,
		     std::move(InConvention)});
	};
	Numeric("Engine.Frame.Time", EMaterialScalar::Float, 1, EMaterialScope::Frame, "Session elapsed seconds");
	Numeric("Engine.Frame.Index", EMaterialScalar::Uint, 1, EMaterialScope::Frame,
	        "Session frame serial modulo uint32");
	Numeric("Engine.View.CameraPosition", EMaterialScalar::Float, 3, EMaterialScope::View, "World-space eye position");
	Numeric("Engine.View.ViewProjection", EMaterialScalar::Float, 4, EMaterialScope::View,
	        "Column-vector world to clip", 4);
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		Numeric("Engine.View.ShadowMatrix" + std::to_string(Index), EMaterialScalar::Float, 4, EMaterialScope::View,
		        "Column-vector world to shadow clip, zero-to-one depth", 4);
		Add({"Engine.View.ShadowDepth" + std::to_string(Index),
		     FMaterialParameterType::Resource(EMaterialValueKind::Texture2D), EMaterialScope::View,
		     "Directional cascade sampled D32 depth"});
	}
	for (const auto* Name :
	     {"ShadowSplits", "ShadowTexels", "ShadowRanges", "ShadowCamera", "ShadowFilter", "ShadowControl"})
	{
		Numeric(std::string("Engine.View.") + Name, EMaterialScalar::Float, 4, EMaterialScope::View,
		        "Cascaded directional shadow contract V1");
	}
	Add({"Engine.View.ShadowSampler", FMaterialParameterType::Resource(EMaterialValueKind::Sampler),
	     EMaterialScope::View, "Less-equal depth comparison sampler"});
	Numeric("Engine.Object.World", EMaterialScalar::Float, 4, EMaterialScope::Object, "Column-vector local to world",
	        4);
	Numeric("Engine.Object.Normal", EMaterialScalar::Float, 4, EMaterialScope::Object,
	        "Inverse transpose local to world", 4);
	Numeric("Engine.Object.OrientationSign", EMaterialScalar::Float, 1, EMaterialScope::Object,
	        "World determinant sign");
	Numeric("Engine.Object.WorldViewProjection", EMaterialScalar::Float, 4, EMaterialScope::Object,
	        "Derived from World and ViewProjection", 4);
	Numeric("Engine.Scene.MainDirectionalLightDirection", EMaterialScalar::Float, 3, EMaterialScope::Scene,
	        "Unit world-space surface-to-light vector");
	Numeric("Engine.Scene.MainDirectionalLightColor", EMaterialScalar::Float, 3, EMaterialScope::Scene,
	        "Linear RGB radiance");
	Numeric("Engine.Scene.AmbientColor", EMaterialScalar::Float, 3, EMaterialScope::Scene,
	        "Linear ambient RGB radiance");
	Numeric("Pbr.AlphaMode", EMaterialScalar::Uint, 1, EMaterialScope::Material, "0 opaque, 1 masked, 2 blended");
	Numeric("Pbr.DoubleSided", EMaterialScalar::Bool, 1, EMaterialScope::Material, "Flip the back-face shading normal");
	Numeric("Pbr.Unlit", EMaterialScalar::Bool, 1, EMaterialScope::Material, "Use unlit base color and emissive");
	Numeric("Pbr.HasNormal", EMaterialScalar::Bool, 1, EMaterialScope::Material,
	        "Enable tangent-space normal sampling");
	Numeric("Pbr.BaseColorFactor", EMaterialScalar::Float, 4, EMaterialScope::Material, "Linear RGBA multiplier");
	Numeric("Pbr.EmissiveFactor", EMaterialScalar::Float, 3, EMaterialScope::Material, "Linear RGB multiplier");
	Numeric("Pbr.MetallicFactor", EMaterialScalar::Float, 1, EMaterialScope::Material, "Metallic scalar");
	Numeric("Pbr.RoughnessFactor", EMaterialScalar::Float, 1, EMaterialScope::Material, "Perceptual roughness");
	Numeric("Pbr.NormalScale", EMaterialScalar::Float, 1, EMaterialScope::Material,
	        "Tangent-space normal XY multiplier");
	Numeric("Pbr.OcclusionStrength", EMaterialScalar::Float, 1, EMaterialScope::Material, "Occlusion blend strength");
	Numeric("Pbr.AlphaCutoff", EMaterialScalar::Float, 1, EMaterialScope::Material, "Masked alpha threshold");
	const std::array<std::string, 5> Roles{"BaseColor", "MetallicRoughness", "Normal", "Occlusion", "Emissive"};
	const std::array<std::string, 5> Aliases{"ALBEDO_TEXTURE", "METALLIC_ROUGHNESS_TEXTURE", "NORMAL_TEXTURE",
	                                         "OCCLUSION_TEXTURE", "EMISSIVE_TEXTURE"};
	for (std::size_t Index = 0; Index < Roles.size(); ++Index)
	{
		const std::string Convention = Index == 0 || Index == 4 ? "sRGB source decoded to linear"
		                               : Index == 2             ? "Linear tangent-space XYZ normal"
		                                                        : "Linear data channels";
		Add({"Pbr." + Roles[Index] + "Texture",
		     FMaterialParameterType::Resource(EMaterialValueKind::Texture2D),
		     EMaterialScope::Material,
		     Convention,
		     true,
		     {Aliases[Index]}});
		Add({"Pbr." + Roles[Index] + "Sampler", FMaterialParameterType::Resource(EMaterialValueKind::Sampler),
		     EMaterialScope::Material, "glTF texture sampler"});
		Numeric("Pbr." + Roles[Index] + "UvSet", EMaterialScalar::Uint, 1, EMaterialScope::Material,
		        "Vertex texture coordinate set 0 or 1");
	}
}

void FMaterialSemanticRegistry::Add(FMaterialSemantic InSemantic)
{
	InSemantic.Type.Validate();
	if (InSemantic.Name.empty() || InSemantic.Convention.empty() || InSemantic.Scope >= EMaterialScope::Count)
	{
		throw std::invalid_argument("Invalid material semantic contract");
	}
	std::vector<std::string> Names = InSemantic.Aliases;
	Names.push_back(InSemantic.Name);
	std::sort(Names.begin(), Names.end());
	if (Names.front().empty() || std::adjacent_find(Names.begin(), Names.end()) != Names.end())
	{
		throw std::invalid_argument("Duplicate or empty material semantic alias");
	}
	for (const FMaterialSemantic& Existing : Semantics)
	{
		for (const std::string& Name : Names)
		{
			if (Existing.Name == Name ||
			    std::find(Existing.Aliases.begin(), Existing.Aliases.end(), Name) != Existing.Aliases.end())
			{
				throw std::invalid_argument("Conflicting material semantic registration: " + Name);
			}
		}
	}
	Semantics.push_back(std::move(InSemantic));
}

void FMaterialSemanticRegistry::Register(FMaterialSemantic InSemantic)
{
	if (bFrozen || InSemantic.Name.find('.') == std::string::npos || InSemantic.Name.starts_with("Engine.") ||
	    InSemantic.Name.starts_with("Pbr."))
	{
		throw std::invalid_argument("Material semantics must be registered before freeze in a custom namespace");
	}
	for (const std::string& Alias : InSemantic.Aliases)
	{
		if (Alias.find('.') == std::string::npos || Alias.starts_with("Engine.") || Alias.starts_with("Pbr."))
		{
			throw std::invalid_argument("Custom semantic aliases require a custom namespace");
		}
	}
	Add(std::move(InSemantic));
	++Version;
}

void FMaterialSemanticRegistry::Freeze()
{
	bFrozen = true;
}

std::uint64_t FMaterialSemanticRegistry::GetVersion() const
{
	return Version;
}

const FMaterialSemantic& FMaterialSemanticRegistry::Find(std::string_view InName) const
{
	for (const FMaterialSemantic& Semantic : Semantics)
	{
		if (Semantic.Name == InName ||
		    std::find(Semantic.Aliases.begin(), Semantic.Aliases.end(), InName) != Semantic.Aliases.end())
		{
			return Semantic;
		}
	}
	throw std::invalid_argument("Unknown material semantic: " + std::string(InName));
}

std::string FMaterialSemanticRegistry::Normalize(std::string_view InName) const
{
	return Find(InName).Name;
}

FMaterialParameterDeclaration DeclareMaterialSemantic(std::string InName, std::string_view InSemantic,
                                                      const FMaterialSemanticRegistry& InRegistry)
{
	const FMaterialSemantic& Semantic = InRegistry.Find(InSemantic);
	FMaterialParameterDeclaration Result;
	Result.Name = std::move(InName);
	Result.Type = Semantic.Type;
	Result.Semantic = Semantic.Name;
	Result.bRequired = Semantic.bRequired;
	if (!Semantic.Name.starts_with("Pbr."))
	{
		Result.Source = EMaterialParameterSource::Semantic;
		Result.OverridePolicy = EMaterialOverridePolicy::Locked;
	}
	return Result;
}

std::shared_ptr<const FMaterialSemanticRegistry> GetStandardMaterialSemantics()
{
	static const std::shared_ptr<const FMaterialSemanticRegistry> Registry = []
	{
		auto Result = std::make_shared<FMaterialSemanticRegistry>();
		Result->Freeze();
		return Result;
	}();
	return Registry;
}
} // namespace Hyperion
