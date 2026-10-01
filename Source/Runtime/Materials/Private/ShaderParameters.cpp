#include "Hyperion/Materials/ShaderParameters.h"
#include "ShaderContracts.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::string ShaderType(const FStandardMaterialBlockMember& InMember)
{
	const std::string Scalar = InMember.Scalar == EMaterialScalar::Float  ? "float"
	                           : InMember.Scalar == EMaterialScalar::Uint ? "uint"
	                           : InMember.Scalar == EMaterialScalar::Int  ? "int"
	                                                                      : "bool";
	if (InMember.Rows > 1)
	{
		return "column_major " + Scalar + std::to_string(InMember.Rows) + "x" + std::to_string(InMember.Columns);
	}
	return Scalar + (InMember.Columns > 1 ? std::to_string(InMember.Columns) : "");
}

void AppendDeclaration(std::string& OutSource, const std::string& InName, const FStandardMaterialBlock& InBlock)
{
	const std::string& Name = InName;
	const FStandardMaterialBlock& Block = InBlock;
	if (Block.Instance.empty())
	{
		OutSource += "#define HYP_UNIFORM_" + Name + "(...) cbuffer " + Name + " : register(__VA_ARGS__) { ";
		for (const auto& Member : Block.Members)
		{
			const auto Component = Member.Offset % 16 / 4;
			OutSource += ShaderType(Member) + " " + Member.Name + " : packoffset(c" +
			             std::to_string(Member.Offset / 16) + "." + "xyzw"[Component] + "); ";
		}
		OutSource += "};\n";
	}
	OutSource += "#define HYP_RECORD_" + Name + " ";
	unsigned Offset = 0;
	for (const auto& Member : Block.Members)
	{
		while (Offset < Member.Offset)
		{
			OutSource += "uint HyperionPadding" + std::to_string(Offset) + "; ";
			Offset += 4;
		}
		const auto Field = Member.Name.substr(Member.Name.find_last_of('.') + 1);
		OutSource += ShaderType(Member) + " " + Field + "; ";
		Offset += Member.Columns * Member.Rows * 4;
	}
	while (Offset < Block.Size)
	{
		OutSource += "uint HyperionPadding" + std::to_string(Offset) + "; ";
		Offset += 4;
	}
	OutSource += "\n";
	if (!Block.Instance.empty())
	{
		OutSource += "struct F" + Name + "Uniform { HYP_RECORD_" + Name + " };\n";
		OutSource += "#define HYP_UNIFORM_" + Name + "(...) cbuffer " + Name + " : register(__VA_ARGS__) { F" + Name +
		             "Uniform " + Block.Instance + "; };\n";
	}
}
} // namespace

std::string GenerateEngineShaderDeclarations()
{
	std::string Source = "#ifndef HYP_ENGINE_UNIFORMS_GENERATED\n#define HYP_ENGINE_UNIFORMS_GENERATED\n";
	Source += "// contract version " + std::to_string(GetEngineSemanticContractVersion()) + "\n";
	for (const auto& Uniform : GetEngineShaderContracts()->Uniforms)
	{
		AppendDeclaration(Source, Uniform.first, Uniform.second);
	}
	return Source + "#endif\n";
}

std::string GenerateShaderDeclarations(const FShaderParameterContractSet& InContracts)
{
	std::string Guard = "HYP_GENERATED_" + InContracts.IncludeName;
	for (char& Character : Guard)
	{
		if (Character == '.' || Character == '/')
		{
			Character = '_';
		}
	}
	std::string Source = "#ifndef " + Guard + "\n#define " + Guard + "\n// contract version " +
	                     std::to_string(InContracts.Version) + "\n";
	for (const auto& Uniform : InContracts.Uniforms)
	{
		AppendDeclaration(Source, Uniform.first, Uniform.second);
	}
	return Source + "#endif\n";
}

std::vector<std::pair<std::string, std::string>> GenerateShaderIncludes(
    std::span<const std::shared_ptr<const FShaderParameterContractSet>> InContracts)
{
	ValidateShaderContracts(InContracts);
	std::vector<std::pair<std::string, std::string>> Result{
	    {"HyperionUniforms.generated.hlsli", GenerateEngineShaderDeclarations()}};
	for (const auto Sets :
	     {std::span<const std::shared_ptr<const FShaderParameterContractSet>>(GetStandardShaderContracts()),
	      InContracts})
	{
		for (const auto& Set : Sets)
		{
			const auto Source = GenerateShaderDeclarations(*Set);
			const auto Existing = std::find_if(Result.begin(), Result.end(),
			                                   [&](const auto& InInclude)
			                                   {
				                                   return InInclude.first == Set->IncludeName;
			                                   });
			if (Existing == Result.end())
			{
				Result.push_back({Set->IncludeName, Source});
			}
			else if (Existing->second != Source)
			{
				throw std::invalid_argument("Conflicting shader contract include: " + Set->IncludeName);
			}
		}
	}
	return Result;
}
} // namespace Hyperion
