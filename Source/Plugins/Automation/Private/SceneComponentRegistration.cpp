#include "Hyperion/AutomationHost/SceneComponentOperations.h"

namespace Hyperion
{
FOperationInfo SceneComponentOperationInfo(std::string InId, std::string InSummary, bool bInReadOnly,
                                           FSceneEditDocument* InDocument, std::string_view InOwner)
{
	FOperationInfo Info;
	Info.Id = std::move(InId);
	Info.Summary = std::move(InSummary);
	Info.Description =
	    "Uses registered component reflection and the live scene document. Query scene.components.list for instance "
	    "IDs and types.describe for values. Edits require current revision and an idle host, validate the complete "
	    "candidate and commit one atomic transaction. Resource bindings remain owned by the target.";
	Info.Owner = InOwner;
	Info.bReadOnly = bInReadOnly;
	Info.Effects =
	    bInReadOnly ? "Reads scene component values." : "Updates shared scene/history; save explicitly to persist.";
	Info.Completion = "Main state committed; does not wait for rendering or write disk.";
	Info.Keywords = {"scene", "component", "property", "camera", "light", "model"};
	Info.Unavailable = InDocument ? "" : "This target has no scene document provider.";
	return Info;
}

FSceneEditDocument& RequireSceneComponentDocument(FSceneEditDocument* InDocument)
{
	if (!InDocument)
	{
		throw FAutomationError(AutomationErrors::Unavailable, "This target has no scene document provider.");
	}
	return *InDocument;
}

void RegisterSceneComponentOperationFamily(FOperationCatalog& InCatalog, const FRecordDescriptor& InType,
                                           std::vector<FOperationDescriptor> InOperations)
{
	InCatalog.RequireOwner();
	const auto Component = SceneComponentRegistry().TryFind(InType.Id);
	if (InCatalog.IsSealed() || !Component || Component->CppType != InType.CppType || Component->Record != &InType)
	{
		throw std::invalid_argument("Late or inconsistent scene component operation registration: " + InType.Id);
	}
	// Validate all descriptors, examples and nested types without publishing any live callable.
	FOperationCatalog Candidate;
	for (const auto& Operation : InOperations)
	{
		Candidate.Register(Operation);
		bool bExists = true;
		try
		{
			(void)InCatalog.Find(Operation.Info.Id);
		}
		catch (const FAutomationError& Error)
		{
			if (Error.Code != AutomationErrors::NotFound)
			{
				throw;
			}
			bExists = false;
		}
		if (bExists)
		{
			throw std::invalid_argument("Duplicate component operation: " + Operation.Info.Id);
		}
	}
	// A conflicting live type must also fail before adding any member of the family.
	for (const auto& Operation : InOperations)
	{
		InCatalog.RegisterType(*Operation.Request);
		InCatalog.RegisterType(*Operation.Result);
	}
	for (auto& Operation : InOperations)
	{
		InCatalog.Register(std::move(Operation));
	}
}
} // namespace Hyperion
