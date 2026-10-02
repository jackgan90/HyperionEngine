#include "../ShaderCompileTarget.h"
#include <stdexcept>

namespace Hyperion::ShadersPrivate
{
namespace
{
std::string RegisterShiftFlag(EShaderRegisterClass InClass)
{
	switch (InClass)
	{
		case EShaderRegisterClass::ConstantBuffer:
			return "-fvk-b-shift";
		case EShaderRegisterClass::ShaderResource:
			return "-fvk-t-shift";
		case EShaderRegisterClass::Sampler:
			return "-fvk-s-shift";
		case EShaderRegisterClass::UnorderedAccess:
			return "-fvk-u-shift";
	}
	throw std::invalid_argument("Unsupported shader register class");
}

void AppendIdentity(std::string& OutIdentity, const std::string& InPart)
{
	OutIdentity += std::to_string(InPart.size()) + ":" + InPart;
}

void AppendTargetIdentity(std::string& OutIdentity, const FShaderCompileTarget& InTarget)
{
	AppendIdentity(OutIdentity, std::to_string(static_cast<int>(InTarget.RequestedFormat)));
	AppendIdentity(OutIdentity, std::to_string(static_cast<int>(InTarget.PayloadFormat)));
	const auto Arguments = ShaderCompileTargetArguments(InTarget);
	AppendIdentity(OutIdentity, std::to_string(Arguments.size()));
	for (const auto& Argument : Arguments)
	{
		AppendIdentity(OutIdentity, Argument);
	}
}
} // namespace

FShaderCompileTarget MakeShaderCompileTarget(EShaderStage InStage, EShaderFormat InFormat, bool bInOptimize)
{
	FShaderCompileTarget Result;
	switch (InStage)
	{
		case EShaderStage::Vertex:
			Result.Profile = "vs_6_0";
			break;
		case EShaderStage::Pixel:
			Result.Profile = "ps_6_0";
			break;
		case EShaderStage::Compute:
			Result.Profile = "cs_6_0";
			break;
		default:
			throw std::invalid_argument("Unsupported shader stage");
	}
	if (InFormat != EShaderFormat::Dxil && InFormat != EShaderFormat::Spirv && InFormat != EShaderFormat::Msl)
	{
		throw std::invalid_argument("Unsupported shader target");
	}
	Result.RequestedFormat = InFormat;
	Result.PayloadFormat = InFormat == EShaderFormat::Msl ? EShaderFormat::Spirv : InFormat;
	Result.bOptimize = bInOptimize;
	Result.DiagnosticLabel = Result.PayloadFormat == EShaderFormat::Dxil ? "DXIL" : "SPIR-V";
	return Result;
}

std::vector<std::string> ShaderCompileTargetArguments(const FShaderCompileTarget& InTarget)
{
	std::vector<std::string> Arguments{"-T", InTarget.Profile, "-HV", InTarget.HlslVersion};
	if (InTarget.bStrict)
	{
		Arguments.push_back("-Ges");
	}
	Arguments.push_back(InTarget.bOptimize ? "-O3" : "-Od");
	if (InTarget.PayloadFormat == EShaderFormat::Spirv)
	{
		Arguments.insert(Arguments.end(), {"-spirv", "-fspv-target-env=" + InTarget.SpirvEnvironment});
		if (InTarget.bSpirvReflect)
		{
			Arguments.push_back("-fspv-reflect");
		}
		for (std::uint32_t Space = 0; Space < ShaderRegisterSpaceCount; ++Space)
		{
			for (const auto& Mapping : ShaderRegisterClassMappings)
			{
				const auto Binding = EncodeShaderBinding(Mapping.Class, Space, 0).value();
				Arguments.insert(Arguments.end(),
				                 {RegisterShiftFlag(Mapping.Class), std::to_string(Binding), std::to_string(Space)});
			}
		}
	}
	else if (InTarget.PayloadFormat != EShaderFormat::Dxil)
	{
		throw std::invalid_argument("Unsupported shader payload format");
	}
	return Arguments;
}

std::string ShaderCompileTargetDiagnostics(const FShaderCompileTarget& InTarget)
{
	return "profile=" + InTarget.Profile + "; target=" + InTarget.DiagnosticLabel +
	       "; optimize=" + (InTarget.bOptimize ? "true" : "false");
}

std::string ShaderCompilationPolicyIdentity(const FShaderCompileTarget& InTarget,
                                            const std::optional<FShaderCompileTarget>& InLogicalTarget)
{
	std::string Identity;
	AppendIdentity(Identity, std::to_string(ShaderCompilationPipelineVersion));
	AppendIdentity(Identity, std::to_string(ShaderBindingMappingVersion));
	AppendIdentity(Identity, std::to_string(FShaderReflection{}.Version));
	AppendTargetIdentity(Identity, InTarget);
	AppendIdentity(Identity, InLogicalTarget ? "LogicalDxil" : "NoLogicalTarget");
	if (InLogicalTarget)
	{
		AppendTargetIdentity(Identity, *InLogicalTarget);
	}
	if (InTarget.RequestedFormat == EShaderFormat::Msl)
	{
		AppendIdentity(Identity, "MslConversion");
		AppendIdentity(Identity, std::to_string(ShaderMslMajorVersion));
		AppendIdentity(Identity, std::to_string(ShaderMslMinorVersion));
	}
	return Identity;
}
} // namespace Hyperion::ShadersPrivate
