#include "Hyperion/AssetEditing/AssetEditWorkflow.h"
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Automation/Session.h"
#include "Hyperion/Content/ContentRootService.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/Transport/Transport.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>
#include <type_traits>

namespace Hyperion
{
struct FErrorContractValue
{
};

template<> const FRecordDescriptor& RecordType<FErrorContractValue>()
{
	static const auto Type = MakeRecord<FErrorContractValue>("test.domain-error", {});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

struct FFailureCase
{
	std::string Code;
	std::string Message = "Provider failure";
	std::string Path;
	FArchiveNode Details = ParseJson("{}");
	std::function<void()> Raise;
};

void CheckFailure(const FArchiveNode& InValue, const FFailureCase& InCase)
{
	const auto View = ReadAutomationResponse(InValue);
	HYP_CHECK(View.Status == EAutomationStatus::Failed && View.Error);
	HYP_CHECK(View.Error->Code == InCase.Code && View.Error->Message == InCase.Message);
	HYP_CHECK(View.Error->Path == InCase.Path && View.Error->Details);
	HYP_CHECK(WriteJson(*View.Error->Details) == WriteJson(InCase.Details));
}

template<class TError> FFailureCase DomainCase(FErrorCodeId InCode, std::string InExpected)
{
	FFailureCase Result;
	Result.Code = std::move(InExpected);
	Result.Raise = [InCode]
	{
		throw TError(InCode, "Provider failure");
	};
	bool bCaught{};
	try
	{
		Result.Raise();
	}
	catch (const TError& Error)
	{
		bCaught = Error.Code == InCode && Error.Code.GetName() == Result.Code;
		CheckFailure(CurrentAutomationFailure(), Result);
	}
	HYP_CHECK(bCaught);
	return Result;
}

void CheckOperationBoundary(const FFailureCase& InCase)
{
	FOperationCatalog Catalog;
	FOperationInfo Info{"test.sync",        "Failure boundary", "Controlled provider failure", "test", "No mutation",
	                    "Failure returned", ParseJson("{}")};
	Catalog.Register(MakeOperation<FErrorContractValue, FErrorContractValue>(Info,
	                                                                         [&](const auto&) -> FErrorContractValue
	                                                                         {
		                                                                         InCase.Raise();
		                                                                         return {};
	                                                                         }));
	bool bFail{};
	unsigned Cancellations{};
	Info.Id = "test.deferred";
	Catalog.Register(MakeAsyncOperation<FErrorContractValue, FErrorContractValue>(
	    Info,
	    [&](const auto&)
	    {
		    return TPendingOperation<FErrorContractValue>{[&]() -> std::optional<FErrorContractValue>
		                                                  {
			                                                  if (bFail)
			                                                  {
				                                                  InCase.Raise();
			                                                  }
			                                                  return {};
		                                                  },
		                                                  [&]
		                                                  {
			                                                  ++Cancellations;
		                                                  }};
	    }));
	Catalog.Seal();
	FAutomationSession Session(Catalog);
	CheckFailure(Session.Call("test.sync", ParseJson("{}")), InCase);
	const auto Started = Session.Call("test.deferred", ParseJson("{}"));
	const auto Job = std::string(ReadAutomationResponse(Started).Job->Id);
	HYP_CHECK(ReadAutomationResponse(Started).Status == EAutomationStatus::Running);
	bFail = true;
	const auto Finished = Session.GetJob(Job);
	const auto View = ReadAutomationResponse(Finished);
	HYP_CHECK(View.Status == EAutomationStatus::Failed && View.Job && View.Job->Outcome);
	CheckFailure(*View.Job->Outcome, InCase);
	HYP_CHECK(Session.PendingCount() == 0 && Cancellations == 0);
	HYP_CHECK(WriteJson(Session.GetJob(Job)) == WriteJson(Finished));
	bFail = false;
	const auto CancelledStart = Session.Call("test.deferred", ParseJson("{}"));
	const auto Cancelled = Session.CancelJob(ReadAutomationResponse(CancelledStart).Job->Id);
	HYP_CHECK(ReadAutomationResponse(Cancelled).Status == EAutomationStatus::Cancelled);
	HYP_CHECK(Session.PendingCount() == 0 && Cancellations == 1);
}

void CheckNativeOperationAdapter()
{
	bool bCaught{};
	try
	{
		InvokeAutomation(
		    []
		    {
			    throw FSceneEditError(SceneEditErrors::StaleHandle, "Stale native handle");
		    });
	}
	catch (const FAutomationError& Error)
	{
		bCaught = Error.Code == SceneEditErrors::StaleHandle && std::string_view(Error.what()) == "Stale native handle";
	}
	HYP_CHECK(bCaught);
	const FAutomationError Rich(SceneEditErrors::Conflict, "Rich failure", "/member", ParseJson("{\"cause\":42}"));
	bCaught = false;
	try
	{
		InvokeAutomation(
		    [&]
		    {
			    throw Rich;
		    });
	}
	catch (const FAutomationError& Error)
	{
		bCaught =
		    Error.Code == Rich.Code && Error.Path == Rich.Path && WriteJson(Error.Details) == WriteJson(Rich.Details);
	}
	HYP_CHECK(bCaught);
}

void CheckDomainErrors()
{
	static_assert(!std::is_convertible_v<std::string, FErrorCode>);
	static_assert(!std::is_constructible_v<FSceneEditError, std::string, std::string>);
	const auto Cases =
	    std::array{DomainCase<FSceneEditError>(SceneEditErrors::StaleHandle, "stale_handle"),
	               DomainCase<FAssetWorkflowError>(AssetWorkflowErrors::StaleDocument, "stale_document"),
	               DomainCase<FAssetImportError>(AssetImportErrors::Dirty, "dirty"),
	               DomainCase<FContentRootError>(ContentRootErrors::RootUnset, "root_unset"),
	               DomainCase<FTransportError>(TransportErrors::UnsupportedTransport, "unsupported_transport"),
	               DomainCase<FAutomationError>(AutomationErrors::Unavailable, "unavailable")};
	for (const auto& Case : Cases)
	{
		CheckOperationBoundary(Case);
	}
}

void CheckOpenAndRichErrors()
{
	std::string ProviderName = "vendor.future.failure/v2";
	const auto Unknown = FErrorCode::FromExternal(ProviderName);
	ProviderName.assign("destroyed original storage");
	HYP_CHECK(Unknown.GetName() == "vendor.future.failure/v2");
	FFailureCase External;
	External.Code = "vendor.future.failure/v2";
	External.Raise = [Unknown]
	{
		throw FTransportError(Unknown, "Provider failure");
	};
	CheckOperationBoundary(External);
	FFailureCase Rich;
	Rich.Code = "vendor.rich";
	Rich.Path = "/options/mode";
	Rich.Details = ParseJson(R"({"cause":{"value":42},"retry":false})");
	Rich.Raise = [&]
	{
		throw FAutomationError(FErrorCode::FromExternal(Rich.Code), Rich.Message, Rich.Path, Rich.Details);
	};
	CheckOperationBoundary(Rich);
	FFailureCase Wire;
	Wire.Code = "invalid_arguments";
	Wire.Path = "/options/value";
	const FWireError WireError(Wire.Path, Wire.Message);
	Wire.Message = WireError.what();
	Wire.Raise = [WireError]
	{
		throw WireError;
	};
	CheckOperationBoundary(Wire);
	FFailureCase Generic;
	Generic.Code = "operation_failed";
	Generic.Raise = []
	{
		throw std::runtime_error("Provider failure");
	};
	CheckOperationBoundary(Generic);
}
} // namespace

int main()
{
	try
	{
		CheckDomainErrors();
		CheckNativeOperationAdapter();
		CheckOpenAndRichErrors();
		std::cout << "Typed domain errors, open codes, rich failures and deferred boundaries passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
