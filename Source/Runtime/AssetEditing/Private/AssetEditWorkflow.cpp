#include "Hyperion/AssetEditing/AssetEditWorkflow.h"
#include "AssetFieldPolicy.h"

namespace Hyperion
{
FAssetEditWorkflow::FAssetEditWorkflow(FTaskSystem& InTasks, std::shared_ptr<FAssetEditDocument> InDocument,
                                       std::uint64_t InGeneration)
    : Tasks(InTasks), Document(std::move(InDocument)), Generation(InGeneration), AssetId(Document->Loaded().Header.Id)
{
	if (Document->Generation() != Generation)
	{
		throw FAssetWorkflowError("stale_revision", "Asset changed; query the current generation");
	}
	if (Document->IsEditing())
	{
		throw FAssetWorkflowError("busy", "An asset edit is already preparing");
	}
	Document->bEditing = true;
}

std::shared_ptr<FAssetEditWorkflow> FAssetEditWorkflow::Encoding(FTaskSystem& InTasks,
                                                                 std::shared_ptr<FAssetEditDocument> InDocument,
                                                                 std::uint64_t InGeneration,
                                                                 EMaterialTextureEncoding InEncoding)
{
	if (InDocument->Loaded().Type->CppType != typeid(FTextureAsset) ||
	    !CanEditTextureEncoding(*InDocument->Loaded().As<FTextureAsset>()))
	{
		throw FAssetWorkflowError("unsupported_type", "Encoding requires an editable RGBA8 2D texture");
	}
	auto Result = std::shared_ptr<FAssetEditWorkflow>(new FAssetEditWorkflow(InTasks, InDocument, InGeneration));
	Result->FieldIdentity = ResolveAssetFieldPolicy(*InDocument->Loaded().Type, &FTextureAsset::Encoding).Field();
	Result->EncodingResult = DispatchAsync<FArchiveNode>(InTasks, {EDomain::Worker},
	                                                     [Draft = InDocument->Snapshot(), InEncoding]
	                                                     {
		                                                     return RebuildTextureEncodingDraft(Draft, InEncoding);
	                                                     });
	return Result;
}

std::shared_ptr<FAssetEditWorkflow> FAssetEditWorkflow::Field(FTaskSystem& InTasks, FAssetService& InAssets,
                                                              std::shared_ptr<FAssetEditDocument> InDocument,
                                                              std::uint64_t InGeneration, std::string InField,
                                                              FArchiveNode InValue)
{
	const auto Policy = ResolveAssetFieldPolicy(*InDocument->Loaded().Type, InField);
	return Field(InTasks, InAssets, std::move(InDocument), InGeneration, Policy.Field(), std::move(InValue));
}

std::shared_ptr<FAssetEditWorkflow> FAssetEditWorkflow::Field(FTaskSystem& InTasks, FAssetService& InAssets,
                                                              std::shared_ptr<FAssetEditDocument> InDocument,
                                                              std::uint64_t InGeneration,
                                                              const FRecordMemberIdentity& InField,
                                                              FArchiveNode InValue)
{
	auto Result = std::shared_ptr<FAssetEditWorkflow>(new FAssetEditWorkflow(InTasks, InDocument, InGeneration));
	Result->FieldIdentity = InField;
	Result->Prepared = std::make_unique<FPreparedAssetField>(
	    PrepareAssetField(*InDocument, Result->FieldIdentity, std::move(InValue)));
	for (const auto& Reference : Result->Prepared->References)
	{
		Result->Graphs.push_back(InAssets.LoadGraphAsync(Reference.Reference, InDocument->Loaded().Path));
	}
	return Result;
}

FAssetEditWorkflow::~FAssetEditWorkflow()
{
	Drain();
}

void FAssetEditWorkflow::Release()
{
	if (bPending)
	{
		Document->bEditing = false;
		bPending = false;
	}
}

bool FAssetEditWorkflow::IsPending() const
{
	return bPending;
}

bool FAssetEditWorkflow::Poll(const std::shared_ptr<FAssetEditDocument>& InCurrent)
{
	if (!bPending)
	{
		return true;
	}
	if ((EncodingResult && !EncodingResult->Ready()) || std::any_of(Graphs.begin(), Graphs.end(),
	                                                                [](const auto& InGraph)
	                                                                {
		                                                                return !InGraph.Ready();
	                                                                }))
	{
		return false;
	}
	Release(); // All completion paths release admission before publishing or reporting an error.
	if (InCurrent != Document || Document->Loaded().Header.Id != AssetId)
	{
		throw FAssetWorkflowError("stale_document", "The edited document was closed or replaced");
	}
	if (Document->Generation() != Generation)
	{
		throw FAssetWorkflowError("stale_revision", "Asset changed while preparing the edit");
	}
	if (EncodingResult)
	{
		Document->Apply(FieldIdentity, *EncodingResult->GetReady(), 0, FAssetEditDocument::ETarget::Root);
	}
	else
	{
		for (std::size_t Index = 0; Index < Graphs.size(); ++Index)
		{
			const auto& Reference = Prepared->References[Index];
			ValidateAssetReferenceGraph(*Graphs[Index].GetReady(), Reference.Reference.TypeId, Reference.Dimension);
		}
		Document->Apply(FieldIdentity, std::move(Prepared->Value), 0);
	}
	return true;
}

void FAssetEditWorkflow::Drain()
{
	const auto Join = [this](const FTaskHandle& InTask)
	{
		try
		{
			Tasks.Wait(InTask);
		}
		catch (...)
		{
			// No commit during destruction. Poll owns terminal preparation errors.
		}
	};
	if (EncodingResult)
	{
		Join(EncodingResult->Task());
	}
	for (const auto& Graph : Graphs)
	{
		Join(Graph.Task());
	}
	Release();
}
} // namespace Hyperion
