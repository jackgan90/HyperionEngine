#include "Support/TestSupport.h"
#include <Hyperion/Config/AppSettings.h>
#include <Hyperion/Plugins/PluginRuntime.h>
#include <Hyperion/Reflection/Wire.h>
#include <array>
#include <fstream>
#include <iostream>

void CheckRasterOptionSettings();

namespace
{
struct FProbe final : Hyperion::FPlugin
{
	std::vector<int>& Events;
	int Id;
	bool bFail;

	FProbe(std::vector<int>& InE, int InI, bool bInF = false) : Events(InE), Id(InI), bFail(bInF)
	{
	}

	void Start() override
	{
		Events.push_back(Id);
		if (bFail)
		{
			throw std::runtime_error("expected startup failure");
		}
	}

	void Stop() noexcept override
	{
		Events.push_back(-Id);
	}
};

void CheckSettingsPersistence()
{
	Hyperion::FAppSettings Settings;
	HYP_CHECK(Settings.bReversedZ);
	HYP_CHECK(Settings.bClusteredLighting);
	Settings.bClusteredLighting = false;
	Settings.bReversedZ = false;
	Settings.Width = 960;
	Settings.TriangleScale = 0.6;
	Settings.MainRenderLead = 2;
	Settings.RenderRhiLead = 3;
	Settings.Plugins = {"triangle"};
	Settings.DisabledPlugins = {"debug-ui"};
	Hyperion::SaveSettings("test-settings.json", Settings);
	const auto Loaded = Hyperion::LoadSettings("test-settings.json");
	HYP_CHECK(!Loaded.bReversedZ);
	HYP_CHECK(Loaded.DisabledPlugins == Settings.DisabledPlugins);
	HYP_CHECK(!Loaded.bClusteredLighting);
	HYP_CHECK(Loaded.MainRenderLead == 2 && Loaded.RenderRhiLead == 3);
	HYP_CHECK(Loaded.Width == 960 && Loaded.TriangleScale == 0.6 && Loaded.Plugins == Settings.Plugins);
	{
		std::ofstream File("test-invalid.json");
		File
		    << R"({"type":"hyperion.application-settings","schema_version":1,"properties":{"title":"changed","width":-20}})";
	}
	const auto Before = Settings;
	bool bRejected = false;
	try
	{
		Hyperion::LoadReflected("test-invalid.json", Hyperion::SettingsType(), &Settings);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected && Settings.Width == Before.Width && Settings.Title == Before.Title);
	{
		std::ofstream File("test-invalid.json");
		File << R"({"type":"hyperion.application-settings","schema_version":99,"properties":{}})";
	}
	bRejected = false;
	try
	{
		(void)Hyperion::LoadSettings("test-invalid.json");
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	{
		std::ofstream File("test-defaults.json");
		File << R"({"type":"hyperion.application-settings","schema_version":1,"properties":{"width":800}})";
	}
	HYP_CHECK(Hyperion::LoadSettings("test-defaults.json").bReversedZ);
	HYP_CHECK(Hyperion::LoadSettings("test-defaults.json").bClusteredLighting);
	HYP_CHECK(Hyperion::LoadSettings("test-defaults.json").Height == Hyperion::FAppSettings{}.Height);
}

void CheckFrameLeadValidation()
{
	for (const std::string Key : {"main_render_lead", "render_rhi_lead"})
	{
		for (const std::string Value : {"-1", "17", "1.5", "999999999999999999999", "\"2\""})
		{
			const auto Text =
			    std::string(R"({"type":"hyperion.application-settings","schema_version":1,"properties":{")") + Key +
			    "\":" + Value + "}}";
			std::ofstream("test-frame-limit.json") << Text;
			bool bRejected{};
			try
			{
				Hyperion::LoadSettings("test-frame-limit.json");
			}
			catch (const std::exception&)
			{
				bRejected = true;
			}
			HYP_CHECK(bRejected);
		}
	}
	const auto Defaults = Hyperion::LoadSettings("test-defaults.json");
	HYP_CHECK(Defaults.MainRenderLead == 1 && Defaults.RenderRhiLead == 1);
}

void CheckSettingsCompatibility()
{
	using namespace Hyperion;
	const std::array<std::string_view, 32> Keys{"title",
	                                            "width",
	                                            "height",
	                                            "workers",
	                                            "rhi_threads",
	                                            "main_render_lead",
	                                            "render_rhi_lead",
	                                            "rhi_backend",
	                                            "render_pipeline",
	                                            "clustered_lighting",
	                                            "contact_shadows",
	                                            "contact_shadow_length",
	                                            "contact_shadow_thickness",
	                                            "contact_shadow_bias",
	                                            "contact_shadow_steps",
	                                            "contact_shadow_debug",
	                                            "hierarchical_depth_mip",
	                                            "reversed_z",
	                                            "gbuffer_layout",
	                                            "exposure",
	                                            "gbuffer_debug",
	                                            "vsync",
	                                            "show_gui",
	                                            "renderdoc_library",
	                                            "renderdoc_output",
	                                            "renderdoc_auto_open",
	                                            "triangle_scale",
	                                            "clear_red",
	                                            "clear_green",
	                                            "clear_blue",
	                                            "disabled_plugins",
	                                            "plugins"};
	const auto& Legacy = SettingsType();
	const auto& Record = RecordType<FAppSettings>();
	HYP_CHECK(Legacy.Id == "hyperion.application-settings" && Legacy.Version == 1);
	HYP_CHECK(Record.Id == "hyperion.applicationsettings.values" && Record.Version == 1);
	HYP_CHECK(Legacy.Properties.size() == Keys.size() && Record.Members.size() == Keys.size());
	for (std::size_t Index = 0; Index < Keys.size(); ++Index)
	{
		HYP_CHECK(Legacy.Properties[Index].Id == Keys[Index] && Record.Members[Index].Id == Keys[Index]);
		HYP_CHECK(Record.Members[Index].Options.bRequired && Record.Members[Index].Options.bPersistent);
	}
	HYP_CHECK(Legacy.Properties[8].Kind == EPropertyKind::String);
	HYP_CHECK(Legacy.Properties[18].Kind == EPropertyKind::String);
	HYP_CHECK(Legacy.Properties[20].Kind == EPropertyKind::Integer);
	HYP_CHECK(Legacy.Properties[20].Minimum == 0 && Legacy.Properties[20].Maximum == 6);
	FAppSettings Settings;
	HYP_CHECK(Settings.RenderPipeline == "deferred" && Settings.GBufferLayout == "compact" &&
	          Settings.GBufferDebug == 0);
	FArchiveNode::FObject Examples;
	for (const std::string Pipeline : {"deferred", "forward"})
	{
		for (const std::string Preset : {"compact", "high"})
		{
			Settings.RenderPipeline = Pipeline;
			Settings.GBufferLayout = Preset;
			Settings.GBufferDebug = 6;
			const auto Wire = WriteRecordWire(Record, &Settings);
			const auto Loaded = std::static_pointer_cast<FAppSettings>(ReadRecordWire(Record, Wire));
			HYP_CHECK(EqualAppSettings(Settings, *Loaded));
			Examples.emplace(Pipeline + "/" + Preset, FArchiveNode(FArchiveNode::FObject{
			                                              {"legacy", ParseJson(EncodeReflected(Legacy, &Settings))},
			                                              {"record", WriteRecord(Record, &Settings)},
			                                              {"wire", Wire}}));
		}
	}
	const FAppSettings Defaults;
	const FArchiveNode Snapshot(FArchiveNode::FObject{{"schema", RecordWireSchema(Record)},
	                                                  {"defaults", WriteRecordWire(Record, &Defaults)},
	                                                  {"examples", FArchiveNode(std::move(Examples))}});
	std::ofstream File("render-option-config-contract.json", std::ios::binary);
	File << WriteJson(Snapshot);
	HYP_CHECK(File.good());
}

} // namespace

int main()
{
	try
	{
		CheckSettingsPersistence();
		CheckFrameLeadValidation();
		CheckSettingsCompatibility();
		CheckRasterOptionSettings();
		bool bRejected = false;
		std::vector<int> Events;
		Hyperion::FPluginRegistry Registry;
		Registry.Add({"base",
		              {},
		              [&]
		              {
			              return std::make_unique<FProbe>(Events, 1);
		              }});
		Registry.Add({"dependent",
		              {"base"},
		              [&]
		              {
			              return std::make_unique<FProbe>(Events, 2);
		              }});
		const std::array Requested{std::string("dependent")};
		{
			auto Plugins = Registry.Activate(Requested);
			HYP_CHECK((Events == std::vector<int>{1, 2}));
		}
		HYP_CHECK((Events == std::vector<int>{1, 2, -2, -1}));
		Registry.Add({"failure",
		              {"base"},
		              [&]
		              {
			              return std::make_unique<FProbe>(Events, 3, true);
		              }});
		Events.clear();
		bRejected = false;
		try
		{
			(void)Registry.Activate(std::array{std::string("failure")});
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected && (Events == std::vector<int>{1, 3, -3, -1}));
		Registry.Add({"cycle-a",
		              {"cycle-b"},
		              [&]
		              {
			              return std::make_unique<FProbe>(Events, 4);
		              }});
		Registry.Add({"cycle-b",
		              {"cycle-a"},
		              [&]
		              {
			              return std::make_unique<FProbe>(Events, 5);
		              }});
		bRejected = false;
		try
		{
			(void)Registry.Activate(std::array{std::string("cycle-a")});
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
		bRejected = false;
		try
		{
			(void)Registry.Activate(std::array{std::string("missing")});
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
		std::cout << "Configuration and plugins passed\n";
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
