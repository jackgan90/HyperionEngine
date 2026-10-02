#include "ShaderCompileTarget.h"
#include <array>
#include <limits>
#include <stdexcept>

namespace Hyperion::ShadersPrivate
{
namespace
{
void Check(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void CheckRegisterProtocol()
{
	Check(ShaderBindingMappingVersion == 2 && ShaderRegisterSpaceCount == 4 && ShaderRegistersPerKind == 1000,
	      "Register protocol compatibility constants");
	const std::array<std::pair<EShaderRegisterClass, std::uint32_t>, 4> Expected{
	    {{EShaderRegisterClass::ConstantBuffer, 0},
	     {EShaderRegisterClass::ShaderResource, 1000},
	     {EShaderRegisterClass::Sampler, 2000},
	     {EShaderRegisterClass::UnorderedAccess, 3000}}};
	for (const auto& [Class, Offset] : Expected)
	{
		for (std::uint32_t Space = 0; Space != 4; ++Space)
		{
			for (const auto Register : {0U, 7U, 999U})
			{
				Check(EncodeShaderBinding(Class, Space, Register) == Offset + Register,
				      "Fixed b/t/s/u encoding in every space");
				Check(DecodeShaderBinding(Space, Offset + Register) == FShaderRegisterAddress{Class, Space, Register},
				      "Fixed b/t/s/u decoding in every space");
			}
			Check(EncodeShaderBinding(Class, Space, 0, 1000) == Offset, "Full register class array remains valid");
			Check(EncodeShaderBinding(Class, Space, 998, 2) == Offset + 998, "Array may end exactly at class boundary");
			Check(DecodeShaderBinding(Space, Offset + 998, 2) == FShaderRegisterAddress{Class, Space, 998},
			      "Array decoding preserves class boundary");
			Check(!EncodeShaderBinding(Class, Space, 999, 2) && !DecodeShaderBinding(Space, Offset + 999, 2),
			      "Array must not cross into the next register class");
			Check(!EncodeShaderBinding(Class, Space, 0, 0) && !DecodeShaderBinding(Space, Offset, 0),
			      "Zero or unbounded resource count is rejected");
		}
	}
	const auto Maximum = std::numeric_limits<std::uint32_t>::max();
	Check(!EncodeShaderBinding(EShaderRegisterClass(99), 0, 0), "Unknown register class is rejected");
	Check(!EncodeShaderBinding(EShaderRegisterClass::ConstantBuffer, 4, 0), "Space four is rejected");
	Check(!EncodeShaderBinding(EShaderRegisterClass::ConstantBuffer, 0, 1000), "Register one thousand is rejected");
	Check(!EncodeShaderBinding(EShaderRegisterClass::ConstantBuffer, 0, Maximum), "Register overflow is rejected");
	Check(!EncodeShaderBinding(EShaderRegisterClass::ConstantBuffer, 0, 1, Maximum), "Array overflow is rejected");
	Check(!DecodeShaderBinding(0, 4000) && !DecodeShaderBinding(0, Maximum), "Out of range target binding is rejected");
	Check(!DecodeShaderBinding(4, 7) && !DecodeShaderBinding(Maximum, 7), "Decoded space range is checked");
	Check(!DecodeShaderBinding(0, 7, Maximum), "Decoded array overflow is rejected");
}

void CheckDefaultTargetArguments()
{
	const std::array<std::pair<EShaderStage, const char*>, 3> Profiles{
	    {{EShaderStage::Vertex, "vs_6_0"}, {EShaderStage::Pixel, "ps_6_0"}, {EShaderStage::Compute, "cs_6_0"}}};
	const std::array<std::pair<const char*, const char*>, 4> Shifts{
	    {{"-fvk-b-shift", "0"}, {"-fvk-t-shift", "1000"}, {"-fvk-s-shift", "2000"}, {"-fvk-u-shift", "3000"}}};
	for (const auto& [Stage, Profile] : Profiles)
	{
		for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
		{
			const auto Target = MakeShaderCompileTarget(Stage, Format, true);
			std::vector<std::string> Expected{"-T", Profile, "-HV", "2021", "-Ges", "-O3"};
			if (Format != EShaderFormat::Dxil)
			{
				Expected.insert(Expected.end(), {"-spirv", "-fspv-target-env=vulkan1.1", "-fspv-reflect"});
				for (std::uint32_t Space = 0; Space != 4; ++Space)
				{
					for (const auto& [Flag, Offset] : Shifts)
					{
						Expected.insert(Expected.end(), {Flag, Offset, std::to_string(Space)});
					}
				}
			}
			Check(ShaderCompileTargetArguments(Target) == Expected, "Production target arguments preserve defaults");
			Check(Target.RequestedFormat == Format, "Requested format remains distinct from payload format");
			Check(Target.PayloadFormat == (Format == EShaderFormat::Dxil ? EShaderFormat::Dxil : EShaderFormat::Spirv),
			      "MSL compilation uses a SPIR-V payload");
			Check(ShaderCompileTargetDiagnostics(Target).find(std::string("profile=") + Profile) != std::string::npos,
			      "Diagnostics use the selected profile");
		}
	}
}

void CheckPolicyChange(const FShaderCompileTarget& InOriginal, const FShaderCompileTarget& InChanged,
                       const std::optional<FShaderCompileTarget>& InLogical)
{
	Check(ShaderCompileTargetArguments(InChanged) != ShaderCompileTargetArguments(InOriginal),
	      "Policy change must reach production compilation arguments");
	Check(ShaderCompilationPolicyIdentity(InChanged, InLogical) !=
	          ShaderCompilationPolicyIdentity(InOriginal, InLogical),
	      "Policy change must reach production cache identity");
}

void CheckTargetPolicyChanges()
{
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		const auto Original = MakeShaderCompileTarget(EShaderStage::Pixel, Format, true);
		std::optional<FShaderCompileTarget> Logical;
		if (Format != EShaderFormat::Dxil)
		{
			Logical = MakeShaderCompileTarget(EShaderStage::Pixel, EShaderFormat::Dxil, true);
		}
		auto Changed = Original;
		Changed.Profile = "ps_6_1";
		CheckPolicyChange(Original, Changed, Logical);
		Check(ShaderCompileTargetDiagnostics(Changed).find("profile=ps_6_1") != std::string::npos,
		      "Changed profile reaches production diagnostics");
		Changed = Original;
		Changed.HlslVersion = "2018";
		CheckPolicyChange(Original, Changed, Logical);
		Changed = Original;
		Changed.bStrict = false;
		CheckPolicyChange(Original, Changed, Logical);
		Changed = Original;
		Changed.bOptimize = false;
		CheckPolicyChange(Original, Changed, Logical);
		Changed = Original;
		Changed.DiagnosticLabel = "A presentation-only label";
		Check(ShaderCompileTargetArguments(Changed) == ShaderCompileTargetArguments(Original) &&
		          ShaderCompilationPolicyIdentity(Changed, Logical) ==
		              ShaderCompilationPolicyIdentity(Original, Logical),
		      "Display labels do not invalidate compiled artifacts");
		Changed = Original;
		Changed.SpirvEnvironment = "vulkan1.2";
		if (Format == EShaderFormat::Dxil)
		{
			Changed.bSpirvReflect = false;
			Check(ShaderCompileTargetArguments(Changed) == ShaderCompileTargetArguments(Original) &&
			          ShaderCompilationPolicyIdentity(Changed, Logical) ==
			              ShaderCompilationPolicyIdentity(Original, Logical),
			      "Inapplicable SPIR-V settings do not invalidate DXIL");
		}
		else
		{
			CheckPolicyChange(Original, Changed, Logical);
			Changed = Original;
			Changed.bSpirvReflect = false;
			CheckPolicyChange(Original, Changed, Logical);
			auto ChangedLogical = Logical;
			ChangedLogical->Profile = "ps_6_1";
			Check(ShaderCompilationPolicyIdentity(Original, ChangedLogical) !=
			          ShaderCompilationPolicyIdentity(Original, Logical),
			      "Logical DXIL profile participates in non-DXIL artifact identity");
			ChangedLogical = Logical;
			ChangedLogical->HlslVersion = "2018";
			Check(ShaderCompilationPolicyIdentity(Original, ChangedLogical) !=
			          ShaderCompilationPolicyIdentity(Original, Logical),
			      "Logical DXIL language participates in non-DXIL artifact identity");
		}
		const auto Restored = MakeShaderCompileTarget(EShaderStage::Pixel, Format, true);
		Check(ShaderCompilationPolicyIdentity(Restored, Logical) == ShaderCompilationPolicyIdentity(Original, Logical),
		      "Restored target policy has the same production identity");
	}
}
} // namespace

void CheckShaderCompilationContracts()
{
	CheckRegisterProtocol();
	CheckDefaultTargetArguments();
	CheckTargetPolicyChanges();
	const auto Spirv = MakeShaderCompileTarget(EShaderStage::Pixel, EShaderFormat::Spirv, true);
	const auto Msl = MakeShaderCompileTarget(EShaderStage::Pixel, EShaderFormat::Msl, true);
	const auto Logical = MakeShaderCompileTarget(EShaderStage::Pixel, EShaderFormat::Dxil, true);
	Check(ShaderCompileTargetArguments(Spirv) == ShaderCompileTargetArguments(Msl) &&
	          ShaderCompilationPolicyIdentity(Spirv, Logical) != ShaderCompilationPolicyIdentity(Msl, Logical),
	      "SPIR-V and MSL retain separate requested-format cache entries");
}
} // namespace Hyperion::ShadersPrivate
