#include "AssetOperations.h"
#include "Hyperion/IO/Path.h"
#include <bit>
#include <chrono>
#include <limits>
#include <thread>

namespace Hyperion
{
namespace
{
void Require(bool bInValue)
{
	if (!bInValue)
	{
		throw std::runtime_error("Material numeric automation contract failed");
	}
}
} // namespace

void CheckMaterialNumericAutomation(FTaskSystem& InTasks, FAssetService& InAssets, FMemoryFileSystem& InFiles)
{
	FMaterialAsset Material;
	Material.Name = "Numeric fixture";
	FMaterialPass Pass;
	Pass.Vertex = {"Test.hlsl", "VSMain"};
	Pass.Pixel = {"Test.hlsl", "PSMain"};
	Material.Passes.push_back(Pass);
	for (const auto Scalar :
	     {EMaterialScalar::Float, EMaterialScalar::Int, EMaterialScalar::Uint, EMaterialScalar::Bool})
	{
		FMaterialAssetParameter Parameter;
		Parameter.Name = "Value" + std::to_string(static_cast<unsigned>(Scalar));
		Parameter.Type = FMaterialParameterType::Numeric(Scalar);
		Parameter.Default = FMaterialAssetValue{Parameter.Type, {0}};
		Material.Parameters.push_back(Parameter);
	}
	InAssets.Types().Register<FMaterialAsset>();
	const auto Path = InAssets.NormalizePath("automation-numeric.hasset");
	InFiles.WriteAtomic(Path, EncodeAsset(RecordType<FMaterialAsset>(), &Material).Bytes);
	FAssetAutomation Provider(InAssets, InTasks);
	auto Open = Provider.Open({PathToUtf8(Path)});
	std::optional<FAssetDocumentInfo> State;
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (!State && std::chrono::steady_clock::now() < Deadline)
	{
		InTasks.PumpMain();
		State = Open.Poll();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	Require(State.has_value());
	const std::vector<FMaterialNumericEdit> Edits{
	    {"Value3", {0.375}}, {"Value1", {-12}}, {"Value2", {4294967295.0}}, {"Value0", {1}}};
	auto Changed = Provider.SetMaterialNumeric({State->Document, State->Generation}, Edits);
	for (const auto& Edit : Edits)
	{
		Require(Provider.MaterialNumeric({Changed.Document, Changed.Generation}, Edit.Name).Values == Edit.Values);
	}
	for (const auto& Bad : std::vector<FMaterialNumericEdit>{{"Value1", {0.5}},
	                                                         {"Value1", {2147483648.0}},
	                                                         {"Value2", {-1}},
	                                                         {"Value2", {4294967296.0}},
	                                                         {"Value0", {2}},
	                                                         {"Value3", {1e100}},
	                                                         {"Value3", {std::numeric_limits<double>::infinity()}},
	                                                         {"Value3", {1, 2}},
	                                                         {"Missing", {1}}})
	{
		bool bRejected{};
		try
		{
			const FMaterialNumericEdit Prefix =
			    Bad.Name == "Value3" ? FMaterialNumericEdit{"Value1", {-2}} : FMaterialNumericEdit{"Value3", {0.75}};
			Provider.SetMaterialNumeric({Changed.Document, Changed.Generation}, {Prefix, Bad});
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		Require(bRejected && Provider.Info({Changed.Document}).Generation == Changed.Generation);
		Require(Provider.MaterialNumeric({Changed.Document, Changed.Generation}, "Value3").Values ==
		        std::vector<double>{0.375});
		Require(Provider.MaterialNumeric({Changed.Document, Changed.Generation}, "Value1").Values ==
		        std::vector<double>{-12});
	}
	Changed = Provider.Undo({Changed.Document, Changed.Generation});
	Require(!Changed.bDirty && !Changed.bCanUndo);
	Require(!Provider.MaterialNumeric({Changed.Document, Changed.Generation}, "Value3").bOverridden);
	Changed = Provider.Redo({Changed.Document, Changed.Generation});
	Require(Provider.MaterialNumeric({Changed.Document, Changed.Generation}, "Value1").Values ==
	        std::vector<double>{-12});
	FOperationCatalog Catalog;
	RegisterAssetOperations(Catalog, &Provider);
	Require(WriteJson(Catalog.DescribeType("hyperion.materialparametertype")).find("Float") != std::string::npos);
}
} // namespace Hyperion
