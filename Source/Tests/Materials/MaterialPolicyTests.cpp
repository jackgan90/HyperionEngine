#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Materials/PbrMaterial.h"
#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Support/TestSupport.h"

using namespace Hyperion;

void CheckLegacyMaterialPasses()
{
	// Fixed version-one records: these do not use the current record writer.
	const auto Fixtures = ParseJson(R"JSON([
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"VSMain"}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"PSMain"}}}},
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"/Engine/Shaders/Model.hlsl","entry":"VSMain"}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"/Engine/Shaders/Model.hlsl","entry":"PSMain"}}}},
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Custom.hlsl","entry":"VSMain"}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Custom.hlsl","entry":"PSMain"}}}},
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"CustomVertex"}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"PSMain"}}}},
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"VSMain"}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Other.hlsl","entry":"PSMain"}}}},
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"VSMain",
 "defines":[{"type":"hyperion.materialshaderdefine","version":1,"fields":{"name":"CUSTOM","value":"1"}}]}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"PSMain"}}}},
{"type":"hyperion.materialpass","version":1,"fields":{
 "vertex":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"VSMain"}},
 "pixel":{"type":"hyperion.materialshader","version":1,"fields":{"path":"Model.hlsl","entry":"PSMain",
 "defines":[{"type":"hyperion.materialshaderdefine","version":1,"fields":{"name":"CUSTOM","value":"1"}}]}}}}
])JSON");
	const std::array Expected{true, true, false, false, false, false, true};
	const auto& Records = std::get<FArchiveNode::FArray>(Fixtures.Value);
	HYP_CHECK(Records.size() == Expected.size());
	for (std::size_t Index = 0; Index < Records.size(); ++Index)
	{
		const auto Pass = ReadValue<FMaterialPass>(Records[Index]);
		const bool bLegacyCoverage =
		    (Pass.Vertex.Path == "Model.hlsl" || Pass.Vertex.Path == "/Engine/Shaders/Model.hlsl") &&
		    Pass.Vertex.Entry == "VSMain" && Pass.Pixel.Path == Pass.Vertex.Path && Pass.Pixel.Entry == "PSMain" &&
		    Pass.Vertex.Defines.empty();
		HYP_CHECK(bLegacyCoverage == Expected[Index]);
		HYP_CHECK(Pass.SilhouettePolicy ==
		          (Expected[Index] ? EMaterialSilhouettePolicy::ModelShader : EMaterialSilhouettePolicy::Disabled));
		HYP_CHECK(RecordVersion(WriteValue(Pass)) == 2);
	}
}

void CheckMaterialPolicies()
{
	FMaterialDescription Description;
	Description.Name = "Explicit coverage";
	FMaterialPass Pass;
	Pass.Vertex = {"Model.hlsl", "VSMain"};
	Pass.Pixel = {"Model.hlsl", "PSMain"};
	Description.Passes = {Pass};
	const FMaterialDefinition Disabled(Description);
	HYP_CHECK(Disabled.GetPass().SilhouettePolicy == EMaterialSilhouettePolicy::Disabled);
	Description.Passes[0].SilhouettePolicy = EMaterialSilhouettePolicy::ModelShader;
	const FMaterialDefinition Enabled(Description);
	HYP_CHECK(Enabled.GetIdentity() != Disabled.GetIdentity());
	HYP_CHECK(Enabled.GetPass().SilhouettePolicy == EMaterialSilhouettePolicy::ModelShader);
	const auto Asset = PersistMaterialDescription(Enabled.GetDescription());
	const auto Bytes = EncodeAsset(RecordType<FMaterialAsset>(), &Asset);
	const auto Document = DecodeAsset(std::make_shared<const std::vector<std::byte>>(Bytes.Bytes));
	const auto Loaded = ReadValue<FMaterialAsset>(Document.Object);
	HYP_CHECK(Loaded.Passes[0].SilhouettePolicy == EMaterialSilhouettePolicy::ModelShader);
	const FMaterialDefinition Restored(ResolveMaterialAssetDescription(Loaded));
	HYP_CHECK(Restored.GetPass().SilhouettePolicy == Enabled.GetPass().SilhouettePolicy);
	Description.Passes[0].Usage = "CustomCoverage";
	Description.Passes[0].Vertex.Path = Description.Passes[0].Pixel.Path = "Custom.hlsl";
	const FMaterialDefinition Custom(Description);
	HYP_CHECK(Custom.GetPass("CustomCoverage").SilhouettePolicy == EMaterialSilhouettePolicy::ModelShader);
	Description.Passes[0].SilhouettePolicy = static_cast<EMaterialSilhouettePolicy>(255);
	bool bRejected{};
	try
	{
		(void)FMaterialDefinition(Description);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	for (const auto Queue : {EMaterialQueue::Opaque, EMaterialQueue::Masked, EMaterialQueue::Transparent})
	{
		const auto Builtin = MakePbrMaterialAsset("PBR", Queue);
		for (const auto& BuiltinPass : Builtin.Passes)
		{
			HYP_CHECK(BuiltinPass.SilhouettePolicy == (BuiltinPass.Usage == "ShadowDepth"
			                                               ? EMaterialSilhouettePolicy::Disabled
			                                               : EMaterialSilhouettePolicy::ModelShader));
		}
	}
}

void CheckLegacyMaterialWire()
{
	// Old wire inputs have no record version and omit the newly optional field.
	const auto Wire = ParseJson(R"JSON({"vertex":{"path":"Model.hlsl","entry":"VSMain"},
	    "pixel":{"path":"Model.hlsl","entry":"PSMain"}})JSON");
	const auto Pass = std::static_pointer_cast<FMaterialPass>(ReadRecordWire(RecordType<FMaterialPass>(), Wire));
	HYP_CHECK(!Pass->SilhouettePolicy);
	FMaterialDescription Description;
	Description.Name = "Old wire coverage";
	Description.Passes = {*Pass};
	const FMaterialDefinition Legacy(Description);
	HYP_CHECK(Legacy.GetPass().SilhouettePolicy == EMaterialSilhouettePolicy::ModelShader);
	for (const auto Value : {EMaterialSilhouettePolicy::Disabled, EMaterialSilhouettePolicy::ModelShader})
	{
		Pass->SilhouettePolicy = Value;
		const auto Roundtrip = std::static_pointer_cast<FMaterialPass>(
		    ReadRecordWire(RecordType<FMaterialPass>(), WriteRecordWire(RecordType<FMaterialPass>(), Pass.get())));
		Description.Passes = {*Roundtrip};
		HYP_CHECK(FMaterialDefinition(Description).GetPass().SilhouettePolicy == Value);
	}
	HYP_CHECK(std::string_view(MaterialUsages::Forward) == "Forward");
	HYP_CHECK(std::string_view(MaterialUsages::HdrForwardOpaque) == "HdrForwardOpaque");
	HYP_CHECK(std::string_view(MaterialUsages::HdrTransparent) == "HdrTransparent");
	HYP_CHECK(std::string_view(MaterialUsages::HdrCompatibility) == "HdrCompatibility");
	HYP_CHECK(std::string_view(MaterialUsages::DeferredBase) == "DeferredBase");
	HYP_CHECK(std::string_view(MaterialUsages::ShadowDepth) == "ShadowDepth");
	HYP_CHECK(std::string_view(MaterialUsages::SilhouetteMask) == "SilhouetteMask");
	HYP_CHECK(std::string_view(MaterialVariants::Default) == "Default");
	HYP_CHECK(std::string_view(MaterialVariants::Instance) == "Instance");
}

void CheckPixelLessSilhouettePolicy()
{
	FMaterialDescription Description;
	Description.Name = "Vertex-only policy";
	FMaterialPass Pass;
	Pass.Vertex = {"Model.hlsl", "VSMain"};
	Pass.State.ColorWriteMask = 0;
	Description.Passes = {Pass};
	const FMaterialDefinition Disabled(Description);
	HYP_CHECK(Disabled.GetPass().SilhouettePolicy == EMaterialSilhouettePolicy::Disabled);
	(void)PersistMaterialDescription(Disabled.GetDescription());
	Description.Passes[0].SilhouettePolicy = EMaterialSilhouettePolicy::ModelShader;
	for (const bool bPersist : {false, true})
	{
		bool bRejected{};
		try
		{
			if (bPersist)
			{
				(void)PersistMaterialDescription(Description);
			}
			else
			{
				(void)FMaterialDefinition(Description);
			}
		}
		catch (const std::invalid_argument& Error)
		{
			bRejected = std::string_view(Error.what()) == "Model shader silhouette coverage requires a pixel shader";
		}
		HYP_CHECK(bRejected);
	}
}
