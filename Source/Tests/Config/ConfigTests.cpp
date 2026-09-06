#include "Support/TestSupport.h"
#include <Hyperion/Config/AppSettings.h>
#include <Hyperion/Plugins/PluginRuntime.h>
#include <array>
#include <fstream>
#include <iostream>

namespace
{
struct FProbe final : Hyperion::FPlugin
{
	std::vector<int>& Events;
	int Id;
	bool Fail;

	FProbe(std::vector<int>& InE, int InI, bool InF = false) : Events(InE), Id(InI), Fail(InF)
	{
	}

	void Start() override
	{
		Events.push_back(Id);
		if (Fail)
		{
			throw std::runtime_error("expected startup failure");
		}
	}

	void Stop() noexcept override
	{
		Events.push_back(-Id);
	}
};
} // namespace

int main()
{
	try
	{
		Hyperion::FAppSettings Settings;
		Settings.Width = 960;
		Settings.TriangleScale = 0.6;
		Settings.Plugins = {"triangle"};
		Hyperion::SaveSettings("test-settings.json", Settings);
		const auto Loaded = Hyperion::LoadSettings("test-settings.json");
		HYP_CHECK(Loaded.Width == 960 && Loaded.TriangleScale == 0.6 && Loaded.Plugins == Settings.Plugins);
		{
			std::ofstream File("test-invalid.json");
			File
			    << R"({"type":"hyperion.application-settings","schema_version":1,"properties":{"title":"changed","width":-20}})";
		}
		const auto Before = Settings;
		bool Rejected = false;
		try
		{
			Hyperion::LoadReflected("test-invalid.json", Hyperion::SettingsType(), &Settings);
		}
		catch (const std::exception&)
		{
			Rejected = true;
		}
		HYP_CHECK(Rejected && Settings.Width == Before.Width && Settings.Title == Before.Title);
		{
			std::ofstream File("test-invalid.json");
			File << R"({"type":"hyperion.application-settings","schema_version":99,"properties":{}})";
		}
		Rejected = false;
		try
		{
			(void)Hyperion::LoadSettings("test-invalid.json");
		}
		catch (const std::exception&)
		{
			Rejected = true;
		}
		HYP_CHECK(Rejected);
		{
			std::ofstream File("test-defaults.json");
			File << R"({"type":"hyperion.application-settings","schema_version":1,"properties":{"width":800}})";
		}
		HYP_CHECK(Hyperion::LoadSettings("test-defaults.json").Height == Hyperion::FAppSettings{}.Height);
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
		Rejected = false;
		try
		{
			(void)Registry.Activate(std::array{std::string("failure")});
		}
		catch (const std::exception&)
		{
			Rejected = true;
		}
		HYP_CHECK(Rejected && (Events == std::vector<int>{1, 3, -3, -1}));
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
		Rejected = false;
		try
		{
			(void)Registry.Activate(std::array{std::string("cycle-a")});
		}
		catch (const std::exception&)
		{
			Rejected = true;
		}
		HYP_CHECK(Rejected);
		Rejected = false;
		try
		{
			(void)Registry.Activate(std::array{std::string("missing")});
		}
		catch (const std::exception&)
		{
			Rejected = true;
		}
		HYP_CHECK(Rejected);
		std::cout << "Configuration and plugins passed\n";
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
