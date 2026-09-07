#include "Hyperion/Materials/MaterialParameters.h"
#include "MaterialIdentity.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>

namespace Hyperion
{
FMaterialParameterType FMaterialParameterType::Numeric(EMaterialScalar InScalar, std::uint32_t InColumns,
                                                       std::uint32_t InRows)
{
	FMaterialParameterType Result;
	Result.Scalar = InScalar;
	Result.Rows = InRows;
	Result.Columns = InColumns;
	Result.Validate();
	return Result;
}

FMaterialParameterType FMaterialParameterType::Resource(EMaterialValueKind InKind)
{
	FMaterialParameterType Result;
	Result.Kind = InKind;
	Result.Validate();
	return Result;
}

FMaterialParameterType FMaterialParameterType::Array(FMaterialParameterType InElement, std::uint32_t InCount)
{
	FMaterialParameterType Result;
	Result.Kind = EMaterialValueKind::Array;
	Result.ArrayCount = InCount;
	Result.Members.push_back(std::move(InElement));
	Result.Validate();
	return Result;
}

void FMaterialParameterType::Validate(std::uint32_t InDepth) const
{
	if (InDepth > 32 || Kind > EMaterialValueKind::Sampler || Scalar > EMaterialScalar::Float || Rows < 1 || Rows > 4 ||
	    Columns < 1 || Columns > 4)
	{
		throw std::invalid_argument("Unsupported material parameter type or nesting depth");
	}
	if (Kind == EMaterialValueKind::Numeric && Rows > 1 && (Scalar != EMaterialScalar::Float || Columns < 2))
	{
		throw std::invalid_argument("Material matrices must have float32 components and 2-4 rows/columns");
	}
	if (Kind == EMaterialValueKind::Structure)
	{
		std::set<std::string> DeclaredNames;
		if (Members.empty() || Members.size() != MemberNames.size())
		{
			throw std::invalid_argument("Material structures require named members");
		}
		for (const std::string& Name : MemberNames)
		{
			if (Name.empty() || !DeclaredNames.insert(Name).second || Name.find_first_of(".:[]") != std::string::npos)
			{
				throw std::invalid_argument("Invalid or duplicate material structure member name");
			}
		}
	}
	else if (Kind == EMaterialValueKind::Array)
	{
		if (Members.size() != 1 || ArrayCount == 0 || ArrayCount > 65536 || !MemberNames.empty())
		{
			throw std::invalid_argument("Material arrays require one element type and a bounded fixed count");
		}
	}
	else if (!Members.empty() || !MemberNames.empty())
	{
		throw std::invalid_argument("Unexpected members on a nonaggregate material type");
	}
	if ((Kind != EMaterialValueKind::Array && ArrayCount != 0) ||
	    (Kind != EMaterialValueKind::Numeric && (Rows != 1 || Columns != 1 || Scalar != EMaterialScalar::Float)))
	{
		throw std::invalid_argument("Noncanonical material type metadata");
	}
	for (const FMaterialParameterType& Member : Members)
	{
		Member.Validate(InDepth + 1);
	}
}

FMaterialValue FMaterialValue::Floats(std::span<const float> InValues, std::uint32_t InRows)
{
	if (InRows == 0 || InValues.size() % InRows != 0 || InValues.size() > 16)
	{
		throw std::invalid_argument("Invalid material float shape");
	}
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float,
	                                              static_cast<std::uint32_t>(InValues.size() / InRows), InRows);
	for (float Value : InValues)
	{
		Result.Words.push_back(std::bit_cast<std::uint32_t>(Value));
	}
	Result.Validate();
	return Result;
}

FMaterialValue FMaterialValue::Float(float InValue)
{
	return Floats({&InValue, 1});
}

FMaterialValue FMaterialValue::Float(FVec2 InValue)
{
	const std::array Values{InValue.X, InValue.Y};
	return Floats(Values);
}

FMaterialValue FMaterialValue::Float(FVec3 InValue)
{
	const std::array Values{InValue.X, InValue.Y, InValue.Z};
	return Floats(Values);
}

FMaterialValue FMaterialValue::Float(FVec4 InValue)
{
	const std::array Values{InValue.X, InValue.Y, InValue.Z, InValue.W};
	return Floats(Values);
}

FMaterialValue FMaterialValue::Matrix(const FMat4& InValue)
{
	std::array<float, 16> Values;
	for (std::size_t Row = 0; Row < 4; ++Row)
	{
		for (std::size_t Column = 0; Column < 4; ++Column)
		{
			Values[Row * 4 + Column] = InValue.Values[Column * 4 + Row];
		}
	}
	return Floats(Values, 4);
}

FMaterialValue FMaterialValue::Int(std::int32_t InValue)
{
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Numeric(EMaterialScalar::Int);
	Result.Words = {std::bit_cast<std::uint32_t>(InValue)};
	return Result;
}

FMaterialValue FMaterialValue::Uint(std::uint32_t InValue)
{
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Numeric(EMaterialScalar::Uint);
	Result.Words = {InValue};
	return Result;
}

FMaterialValue FMaterialValue::Bool(bool bInValue)
{
	FMaterialValue Result = Uint(bInValue ? 1U : 0U);
	Result.Type.Scalar = EMaterialScalar::Bool;
	return Result;
}

FMaterialValue FMaterialValue::FromTexture(std::shared_ptr<const FMaterialTextureSource> InTexture)
{
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Result.Texture = std::move(InTexture);
	Result.Validate();
	return Result;
}

FMaterialValue FMaterialValue::FromBuffer(FMaterialBufferView InBuffer)
{
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Resource(EMaterialValueKind::ReadBuffer);
	Result.Buffer = std::move(InBuffer);
	Result.Validate();
	return Result;
}

FMaterialValue FMaterialValue::FromSampler(FMaterialSampler InSampler)
{
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Resource(EMaterialValueKind::Sampler);
	Result.Sampler = InSampler;
	Result.Validate();
	return Result;
}

FMaterialValue FMaterialValue::Array(std::vector<FMaterialValue> InElements)
{
	if (InElements.empty() || InElements.size() > 65536)
	{
		throw std::invalid_argument("Invalid material array size");
	}
	FMaterialValue Result;
	Result.Type = FMaterialParameterType::Array(InElements.front().Type, static_cast<std::uint32_t>(InElements.size()));
	Result.Elements = std::move(InElements);
	Result.Validate();
	return Result;
}

void FMaterialValue::Validate() const
{
	Type.Validate();
	if ((Type.Kind != EMaterialValueKind::Numeric && !Words.empty()) ||
	    (Type.Kind != EMaterialValueKind::Array && Type.Kind != EMaterialValueKind::Structure && !Elements.empty()) ||
	    (Type.Kind != EMaterialValueKind::Texture2D && Texture) ||
	    (Type.Kind != EMaterialValueKind::ReadBuffer && Buffer.Source))
	{
		throw std::invalid_argument("Material value contains data of another type");
	}
	if (Type.Kind == EMaterialValueKind::Numeric)
	{
		if (Words.size() != Type.Rows * Type.Columns)
		{
			throw std::invalid_argument("Material numeric shape does not match its value");
		}
		for (std::uint32_t Word : Words)
		{
			if ((Type.Scalar == EMaterialScalar::Bool && Word > 1) ||
			    (Type.Scalar == EMaterialScalar::Float && !std::isfinite(std::bit_cast<float>(Word))))
			{
				throw std::invalid_argument("Material value requires finite floats or canonical booleans");
			}
		}
	}
	else if (Type.Kind == EMaterialValueKind::Array || Type.Kind == EMaterialValueKind::Structure)
	{
		const std::size_t Count = Type.Kind == EMaterialValueKind::Array ? Type.ArrayCount : Type.Members.size();
		if (Elements.size() != Count)
		{
			throw std::invalid_argument("Material aggregate value count mismatch");
		}
		for (std::size_t Index = 0; Index < Count; ++Index)
		{
			const FMaterialParameterType& Expected = Type.Members[Type.Kind == EMaterialValueKind::Array ? 0 : Index];
			if (Elements[Index].Type != Expected)
			{
				throw std::invalid_argument("Material aggregate member type mismatch");
			}
			Elements[Index].Validate();
		}
	}
	else if (Type.Kind == EMaterialValueKind::Texture2D && !Texture)
	{
		throw std::invalid_argument("Null material texture");
	}
	else if (Type.Kind == EMaterialValueKind::ReadBuffer)
	{
		Buffer.Validate();
	}
	else if (Type.Kind == EMaterialValueKind::Sampler)
	{
		Sampler.Validate();
	}
}

FMaterialParameterSchema::FMaterialParameterSchema(std::vector<FMaterialParameterDeclaration> InParameters,
                                                   std::uint64_t InVersion, bool bInPrepared)
    : Identity(MaterialsPrivate::NextIdentity()), Version(InVersion), bPrepared(bInPrepared),
      Parameters(std::move(InParameters))
{
	std::set<std::string> DeclaredNames;
	std::set<std::string> Targets;
	if (Version == 0)
	{
		throw std::invalid_argument("Material schema requires a nonzero version");
	}
	for (const FMaterialParameterDeclaration& Parameter : Parameters)
	{
		Parameter.Type.Validate();
		if (Parameter.Name.empty() || !DeclaredNames.insert(Parameter.Name).second ||
		    Parameter.Source > EMaterialParameterSource::Semantic ||
		    Parameter.OverridePolicy > EMaterialOverridePolicy::AllowOverride ||
		    (Parameter.OverrideScopes & ~255U) != 0 ||
		    (Parameter.Source == EMaterialParameterSource::Semantic && Parameter.Semantic.empty()))
		{
			throw std::invalid_argument("Invalid material parameter declaration: " + Parameter.Name);
		}
		for (const std::string& Target : Parameter.Targets)
		{
			if (Target.empty() || !Targets.insert(Target).second)
			{
				throw std::invalid_argument("Duplicate material shader target: " + Target);
			}
		}
		if (Parameter.Default)
		{
			Parameter.Default->Validate();
			if (Parameter.Default->Type != Parameter.Type)
			{
				throw std::invalid_argument("Material default type mismatch: " + Parameter.Name);
			}
		}
	}
	BuildLookup();
}

std::uint64_t FMaterialParameterSchema::GetIdentity() const
{
	return Identity;
}

std::uint64_t FMaterialParameterSchema::GetVersion() const
{
	return Version;
}

bool FMaterialParameterSchema::IsPrepared() const
{
	return bPrepared;
}

const std::vector<FMaterialParameterDeclaration>& FMaterialParameterSchema::GetParameters() const
{
	return Parameters;
}

void FMaterialParameterSchema::BuildLookup()
{
	for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
	{
		const auto& Parameter = Parameters[Index];
		Names.emplace(Parameter.Name, Index);
		const auto Add = [&](const std::string& InName)
		{
			for (const auto& Name : {InName, InName.substr(InName.find_last_of(".:") + 1)})
			{
				auto& Entries = Aliases[Name];
				if (std::find(Entries.begin(), Entries.end(), Index) == Entries.end())
				{
					Entries.push_back(Index);
				}
			}
		};
		Add(Parameter.Name);
		for (const auto& Target : Parameter.Targets)
		{
			Add(Target);
		}
		if (!Parameter.Semantic.empty())
		{
			SemanticNames[Parameter.Semantic].push_back(Index);
		}
	}
}

FMaterialParameterHandle FMaterialParameterSchema::Find(std::string_view InName) const
{
	if (const auto It = Names.find(InName); It != Names.end())
	{
		return {Identity, Version, It->second};
	}
	const auto It = Aliases.find(InName);
	if (It == Aliases.end())
	{
		throw std::invalid_argument(std::string(bPrepared ? "UnknownParameter: " : "InterfaceNotReady: ") +
		                            std::string(InName));
	}
	if (It->second.size() != 1)
	{
		std::string Alternatives;
		for (const auto Index : It->second)
		{
			Alternatives += Parameters[Index].Name + " ";
		}
		throw std::invalid_argument("Ambiguous material parameter " + std::string(InName) + ": " + Alternatives);
	}
	return {Identity, Version, It->second.front()};
}

FMaterialParameterHandle FMaterialParameterSchema::FindSemantic(std::string_view InSemantic) const
{
	const auto It = SemanticNames.find(InSemantic);
	if (It == SemanticNames.end())
	{
		throw std::invalid_argument("Unknown material semantic: " + std::string(InSemantic));
	}
	if (It->second.size() != 1)
	{
		throw std::invalid_argument("Ambiguous material semantic: " + std::string(InSemantic));
	}
	return {Identity, Version, It->second.front()};
}

const FMaterialParameterDeclaration& FMaterialParameterSchema::Get(FMaterialParameterHandle InHandle) const
{
	if (InHandle.SchemaIdentity != Identity || InHandle.SchemaVersion != Version || InHandle.Index >= Parameters.size())
	{
		throw std::invalid_argument("Stale material parameter handle");
	}
	return Parameters[InHandle.Index];
}

void ValidateMaterialOverride(const FMaterialParameterDeclaration& InParameter, const FMaterialValue& InValue,
                              EMaterialScope InScope)
{
	InValue.Validate();
	if (InParameter.Type != InValue.Type)
	{
		throw std::invalid_argument("Material parameter type mismatch: " + InParameter.Name);
	}
	if (InScope >= EMaterialScope::Count || InParameter.OverridePolicy == EMaterialOverridePolicy::Locked ||
	    (InParameter.OverrideScopes & MaterialScopeBit(InScope)) == 0)
	{
		throw std::invalid_argument("Material parameter is locked in this override scope: " + InParameter.Name);
	}
}
} // namespace Hyperion
