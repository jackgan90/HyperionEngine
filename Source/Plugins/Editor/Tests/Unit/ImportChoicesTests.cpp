#include "../../Private/ImportChoices.h"
#include "Hyperion/AssetImport/ImageImport.h"
#include <array>
#include <iostream>
#include <source_location>
#include <stdexcept>

namespace
{
using namespace Hyperion;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Import choice check failed at " + std::to_string(InLocation.line()));
	}
}

void CheckSettings(const std::vector<FImportTypeChoice>& InChoices, const std::string& InType,
                   const std::string& InSource, const std::string& InExpectedType)
{
	FImportRequest Request;
	Request.Type = InType;
	Request.Source = InSource;
	Request.Name = "Imported name";
	Request.bScene = true;
	const auto* Choice = ResolveImportChoice(InChoices, InType, InSource);
	Check(Choice && Choice->Type == InExpectedType);
	const auto Snapshot = ImportSettingsSnapshot(Request, SelectImportSource(DefaultAssetImporters(), InSource, InType),
	                                             EMaterialTextureEncoding::Srgb, {8, 4, 8});
	Check(Snapshot.Type == InType);
	Check(Snapshot.bScene == (InExpectedType == RecordType<FModelAsset>().Id));
	Check(Snapshot.TextureEncoding.has_value() == (InExpectedType == RecordType<FTextureAsset>().Id));
	Check(Snapshot.Sky.has_value() == (InExpectedType == RecordType<FSkyAsset>().Id));
	Check(Snapshot.Name.empty() == (InExpectedType == RecordType<FSkyAsset>().Id));
}

void CheckReordering()
{
	const auto Original = FAssetImportWorkspace::Capabilities();
	Check(Original.Formats.size() == 3);
	std::array<std::size_t, 3> Order{0, 1, 2};
	do
	{
		FImportCapabilities Reordered;
		for (const auto Index : Order)
		{
			Reordered.Formats.push_back(Original.Formats[Index]);
		}
		const auto Choices = ImportTypeChoices(Reordered);
		Check(Choices.size() == Original.Formats.size());
		for (const auto& Choice : Choices)
		{
			const auto* Selected = ResolveImportChoice(Choices, Choice.Type, "");
			Check(Selected && Selected->Type == Choice.Type && Selected->Label == Choice.Label);
			const auto Filters = ImportSourceFilters(Choices, Choice.Type);
			Check(Filters.size() == 1 && Filters.front().Name == Choice.Label);
			for (const auto& Extension : Choice.Extensions)
			{
				Check(Filters.front().Pattern.find("*" + Extension) != std::string::npos);
				CheckSettings(Choices, Choice.Type, "asset" + Extension, Choice.Type);
				CheckSettings(Choices, "", "asset" + Extension, Choice.Type);
			}
		}
		CheckSettings(Choices, "", "IMAGE.PNG", RecordType<FTextureAsset>().Id);
		const auto AllFilters = ImportSourceFilters(Choices, "");
		Check(AllFilters.size() == 4);
		for (const auto* Extension : {".hasset", ".json", ".tga", ".bmp"})
		{
			Check(!ResolveImportChoice(Choices, "", std::string("asset") + Extension));
			Check(AllFilters.front().Pattern.find(Extension) == std::string::npos);
		}
		FImportRequest Mismatch;
		Mismatch.Type = RecordType<FSkyAsset>().Id;
		Mismatch.Source = "Color.png";
		const auto Snapshot = ImportSettingsSnapshot(
		    Mismatch, SelectImportSource(DefaultAssetImporters(), Mismatch.Source, Mismatch.Type),
		    EMaterialTextureEncoding::Srgb, {8, 4, 8});
		Check(Snapshot.Type == Mismatch.Type && !Snapshot.Sky && !Snapshot.TextureEncoding && !Snapshot.bScene);
		Check(ImportSourceFilters(Choices, Mismatch.Type).front().Pattern == "*.hdr;*.exr");
	} while (std::next_permutation(Order.begin(), Order.end()));
}

void CheckSelectedDescriptor()
{
	auto Importer = MakeImageImporter();
	Importer.Extensions = {".TEST_TEXTURE"};
	auto FixedEncoding = Importer;
	FixedEncoding.Id = "test.fixed-encoding";
	FixedEncoding.Extensions = {".FIXED_TEXTURE"};
	FixedEncoding.Settings.bTextureEncoding = false;
	const std::vector<FAssetImporter> Descriptors{NormalizeAssetImporter(std::move(Importer)),
	                                              NormalizeAssetImporter(std::move(FixedEncoding))};
	const auto Choices = ImportTypeChoices(ProjectImportCapabilities(Descriptors));
	Check(Choices.size() == 1 && Choices[0].SupportsSource("color.TEST_TEXTURE"));
	Check(ImportSourceFilters(Choices, "").front().Pattern == "*.test_texture;*.fixed_texture");
	FImportRequest Request;
	Request.Source = "color.TEST_TEXTURE";
	const auto Snapshot = ImportSettingsSnapshot(Request, SelectImportSource(Descriptors, Request.Source),
	                                             EMaterialTextureEncoding::Linear, {});
	Check(Snapshot.TextureEncoding == EMaterialTextureEncoding::Linear && !Snapshot.Sky && !Snapshot.bScene);
	ValidateImportSettings({Snapshot.TextureEncoding, Snapshot.Sky}, &Descriptors[0]);
	Request.Source = "color.FIXED_TEXTURE";
	const auto FixedSnapshot = ImportSettingsSnapshot(Request, SelectImportSource(Descriptors, Request.Source),
	                                                  EMaterialTextureEncoding::Srgb, {});
	Check(!FixedSnapshot.TextureEncoding && !FixedSnapshot.Sky);
	ValidateImportSettings({FixedSnapshot.TextureEncoding, FixedSnapshot.Sky}, &Descriptors[1]);
}
} // namespace

int main()
{
	try
	{
		CheckReordering();
		CheckSelectedDescriptor();
		std::cout << "Import choice capability ordering, filters and settings passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
