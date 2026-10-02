#pragma once
#include "Hyperion/Shaders/ShaderCompiler.h"
#include <optional>
#include <string>
#include <vector>

namespace Hyperion::ShadersPrivate
{
inline constexpr std::uint32_t ShaderCompilationPipelineVersion = 13;
inline constexpr std::uint32_t ShaderMslMajorVersion = 2;
inline constexpr std::uint32_t ShaderMslMinorVersion = 0;

struct FShaderCompileTarget
{
	EShaderFormat RequestedFormat = EShaderFormat::Dxil;
	EShaderFormat PayloadFormat = EShaderFormat::Dxil;
	std::string Profile;
	std::string HlslVersion = "2021";
	bool bStrict = true;
	bool bOptimize = true;
	std::string SpirvEnvironment = "vulkan1.1";
	bool bSpirvReflect = true;
	std::string DiagnosticLabel;
};

FShaderCompileTarget MakeShaderCompileTarget(EShaderStage InStage, EShaderFormat InFormat, bool bInOptimize);
std::vector<std::string> ShaderCompileTargetArguments(const FShaderCompileTarget& InTarget);
std::string ShaderCompileTargetDiagnostics(const FShaderCompileTarget& InTarget);
std::string ShaderCompilationPolicyIdentity(const FShaderCompileTarget& InTarget,
                                            const std::optional<FShaderCompileTarget>& InLogicalTarget);
} // namespace Hyperion::ShadersPrivate
