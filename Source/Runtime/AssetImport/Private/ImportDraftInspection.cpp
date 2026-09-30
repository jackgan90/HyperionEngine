#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
std::string TextureFormat(ETextureFormat InFormat)
{
	return InFormat == ETextureFormat::Rgba8Unorm    ? "RGBA8"
	       : InFormat == ETextureFormat::Rgba16Float ? "RGBA16F"
	                                                 : "RGBA32F";
}

template<class T> std::vector<T> Page(const std::vector<T>& InValues, const FImportDraftQuery& InQuery)
{
	const auto Begin = std::min<std::size_t>(InQuery.Offset, InValues.size());
	const auto End = std::min<std::size_t>(Begin + InQuery.Limit, InValues.size());
	return {InValues.begin() + Begin, InValues.begin() + End};
}

void DescribeModel(FImportDraftInfo& OutInfo, const FModelAsset& InModel, const FImportDraftQuery& InQuery)
{
	OutInfo.TotalNodes = static_cast<std::uint32_t>(InModel.Nodes.size());
	OutInfo.TotalPrimitives = static_cast<std::uint32_t>(InModel.Primitives.size());
	OutInfo.TotalMaterialSlots = static_cast<std::uint32_t>(InModel.MaterialSlots.size());
	OutInfo.TotalDiagnostics = static_cast<std::uint32_t>(InModel.Diagnostics.size());
	OutInfo.Nodes = Page(InModel.Nodes, InQuery);
	for (std::size_t Index = 0; Index < OutInfo.Nodes.size(); ++Index)
	{
		OutInfo.Nodes[Index].Id = ModelNodeId(InModel, InQuery.Offset + Index);
	}
	OutInfo.MaterialSlots = Page(InModel.MaterialSlots, InQuery);
	for (std::size_t Index = 0; Index < InModel.Primitives.size(); ++Index)
	{
		const auto& Primitive = InModel.Primitives[Index];
		OutInfo.Vertices += Primitive.Positions.size() / 3;
		OutInfo.Triangles += Primitive.Indices.size() / 3;
		if (Index >= InQuery.Offset && OutInfo.Primitives.size() < InQuery.Limit)
		{
			OutInfo.Primitives.push_back({ModelPrimitiveId(InModel, Index), Primitive.Name, Primitive.Material,
			                              Primitive.Positions.size() / 3, Primitive.Indices.size()});
		}
	}
	const auto Bounds = ModelBounds(InModel);
	OutInfo.BoundsMin = Bounds.Minimum;
	OutInfo.BoundsMax = Bounds.Maximum;
	OutInfo.Diagnostics = Page(InModel.Diagnostics, InQuery);
}

void DescribeMaterial(FImportDraftInfo& OutInfo, const FMaterialAsset& InMaterial, const FImportDraftQuery& InQuery)
{
	for (const auto& Pass : Page(InMaterial.Passes, InQuery))
	{
		OutInfo.Details.push_back(Pass.Usage + " | " + Pass.Vertex.Path + " / " + Pass.Pixel.Path);
	}
	for (const auto& Parameter : InMaterial.Parameters)
	{
		if (Parameter.Type.Kind == EMaterialValueKind::Numeric)
		{
			if (OutInfo.TotalMaterial++ >= InQuery.Offset && OutInfo.Material.size() < InQuery.Limit)
			{
				const bool bHasValue =
				    Parameter.Default.has_value() || std::any_of(InMaterial.Values.begin(), InMaterial.Values.end(),
				                                                 [&](const auto& InValue)
				                                                 {
					                                                 return InValue.Name == Parameter.Name;
				                                                 });
				OutInfo.Material.push_back(
				    bHasValue ? DescribeMaterialNumeric(InMaterial, Parameter.Name)
				              : FMaterialNumericInfo{Parameter.Name, Parameter.Type, Parameter.Semantic, false});
			}
		}
	}
}
} // namespace

void DescribeImportRoot(FImportDraftInfo& OutInfo, const FConvertedAsset& InRoot, const FImportDraftQuery& InQuery)
{
	OutInfo.Type = InRoot.Type->Id;
	OutInfo.SourceWidth = InRoot.SourceWidth;
	OutInfo.SourceHeight = InRoot.SourceHeight;
	OutInfo.Sky = InRoot.EffectiveSky;
	const auto Dependencies = CollectAssetDependencies(*InRoot.Type, InRoot.Object.get());
	OutInfo.TotalDependencies = static_cast<std::uint32_t>(Dependencies.size());
	OutInfo.Dependencies = Page(Dependencies, InQuery);
	const auto SetName = [&](const auto& InAsset)
	{
		OutInfo.Name = InAsset.Name;
		OutInfo.bNameEditable = true;
	};
	if (InRoot.Type->CppType == typeid(FModelAsset))
	{
		const auto& Model = *static_cast<const FModelAsset*>(InRoot.Object.get());
		SetName(Model);
		DescribeModel(OutInfo, Model, InQuery);
	}
	else if (InRoot.Type->CppType == typeid(FTextureAsset))
	{
		const auto& Texture = *static_cast<const FTextureAsset*>(InRoot.Object.get());
		SetName(Texture);
		OutInfo.Width = Texture.Mips.front().Width;
		OutInfo.Height = Texture.Mips.front().Height;
		OutInfo.Mips = static_cast<std::uint32_t>(Texture.Mips.size());
		OutInfo.Format = TextureFormat(Texture.Format);
		OutInfo.Encoding = Texture.Encoding == EMaterialTextureEncoding::Srgb ? "sRGB" : "Linear";
		OutInfo.Dimension = Texture.Dimension;
		OutInfo.PixelBytes = 0;
		for (const auto& Mip : Texture.Mips)
		{
			OutInfo.PixelBytes += Mip.Bytes.size();
		}
		OutInfo.Details.push_back((Texture.Dimension == ETextureDimension::Cube ? std::string("Cube") : "Texture2D") +
		                          " | pixel bytes: " + std::to_string(OutInfo.PixelBytes));
	}
	else if (InRoot.Type->CppType == typeid(FMaterialAsset))
	{
		const auto& Material = *static_cast<const FMaterialAsset*>(InRoot.Object.get());
		SetName(Material);
		DescribeMaterial(OutInfo, Material, InQuery);
	}
	else if (InRoot.Type->CppType == typeid(FSkyAsset))
	{
		SetName(*static_cast<const FSkyAsset*>(InRoot.Object.get()));
	}
	else if (InRoot.Type->CppType == typeid(FSceneManifest))
	{
		const auto& Scene = *static_cast<const FSceneManifest*>(InRoot.Object.get());
		OutInfo.Details.push_back("Scene structure is read-only | default camera: " + Scene.DefaultCamera);
		OutInfo.TotalNodes = static_cast<std::uint32_t>(Scene.Nodes.size());
		for (const auto& Node : Page(Scene.Nodes, InQuery))
		{
			OutInfo.Nodes.push_back({Node.Name, Node.Transform, {}, {}, Node.Id});
		}
	}
	OutInfo.TotalProducts = static_cast<std::uint32_t>(InRoot.Products.size());
	for (const auto& Product : Page(InRoot.Products, InQuery))
	{
		FImportProductInfo Info{Product.Key, Product.Type->Id};
		if (Product.Type->CppType == typeid(FTextureAsset))
		{
			const auto& Texture = *static_cast<const FTextureAsset*>(Product.Object.get());
			Info.Width = Texture.Mips.front().Width;
			Info.Height = Texture.Mips.front().Height;
			Info.Mips = static_cast<std::uint32_t>(Texture.Mips.size());
			Info.Format = TextureFormat(Texture.Format);
		}
		OutInfo.Products.push_back(std::move(Info));
	}
}

FImportDraftInfo FAssetImportWorkspace::Draft(const FImportDraftQuery& InRequest) const
{
	if (!InRequest.Limit || InRequest.Limit > ImportDraftMaxPageLimit)
	{
		throw FAssetImportError("invalid_arguments", "Draft property page limit must be 1-64");
	}
	const auto Entry = FindDraft(InRequest.Draft);
	if (Entry->Inspection && Entry->Inspection->Generation == Entry->Generation &&
	    Entry->InspectionQuery.Offset == InRequest.Offset && Entry->InspectionQuery.Limit == InRequest.Limit)
	{
		return *Entry->Inspection;
	}
	auto Result = InspectDraft(*Entry, InRequest);
	Entry->Inspection = Result;
	Entry->InspectionQuery = InRequest;
	return Result;
}

FImportDraftInfo FAssetImportWorkspace::InspectDraft(const FImportDraft& InDraft, const FImportDraftQuery& InQuery)
{
	FImportDraftInfo Result;
	Result.Draft = InDraft.Id;
	Result.Generation = InDraft.Generation;
	Result.Status = InDraft.Status;
	Result.Error = InDraft.Error;
	Result.Properties = InDraft.History[InDraft.Cursor];
	Result.bDirty = ImportPropertiesKey(Result.Properties) != InDraft.SavedKey;
	Result.bCanUndo = InDraft.Cursor > 0;
	Result.bCanRedo = InDraft.Cursor + 1 < InDraft.History.size();
	if (InDraft.Prepared)
	{
		DescribeImportRoot(Result, InDraft.Edited, InQuery);
	}
	return Result;
}

FImportDraftInfo FAssetImportWorkspace::CommitDraft(const std::shared_ptr<FImportDraft>& InDraft,
                                                    FImportDraft InCandidate)
{
	const auto Slot = std::find(Drafts.begin(), Drafts.end(), InDraft);
	if (Slot == Drafts.end())
	{
		throw std::logic_error("Cannot commit a discarded import draft");
	}
	++InCandidate.Generation;
	InCandidate.InspectionQuery = {InCandidate.Id};
	auto Result = InspectDraft(InCandidate, InCandidate.InspectionQuery);
	InCandidate.Inspection = Result;
	// Derived metadata can reject otherwise valid local values. Commit only after it and the reply exist.
	auto Prepared = std::make_shared<FImportDraft>(std::move(InCandidate));
	*Slot = std::move(Prepared);
	return Result;
}

template<> const FRecordDescriptor& RecordType<FImportDraftList>()
{
	static const auto Type =
	    MakeRecord<FImportDraftList>("asset.import.draft-list", {Member("drafts", &FImportDraftList::Drafts)});
	return Type;
}
} // namespace Hyperion
