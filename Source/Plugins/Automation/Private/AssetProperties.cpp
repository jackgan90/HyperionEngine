#include "Hyperion/AssetEditing/AssetProperties.h"
#include "AssetOperations.h"

namespace Hyperion
{
FArchiveNode FAssetAutomation::ReadField(const std::string& InDocument, std::string_view InType,
                                         std::string_view InField)
{
	const auto Entry = Find(InDocument);
	if (Entry->Document->Loaded().Header.TypeId != InType)
	{
		throw FAutomationError("unsupported_type", "Operation requires a " + std::string(InType) + " document");
	}
	return Entry->Document->Get(InField);
}

TPendingOperation<FAssetDocumentInfo> FAssetAutomation::SetField(const std::string& InDocument,
                                                                 std::uint64_t InGeneration, std::string_view InType,
                                                                 std::string InField, FArchiveNode InValue)
{
	auto Entry = Edit(InDocument, InGeneration);
	(void)ReadField(InDocument, InType, InField);
	try
	{
		return PendingEdit(Entry, FAssetEditWorkflow::Field(Tasks, Assets, Entry->Document, InGeneration,
		                                                    std::move(InField), std::move(InValue)));
	}
	catch (const FAssetWorkflowError& Failure)
	{
		throw FAutomationError(Failure.Code, Failure.what());
	}
}

std::vector<FModelPrimitiveInfo> FAssetAutomation::ModelPrimitives(const FAssetMutationRequest& InRequest)
{
	const auto Entry = Find(InRequest.Document);
	if (Entry->Document->Generation() != InRequest.Generation)
	{
		throw FAutomationError("stale_revision", "Asset changed; restart the property query");
	}
	return DescribeModelPrimitives(*Entry->Document);
}

FAssetDocumentInfo FAssetAutomation::SetPrimitives(const FAssetMutationRequest& InRequest, std::uint32_t InOffset,
                                                   const std::vector<FModelPrimitiveInfo>& InValues)
{
	const auto Entry = Edit(InRequest.Document, InRequest.Generation);
	auto Values = DescribeModelPrimitives(*Entry->Document);
	if (InValues.empty() || InValues.size() > 100 || InOffset > Values.size() ||
	    InValues.size() > Values.size() - InOffset)
	{
		throw std::invalid_argument("Replace 1-100 existing primitive entries");
	}
	std::copy(InValues.begin(), InValues.end(), Values.begin() + InOffset);
	SetModelPrimitives(*Entry->Document, Values);
	return Describe(*Entry);
}
} // namespace Hyperion
