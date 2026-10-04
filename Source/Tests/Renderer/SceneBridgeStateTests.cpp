#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Renderer/SceneBridge.h"
#include "Support/ModelAssetSupport.h"
#include "Support/TestSupport.h"
#include <array>
#include <chrono>
#include <thread>

namespace
{
using namespace Hyperion;

std::shared_ptr<const FSceneModelData> MakeBridgeData()
{
	FModelSource Source;
	FModelPrimitive Primitive;
	Primitive.Positions = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
	Primitive.Indices = {0, 1, 2};
	Primitive.Material = 0;
	Source.Primitives = {Primitive, Primitive};
	Source.Materials.emplace_back();
	Source.Materials.front().bUnlit = true;
	FModelNode Node;
	Node.Primitives = {0, 1};
	Source.Nodes.push_back(Node);
	Source.Roots = {0};
	return PrepareSourceModel(std::make_shared<const FModelSource>(std::move(Source)));
}

void ObservePublication(FTaskSystem& InTasks, FSceneRenderBridge& InBridge)
{
	InBridge.Flush();
	InTasks.Wait(InBridge.GetReceipt());
	InBridge.Flush();
}

void AwaitBridgeModel(FTaskSystem& InTasks, FSceneRenderBridge& InBridge, FSceneHandle InHandle)
{
	InBridge.Flush();
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!InBridge.IsReady(InHandle) && InBridge.GetError(InHandle).empty() &&
	       std::chrono::steady_clock::now() < Deadline)
	{
		InBridge.Flush();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(InBridge.IsReady(InHandle));
	ObservePublication(InTasks, InBridge);
}

std::shared_ptr<const FRenderResource> CheckBridgeStatusComponents(FTaskSystem& InTasks, FRenderSession& InSession,
                                                                   std::shared_ptr<const FSceneModelData> InData)
{
	FScene Scene;
	FSceneRenderBridge Bridge(Scene, InSession, InTasks);
	const auto Initial = Bridge.GetStatusRevision();
	HYP_CHECK(Initial.BridgeStatus == 1 && Bridge.GetModelStatusRevision().BridgeStatus == 1);
	HYP_CHECK(Initial.ResourcePublication == InSession.GetResources().GetPublicationRevision());
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("camera", {0, 0, 3}, {}));
	ObservePublication(InTasks, Bridge);
	const auto Metadata = Bridge.GetStatusRevision();
	const auto ModelMetadata = Bridge.GetModelStatusRevision();
	HYP_CHECK(Metadata.BridgeStatus > Initial.BridgeStatus && ModelMetadata.BridgeStatus == 1);
	Bridge.Flush();
	HYP_CHECK(Bridge.GetStatusRevision() == Metadata && Bridge.GetModelStatusRevision() == ModelMetadata);
	Scene.SetWorldTransform(Camera, Translation({1, 0, 3}));
	ObservePublication(InTasks, Bridge);
	HYP_CHECK(Bridge.GetStatusRevision().BridgeStatus > Metadata.BridgeStatus);
	HYP_CHECK(Bridge.GetModelStatusRevision() == ModelMetadata);

	const auto BeforeResource = Bridge.GetStatusRevision();
	const auto BeforeModelResource = Bridge.GetModelStatusRevision();
	auto Resource = InSession.GetResources().RequestModel(InData);
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (Resource->GetStatus() != ERenderResourceStatus::Ready && Resource->GetError().empty() &&
	       std::chrono::steady_clock::now() < Deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(Resource->GetStatus() == ERenderResourceStatus::Ready);
	const auto AfterResource = Bridge.GetStatusRevision();
	const auto AfterModelResource = Bridge.GetModelStatusRevision();
	HYP_CHECK(AfterResource.BridgeStatus == BeforeResource.BridgeStatus && AfterResource != BeforeResource);
	HYP_CHECK(AfterModelResource.BridgeStatus == BeforeModelResource.BridgeStatus &&
	          AfterModelResource != BeforeModelResource);
	HYP_CHECK(AfterResource.ResourcePublication > BeforeResource.ResourcePublication);
	HYP_CHECK(AfterModelResource.ResourcePublication == InSession.GetResources().GetPublicationRevision());

	const auto Handle = Scene.Add({"model", InData});
	AwaitBridgeModel(InTasks, Bridge, Handle);
	HYP_CHECK(Bridge.GetStatusRevision().BridgeStatus > AfterResource.BridgeStatus);
	HYP_CHECK(Bridge.GetModelStatusRevision().BridgeStatus > AfterModelResource.BridgeStatus);
	const auto Ready = Bridge.GetStatusRevision();
	const auto ModelReady = Bridge.GetModelStatusRevision();
	Bridge.Flush();
	HYP_CHECK(Bridge.GetStatusRevision() == Ready && Bridge.GetModelStatusRevision() == ModelReady);
	Scene.Remove(Handle);
	ObservePublication(InTasks, Bridge);
	HYP_CHECK(Bridge.GetModelStatusRevision().BridgeStatus > ModelReady.BridgeStatus);
	Bridge.Close();
	return Resource;
}

using FSectionSnapshots = std::array<std::shared_ptr<const FMaterialSnapshot>, 2>;

FSectionSnapshots ReadSectionSnapshots(FTaskSystem& InTasks, FRenderSession& InSession)
{
	FSectionSnapshots Result;
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              FRenderView View;
		                              View.CullingMode = ESceneCullingMode::None;
		                              const auto Items = InSession.GetScene().Collect(View);
		                              HYP_CHECK(Items.Items.Size() == Result.size());
		                              for (const auto& Item : Items.Items)
		                              {
			                              HYP_CHECK(Item.State.Section < Result.size());
			                              HYP_CHECK(!Result[Item.State.Section]);
			                              Result[Item.State.Section] = Item.State.Surface->GetSnapshot();
		                              }
	                              }));
	return Result;
}

void CheckSectionSnapshots(FTaskSystem& InTasks, FRenderSession& InSession, const FSectionSnapshots& InExpected)
{
	const auto Actual = ReadSectionSnapshots(InTasks, InSession);
	for (std::size_t Index = 0; Index < Actual.size(); ++Index)
	{
		HYP_CHECK(Actual[Index] && InExpected[Index]);
		HYP_CHECK(Actual[Index]->Identity == InExpected[Index]->Identity);
		HYP_CHECK(Actual[Index]->Revision == InExpected[Index]->Revision);
	}
}

void CheckBridgeMaterialRevisions(FTaskSystem& InTasks, FRenderSession& InSession,
                                  std::shared_ptr<const FSceneModelData> InData,
                                  std::shared_ptr<const FMaterialSnapshot> InDefault)
{
	auto Surface = std::make_shared<FMaterialInstance>(InDefault);
	auto Section = std::make_shared<FMaterialInstance>(InDefault);
	HYP_CHECK(Surface->GetIdentity() != Section->GetIdentity() && Surface->GetRevision() == Section->GetRevision());
	FScene Scene;
	FSceneRenderBridge Bridge(Scene, InSession, InTasks);
	FSceneModel Model{"sections", InData};
	Model.Surface.Instance = Surface;
	Model.SectionSurfaces[0].Instance = Surface;
	Model.SectionSurfaces[1].Instance = Section;
	const auto Handle = Scene.Add(Model);
	AwaitBridgeModel(InTasks, Bridge, Handle);
	CheckSectionSnapshots(InTasks, InSession, {Surface->Freeze(), Section->Freeze()});
	const auto Retained = ReadSectionSnapshots(InTasks, InSession);
	const auto OldSectionRevision = Section->GetRevision();
	const auto SceneRevision = Scene.GetRevision();
	const auto Prepared = Bridge.GetModelPreparationCount();
	Section->SetSemantic("Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{0, 1, 0, 1}));
	HYP_CHECK(Scene.GetChanges().empty() && Scene.GetRevision() == SceneRevision);
	AwaitBridgeModel(InTasks, Bridge, Handle);
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 1);
	CheckSectionSnapshots(InTasks, InSession, {Surface->Freeze(), Section->Freeze()});
	HYP_CHECK(Retained[1]->Revision == OldSectionRevision && Retained[1]->Revision != Section->GetRevision());
	Bridge.Flush();
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 1);

	Model.SectionSurfaces.erase(1);
	Model.SectionSurfaces[0].Instance = Section;
	Scene.Update(Handle, Model);
	AwaitBridgeModel(InTasks, Bridge, Handle);
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 2);
	CheckSectionSnapshots(InTasks, InSession, {Section->Freeze(), Surface->Freeze()});
	Model.SectionSurfaces.clear();
	Scene.Update(Handle, Model);
	AwaitBridgeModel(InTasks, Bridge, Handle);
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 3);
	CheckSectionSnapshots(InTasks, InSession, {Surface->Freeze(), Surface->Freeze()});
	Section->SetSemantic("Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{1, 0, 0, 1}));
	Bridge.Flush();
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 3);

	Model.Surface = {};
	Scene.Update(Handle, Model);
	AwaitBridgeModel(InTasks, Bridge, Handle);
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 4);
	CheckSectionSnapshots(InTasks, InSession, {InDefault, InDefault});
	Surface->SetSemantic("Pbr.BaseColorFactor", FMaterialValue::Float(FVec4{0, 0, 1, 1}));
	Bridge.Flush();
	HYP_CHECK(Bridge.GetModelPreparationCount() == Prepared + 4);
	Bridge.Close();
}
} // namespace

void RunSceneBridgeStateTests(Hyperion::FTaskSystem& InTasks, Hyperion::FRenderSession& InSession)
{
	const auto Data = MakeBridgeData();
	const auto Resource = CheckBridgeStatusComponents(InTasks, InSession, Data);
	CheckBridgeMaterialRevisions(InTasks, InSession, Data, Resource->GetMaterial(0)->GetSnapshot());
}
