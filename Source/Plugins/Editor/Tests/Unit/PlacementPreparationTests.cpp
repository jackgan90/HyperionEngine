#include "../../Private/PlacementService.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <iostream>
#include <source_location>

using namespace Hyperion;

namespace
{
void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Placement preparation check failed at " + std::to_string(InLocation.line()));
	}
}

void CheckAggregation()
{
	FPlacementCandidate Candidate;
	Candidate.Model = FAssetRef{};
	FPlacementPreparationContext Context;
	Context.bSceneAvailable = true;
	const auto Resolve = [&](FPlacementPreparation InModel)
	{
		return FPlacementService::ResolvePreparation(Candidate, InModel, Context);
	};
	for (const auto Stage : {EPlacementPreparationStage::ModelLoading, EPlacementPreparationStage::ModelUpload})
	{
		const auto Pending = Resolve({EPlacementPreparationState::Pending, Stage, "Failed: display only"});
		Check(Pending.State == EPlacementPreparationState::Pending && Pending.Stage == Stage);
		for (const std::string Error : {"Preparing is an error", "Failed: collision", ""})
		{
			const auto Failed = Resolve({EPlacementPreparationState::Failed, Stage, Error});
			Check(Failed.State == EPlacementPreparationState::Failed && Failed.Error == Error);
			Check(!FormatPlacementPreparation(Failed).empty());
		}
	}
	Context.PreviewMaterial = {EPlacementPreparationState::Pending, EPlacementPreparationStage::PreviewMaterial};
	Check(Resolve({}).State == EPlacementPreparationState::Pending);
	Context.PreviewMaterial.State = EPlacementPreparationState::Failed;
	Context.PreviewMaterial.Error = "Preparing shader failed";
	Check(Resolve({}).State == EPlacementPreparationState::Failed);
	Context.PreviewMaterial = {};
	Check(Resolve({}).State == EPlacementPreparationState::Ready);
	Context.bSceneAvailable = false;
	Check(Resolve({}).Stage == EPlacementPreparationStage::Scene);
	Check(Resolve({}).State == EPlacementPreparationState::Failed);
}

void CheckIcons()
{
	FPlacementCandidate Candidate;
	Candidate.Icon = "Light";
	FPlacementPreparationContext Context;
	Context.bSceneAvailable = true;
	Check(FPlacementService::ResolvePreparation(Candidate, {}, Context).State == EPlacementPreparationState::Failed);
	for (const auto Stage : {EPlacementPreparationStage::IconLoading, EPlacementPreparationStage::IconUpload})
	{
		Context.Icon = FPlacementPreparation{EPlacementPreparationState::Pending, Stage};
		const auto Pending = FPlacementService::ResolvePreparation(Candidate, {}, Context);
		Check(Pending.State == EPlacementPreparationState::Pending && Pending.Stage == Stage);
		Context.Icon->State = EPlacementPreparationState::Failed;
		Context.Icon->Error = "Preparing icon failed";
		Check(FPlacementService::ResolvePreparation(Candidate, {}, Context).State ==
		      EPlacementPreparationState::Failed);
	}
	Context.Icon = FPlacementPreparation{};
	Check(FPlacementService::ResolvePreparation(Candidate, {}, Context).State == EPlacementPreparationState::Ready);
	Candidate.Icon.clear();
	Context.Icon.reset();
	Context.PreviewMaterial.State = EPlacementPreparationState::Failed;
	Check(FPlacementService::ResolvePreparation(Candidate, {}, Context).State == EPlacementPreparationState::Ready);
}

void CheckModelAndCommit()
{
	FPlacementService Service;
	FPlacementCandidate Candidate;
	Candidate.Id = "model";
	Candidate.Model = FAssetRef{};
	FPlacementPreparationContext Context;
	Context.bSceneAvailable = true;
	Check(Service.GetPreparation(Candidate, Context).Stage == EPlacementPreparationStage::ModelLoading);
	auto& Model = Service.Models[Candidate.Id];
	Model.bLoadComplete = true;
	for (const std::string Error : {"Preparing load failed", ""})
	{
		Model.Error = Error;
		Check(Service.GetPreparation(Candidate, Context).State == EPlacementPreparationState::Failed);
	}
	Model.Error.clear();
	Model.Data = std::make_shared<FSceneModelData>();
	Check(Service.GetPreparation(Candidate, Context).Stage == EPlacementPreparationStage::ModelUpload);
	Check(Service.GetPreparation(Candidate, Context).State == EPlacementPreparationState::Pending);
	Model.UploadError = "";
	Check(Service.GetPreparation(Candidate, Context).State == EPlacementPreparationState::Failed);
	FSceneEditDocument Document;
	for (const auto State : {EPlacementPreparationState::Pending, EPlacementPreparationState::Failed})
	{
		Context.bSceneAvailable = State == EPlacementPreparationState::Pending;
		Model.UploadError.reset();
		bool bRejected{};
		try
		{
			Service.Commit(Candidate, {}, Document, Context);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		Check(bRejected && Document.GetState().History.empty());
	}
}
} // namespace

int main()
{
	try
	{
		CheckAggregation();
		CheckIcons();
		CheckModelAndCommit();
		std::cout << "PASS: typed placement stages, message collisions and final admission\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
