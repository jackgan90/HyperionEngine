#include "AssetOperations.h"
#include "Hyperion/AssetEditing/AssetProperties.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <bit>
#include <chrono>
#include <source_location>
#include <thread>

namespace
{
using namespace Hyperion;

void Check(bool bInValue, std::source_location InLocation = std::source_location::current())
{
	if (!bInValue)
	{
		throw std::runtime_error("Asset field submission failed at " + std::to_string(InLocation.line()));
	}
}

struct FSubmissionFixture
{
	FTaskSystem Tasks{2, 1};
	std::shared_ptr<FMemoryFileSystem> Files = std::make_shared<FMemoryFileSystem>();
	FIOService IO{Tasks, Files};
	FAssetService Assets{IO};
	FMaterialAsset Material;
	FRecordMemberIdentity Field;

	explicit FSubmissionFixture(bool bInTexture = true)
	{
		RegisterSceneAssetTypes(Assets.Types());
		Material.Name = "Compound default";
		const auto Texture = BuildTextureAsset("Texture", EMaterialTextureEncoding::Linear, {1, 1, {1, 2, 3, 255}});
		const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Texture);
		Files->WriteAtomic(Assets.NormalizePath("submission-texture.hasset"), Encoded.Bytes);
		FMaterialPass Pass;
		Pass.Vertex = {"Test.hlsl", "VSMain"};
		Pass.Pixel = {"Test.hlsl", "PSMain"};
		Material.Passes.push_back(Pass);
		FMaterialAssetParameter Parameter;
		Parameter.Name = "Settings";
		Parameter.Type.Kind = EMaterialValueKind::Structure;
		Parameter.Type.MemberNames = {"Factor", "Sampler"};
		Parameter.Type.Members = {FMaterialParameterType::Numeric(EMaterialScalar::Float),
		                          FMaterialParameterType::Resource(EMaterialValueKind::Sampler)};
		FMaterialAssetValue Default;
		Default.Elements = {{Parameter.Type.Members[0], {std::bit_cast<std::uint32_t>(1.f)}},
		                    {Parameter.Type.Members[1]}};
		if (bInTexture)
		{
			Parameter.Type.MemberNames.push_back("Texture");
			Parameter.Type.Members.push_back(FMaterialParameterType::Resource(EMaterialValueKind::Texture2D));
			FMaterialAssetValue Value{Parameter.Type.Members.back()};
			Value.Texture = FAssetRef{Encoded.Header.Id, "submission-texture.hasset", Encoded.Header.TypeId, {}};
			Default.Elements.push_back(std::move(Value));
		}
		Default.Type = Parameter.Type;
		Parameter.Default = std::move(Default);
		Material.Parameters.push_back(std::move(Parameter));
		ValidateMaterialAsset(Material);
		Files->WriteAtomic(Assets.NormalizePath("submission-material.hasset"),
		                   EncodeAsset(RecordType<FMaterialAsset>(), &Material).Bytes);
		Field = ResolveAssetFieldPolicy(RecordType<FMaterialAsset>(), &FMaterialAsset::Values).Field();
	}

	std::shared_ptr<FAssetEditDocument> Document()
	{
		return std::make_shared<FAssetEditDocument>(Assets.LoadAsync("submission-material.hasset").Get(Tasks));
	}

	FMaterialAssetValues Candidate(bool bInSampler = false) const
	{
		auto Value = *Material.Parameters.front().Default;
		if (bInSampler)
		{
			Value.Elements[1].Sampler.MinLod = 1.f;
		}
		else
		{
			Value.Elements[0].Words[0] = std::bit_cast<std::uint32_t>(.25f);
		}
		return {{"Settings", std::move(Value)}};
	}

	void Complete(const std::shared_ptr<FAssetEditWorkflow>& InWorkflow,
	              const std::shared_ptr<FAssetEditDocument>& InDocument)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while (!InWorkflow->Poll(InDocument))
		{
			Check(std::chrono::steady_clock::now() < Deadline);
			Tasks.PumpMain();
			std::this_thread::yield();
		}
	}
};

void CheckFirstOverride(bool bInSampler)
{
	FSubmissionFixture Fixture;
	auto Document = Fixture.Document();
	const auto Before = Document->Snapshot();
	const auto Generation = Document->Generation();
	const auto Preview = Document->PreviewGeneration();
	const auto Candidate = Fixture.Candidate(bInSampler);
	auto Pending = FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Generation, Fixture.Field,
	                                               WriteValue(Candidate), 71);
	Check(Pending && Pending->Interaction() == 71 && Document->IsEditing());
	Check(Document->Generation() == Generation && !Document->CanUndo());
	Fixture.Complete(Pending, Document);
	Check(!Document->IsEditing() && Document->Generation() == Generation + 1 &&
	      Document->PreviewGeneration() == Preview + 1);
	Check(ReadValue<FMaterialAssetValues>(Document->Get("values")) == Candidate);
	auto Next = Candidate;
	Next[0].Value.Elements[0].Words[0] = std::bit_cast<std::uint32_t>(.5f);
	Check(!FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
	                                       Fixture.Field, WriteValue(Next), 71));
	Document->FinishInteraction();
	Check(Document->Undo() && EqualInspectionValue(Document->Snapshot(), Before) && !Document->CanUndo());
	Check(Document->Redo() && ReadValue<FMaterialAssetValues>(Document->Get("values")) == Next);

	FAssetAutomation Automation(Fixture.Assets, Fixture.Tasks);
	auto Opening = Automation.Open({"submission-material.hasset"});
	std::optional<FAssetDocumentInfo> Info;
	while (!(Info = Opening.Poll()))
	{
		Fixture.Tasks.PumpMain();
		std::this_thread::yield();
	}
	auto Operation = Automation.SetField(Info->Document, Info->Generation, Fixture.Field, WriteValue(Candidate));
	while (!(Info = Operation.Poll()))
	{
		Fixture.Tasks.PumpMain();
		std::this_thread::yield();
	}
	Check(Info->bDirty && Info->bCanUndo && !Info->bEditing);
	Check(EqualInspectionValue(Automation.ReadField(Info->Document, Fixture.Field), WriteValue(Candidate)));
}

void CheckSubmissionTermination()
{
	FSubmissionFixture Fixture;
	for (unsigned Case = 0; Case < 4; ++Case)
	{
		auto Document = Fixture.Document();
		const auto Before = Document->Snapshot();
		auto Pending = FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
		                                               Fixture.Field, WriteValue(Fixture.Candidate()), 91);
		if (Case == 0)
		{
			Pending->FinishInteraction();
			Fixture.Complete(Pending, Document);
			auto Next = Fixture.Candidate(true);
			FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
			                                Fixture.Field, WriteValue(Next), 91);
			Check(Document->Undo() && !EqualInspectionValue(Document->Snapshot(), Before));
			Check(Document->Undo() && EqualInspectionValue(Document->Snapshot(), Before));
		}
		else if (Case == 1)
		{
			Pending->CancelInteraction(91);
			Fixture.Complete(Pending, Document);
			Check(!Document->CanUndo() && !Document->IsDirty() && EqualInspectionValue(Document->Snapshot(), Before));
		}
		else if (Case == 2)
		{
			Pending->Drain();
			Check(!Document->CanUndo() && EqualInspectionValue(Document->Snapshot(), Before));
		}
		else
		{
			bool bRejected{};
			try
			{
				Fixture.Complete(Pending, Fixture.Document());
			}
			catch (const FAssetWorkflowError& Error)
			{
				bRejected = Error.Code == AssetWorkflowErrors::StaleDocument;
			}
			Check(bRejected && !Document->CanUndo() && EqualInspectionValue(Document->Snapshot(), Before));
		}
		Check(!Document->IsEditing());
	}
}

void CheckPendingCandidate()
{
	FSubmissionFixture Fixture;
	auto Document = Fixture.Document();
	const auto Before = Document->Snapshot();
	auto Pending = FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
	                                               Fixture.Field, WriteValue(Fixture.Candidate()), 93);
	auto Latest = Fixture.Candidate(true);
	Latest[0].Value.Elements[0].Words[0] = std::bit_cast<std::uint32_t>(.5f);
	Pending->UpdateField(Fixture.Field, WriteValue(Latest), 93);
	Check(Pending->CanContinueInteraction() && EqualInspectionValue(Document->Snapshot(), Before));
	Check(ReadValue<FMaterialAssetValues>(*Pending->PreparedField(Fixture.Field)) == Latest);
	for (unsigned Case = 0; Case < 3; ++Case)
	{
		auto Invalid = Latest;
		if (Case == 1)
		{
			Invalid[0].Value.Elements[2].Texture->Path = "missing-texture.hasset";
			Invalid[0].Value.Elements[2].Texture->Id.clear();
		}
		else if (Case == 2)
		{
			Invalid[0].Value.Elements[0].Words.clear();
		}
		bool bRejected{};
		try
		{
			Pending->UpdateField(Fixture.Field, WriteValue(Invalid), Case == 0 ? 94 : 93);
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		Check(bRejected && ReadValue<FMaterialAssetValues>(*Pending->PreparedField(Fixture.Field)) == Latest);
		Check(EqualInspectionValue(Document->Snapshot(), Before));
	}
	Pending->FinishInteraction();
	Check(!Pending->CanContinueInteraction());
	Fixture.Complete(Pending, Document);
	Check(ReadValue<FMaterialAssetValues>(Document->Get("values")) == Latest);
	Check(Document->Undo() && EqualInspectionValue(Document->Snapshot(), Before));
}

void CheckCancelledGesture()
{
	FSubmissionFixture Fixture;
	auto Document = Fixture.Document();
	const auto Before = Document->Snapshot();
	auto Pending = FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
	                                               Fixture.Field, WriteValue(Fixture.Candidate()), 92);
	Fixture.Complete(Pending, Document);
	auto Candidate = Fixture.Candidate(true);
	Candidate[0].Value.Elements[2].Texture->Path = "missing-texture.hasset";
	Candidate[0].Value.Elements[2].Texture->Id.clear();
	Pending = FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
	                                          Fixture.Field, WriteValue(Candidate), 92);
	Pending->CancelInteraction(92);
	Fixture.Complete(Pending, Document);
	Check(!Document->IsDirty() && !Document->CanUndo() && !Document->CanRedo() && !Document->IsEditing());
	Check(EqualInspectionValue(Document->Snapshot(), Before));
}

void CheckReferenceValidation()
{
	FSubmissionFixture Fixture;
	for (const bool bMissing : {false, true})
	{
		auto Document = Fixture.Document();
		auto Candidate = Fixture.Candidate();
		if (bMissing)
		{
			Candidate[0].Value.Elements[2].Texture->Path = "missing-texture.hasset";
			Candidate[0].Value.Elements[2].Texture->Id.clear();
		}
		else
		{
			const auto Texture =
			    BuildTextureAsset("Replacement", EMaterialTextureEncoding::Linear, {1, 1, {4, 5, 6, 255}});
			const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Texture);
			Fixture.Files->WriteAtomic(Fixture.Assets.NormalizePath("replacement.hasset"), Encoded.Bytes);
			Candidate[0].Value.Elements[2].Texture =
			    FAssetRef{Encoded.Header.Id, "replacement.hasset", Encoded.Header.TypeId, {}};
		}
		auto Pending = FAssetEditWorkflow::SubmitField(Fixture.Tasks, Fixture.Assets, Document, Document->Generation(),
		                                               Fixture.Field, WriteValue(Candidate));
		bool bFailed{};
		try
		{
			Fixture.Complete(Pending, Document);
		}
		catch (const std::exception&)
		{
			bFailed = true;
		}
		Check(bFailed == bMissing && !Document->IsEditing());
		Check(Document->CanUndo() != bMissing && Document->IsDirty() != bMissing);
	}
	FSubmissionFixture Numeric(false);
	auto Document = Numeric.Document();
	Check(!FAssetEditWorkflow::SubmitField(Numeric.Tasks, Numeric.Assets, Document, Document->Generation(),
	                                       Numeric.Field, WriteValue(Numeric.Candidate())));
	Check(Document->CanUndo() && !Document->IsEditing());
}
} // namespace

void CheckAssetFieldSubmissions()
{
	CheckFirstOverride(false);
	CheckFirstOverride(true);
	CheckSubmissionTermination();
	CheckPendingCandidate();
	CheckCancelledGesture();
	CheckReferenceValidation();
}
