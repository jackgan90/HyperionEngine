#include "Hyperion/Application/ApplicationHost.h"
#include "Support/TestSupport.h"
#include <iostream>

namespace
{
using namespace Hyperion;

struct FNumber
{
	int Value = 7;
};

struct FNotice
{
	int Value{};
};

struct FProbe final : FPlugin
{
	std::function<void(FPluginContext&)> OnStart;
	std::function<void(const FPluginUpdate&)> OnUpdate;

	void Start(FPluginContext& InContext) override
	{
		if (OnStart)
		{
			OnStart(InContext);
		}
	}

	void Update(const FPluginUpdate& InUpdate) override
	{
		if (OnUpdate)
		{
			OnUpdate(InUpdate);
		}
	}
};

template<class F> void Rejects(F InWork)
{
	bool bRejected{};
	try
	{
		InWork();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

FPluginDescriptor Probe(std::string InId, std::function<void(FPluginContext&)> InStart = {})
{
	FPluginDescriptor Result;
	Result.Id = std::move(InId);
	Result.Create = [Start = std::move(InStart)]
	{
		auto Instance = std::make_unique<FProbe>();
		Instance->OnStart = Start;
		return Instance;
	};
	return Result;
}

void CheckServices(bool bInFail)
{
	FPluginRegistry Registry;
	FNumber Number;
	int Notifications{};
	int Cleanups{};
	bool bConsumed{};
	auto Provider = Probe("provider",
	                      [&](FPluginContext& InContext)
	                      {
		                      InContext.Provide(Number);
		                      InContext.Subscribe<FNotice>(
		                          [&](const FNotice& InNotice)
		                          {
			                          Notifications += InNotice.Value;
		                          });
		                      InContext.Defer(
		                          [&]
		                          {
			                          ++Cleanups;
		                          });
		                      if (bInFail)
		                      {
			                      throw std::runtime_error("expected failure after publication");
		                      }
	                      });
	Provider.Provides = {typeid(FNumber)};
	Registry.Add(std::move(Provider));
	auto Consumer = Probe("consumer",
	                      [&](FPluginContext& InContext)
	                      {
		                      HYP_CHECK(!bInFail && InContext.Require<FNumber>().Value == 7);
		                      bConsumed = true;
		                      InContext.Defer(
		                          [&, Service = &InContext.Require<FNumber>()]
		                          {
			                          HYP_CHECK(Cleanups == 0 && Service->Value == 7);
		                          });
	                      });
	Consumer.Requires = {typeid(FNumber)};
	Registry.Add(std::move(Consumer));
	Registry.Add(Probe("independent"));
	auto Services = std::make_shared<FPluginServices>();
	auto Set = Registry.Activate({{"consumer", "provider", "missing", "independent"}}, Services);
	HYP_CHECK(Set.IsActive("independent") && bConsumed != bInFail);
	HYP_CHECK(Set.GetDiagnostics().size() == (bInFail ? 3 : 1));
	Services->Publish(FNotice{5});
	HYP_CHECK(Notifications == (bInFail ? 0 : 5));
	Set.Stop();
	Set.Stop();
	Services->Publish(FNotice{5});
	HYP_CHECK(Notifications == (bInFail ? 0 : 5));
	HYP_CHECK(!Services->Find<FNumber>() && Cleanups == 1);
}

void CheckOrderAndDisable()
{
	FPluginRegistry Registry;
	Registry.Add(Probe("graphics"));
	auto Capture = Probe("capture");
	Capture.Before = {"graphics"};
	Registry.Add(std::move(Capture));
	auto Scene = Probe("scene");
	Scene.Dependencies = {"graphics"};
	Registry.Add(std::move(Scene));
	auto Services = std::make_shared<FPluginServices>();
	{
		auto Set = Registry.Activate({{"scene", "capture"}}, Services);
		HYP_CHECK((Set.GetOrder() == std::vector<std::string>{"capture", "graphics", "scene"}));
	}
	{
		auto Set = Registry.Activate({{"scene", "capture"}, {"graphics"}}, Services);
		HYP_CHECK((Set.GetOrder() == std::vector<std::string>{"capture"}));
		HYP_CHECK(Set.GetDiagnostics().size() == 2);
	}
	{
		auto Set = Registry.Activate({{"scene"}}, Services);
		HYP_CHECK(!Set.IsActive("capture") && Set.IsActive("scene"));
	}
}

void CheckInvalidPlans()
{
	FPluginRegistry Registry;
	auto First = Probe("first");
	First.Before = {"second"};
	Registry.Add(std::move(First));
	auto Second = Probe("second");
	Second.Before = {"first"};
	Registry.Add(std::move(Second));
	Rejects(
	    [&]
	    {
		    Registry.Plan({{"first", "second"}}, FPluginServices{});
	    });
	FPluginRegistry Providers;
	for (const auto* Id : {"one", "two"})
	{
		auto Descriptor = Probe(Id);
		Descriptor.Provides = {typeid(FNumber)};
		Providers.Add(std::move(Descriptor));
	}
	Rejects(
	    [&]
	    {
		    Providers.Plan({{"one", "two"}}, FPluginServices{});
	    });
	FPluginServices Services;
	std::thread WrongThread(
	    [&]
	    {
		    Rejects(
		        [&]
		        {
			        Services.Find<FNumber>();
		        });
	    });
	WrongThread.join();
}

void CheckHost()
{
	{
		FApplicationHost Empty(1, 1);
		Empty.Start({}, {});
		Empty.Run(3);
	}
	FApplicationHost Host(1, 1);
	FPluginRegistry Registry;
	std::uint64_t Frames{};
	bool bWorkCompleted{};
	auto Feature = Probe("feature");
	Feature.Requires = {typeid(FApplicationControl), typeid(FTaskSystem)};
	Feature.CreateWithContext = [&](FPluginContext& InContext)
	{
		auto Instance = std::make_unique<FProbe>();
		auto& Tasks = InContext.Require<FTaskSystem>();
		TrackPluginTask(InContext, Tasks,
		                Tasks.Dispatch({EDomain::Worker},
		                               [&]
		                               {
			                               bWorkCompleted = true;
		                               }));
		Instance->OnUpdate = [&, Control = &InContext.Require<FApplicationControl>()](const FPluginUpdate& InUpdate)
		{
			HYP_CHECK(InUpdate.Frame == Frames && InUpdate.DeltaSeconds >= 0);
			if (++Frames == 4)
			{
				Control->RequestExit();
			}
		};
		return Instance;
	};
	Registry.Add(std::move(Feature));
	Host.Start(Registry, {{"feature"}});
	Host.Run();
	Host.Stop();
	HYP_CHECK(Frames == 4 && bWorkCompleted);
}

void CheckEventStateAndReentrancy()
{
	FPluginServices Services;
	FPluginContext Context(Services, "events", {}, {}, {});
	FPluginContext Removed(Services, "removed", {}, {}, {});
	std::vector<int> Observed;
	int RemovedCalls{};
	int AddedCalls{};
	Context.Subscribe<FNotice>(
	    [&, Count = 0](const FNotice& InNotice) mutable
	    {
		    Observed.push_back(++Count);
		    if (InNotice.Value == 1)
		    {
			    Removed.Close();
			    Context.Subscribe<FNotice>(
			        [&](const FNotice&)
			        {
				        ++AddedCalls;
			        });
			    Context.Publish(FNotice{});
		    }
	    });
	Removed.Subscribe<FNotice>(
	    [&](const FNotice&)
	    {
		    ++RemovedCalls;
	    });
	Context.Publish(FNotice{1});
	Context.Publish(FNotice{});
	HYP_CHECK((Observed == std::vector<int>{1, 2, 3}));
	HYP_CHECK(RemovedCalls == 0 && AddedCalls == 2);
	Context.Close();
	Services.Publish(FNotice{});
	HYP_CHECK(Observed.size() == 3 && AddedCalls == 2);
}

void CheckStrictPreflight()
{
	FPluginRegistry Registry;
	int Constructions{};
	int Starts{};
	auto Valid = Probe("valid");
	Valid.Create = [&]
	{
		++Constructions;
		auto Instance = std::make_unique<FProbe>();
		Instance->OnStart = [&](FPluginContext&)
		{
			++Starts;
		};
		return Instance;
	};
	Registry.Add(std::move(Valid));
	auto Consumer = Probe("consumer");
	Consumer.Dependencies = {"missing"};
	Registry.Add(std::move(Consumer));
	auto ServiceConsumer = Probe("service-consumer");
	ServiceConsumer.Requires = {typeid(FNumber)};
	Registry.Add(std::move(ServiceConsumer));
	for (const auto* Id : {"missing", "consumer", "service-consumer"})
	{
		Rejects(
		    [&]
		    {
			    const std::vector<std::string> Requested{"valid", Id};
			    Registry.Activate(Requested);
		    });
		HYP_CHECK(Constructions == 0 && Starts == 0);
	}
	Rejects(
	    [&]
	    {
		    Registry.Activate({{"valid", "consumer"}, {"consumer"}, EPluginFailurePolicy::Strict},
		                      std::make_shared<FPluginServices>());
	    });
	HYP_CHECK(Constructions == 0 && Starts == 0);
}

void CheckStartupCleanupFailure(bool bInFactory)
{
	FApplicationHost Host(1, 1);
	FPluginRegistry Registry;
	int Cleanups{};
	int Notifications{};
	auto Fail = [&](FPluginContext& InContext)
	{
		InContext.Subscribe<FNotice>(
		    [&](const FNotice&)
		    {
			    ++Notifications;
		    });
		InContext.Defer(
		    [&]
		    {
			    ++Cleanups;
		    });
		InContext.Defer(
		    []
		    {
			    throw std::runtime_error("startup cleanup failed");
		    });
		throw std::runtime_error("startup failed");
	};
	auto Failed = Probe("failed", Fail);
	if (bInFactory)
	{
		Failed.CreateWithContext = [&](FPluginContext& InContext) -> std::unique_ptr<FPlugin>
		{
			Fail(InContext);
			return {};
		};
	}
	Registry.Add(std::move(Failed));
	Registry.Add(Probe("independent"));
	Host.Start(Registry, {{"failed", "independent"}});
	HYP_CHECK(Host.GetPlugins().IsActive("independent"));
	HYP_CHECK(Host.GetPlugins().GetDiagnostics().size() == 1);
	HYP_CHECK(Host.GetPlugins().GetDiagnostics().front().Message == "startup failed");
	Host.GetServices().Publish(FNotice{});
	HYP_CHECK(Cleanups == 1 && Notifications == 0);
	Host.Run(1);
	Host.Stop();
	bool bCleanupReported{};
	try
	{
		Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	}
	catch (const std::runtime_error& Failure)
	{
		bCleanupReported = std::string(Failure.what()) == "startup cleanup failed";
	}
	HYP_CHECK(bCleanupReported);
	Host.Stop();
	HYP_CHECK(Cleanups == 1);
}

void CheckScopeFailures()
{
	FPluginServices Services;
	FNumber Number;
	FPluginContext Context(Services, "scope", {typeid(FNumber)}, {}, {});
	Rejects(
	    [&]
	    {
		    Context.Require<FNumber>();
	    });
	Context.Provide(Number);
	Context.Subscribe<FNotice>(
	    [](const FNotice&)
	    {
	    });
	int Joined{};
	Context.Defer(
	    [&]
	    {
		    ++Joined;
	    });
	Context.Defer(
	    []
	    {
		    throw std::runtime_error("join failure");
	    });
	Context.Drain();
	HYP_CHECK(Joined == 1 && Context.GetCleanupFailure() && Services.Find<FNumber>() == &Number);
	Rejects(
	    [&]
	    {
		    Context.Subscribe<FNotice>(
		        [](const FNotice&)
		        {
		        });
	    });
	Context.Close();
	HYP_CHECK(!Services.Find<FNumber>());
	FApplicationHost Host(1, 1);
	FPluginRegistry Registry;
	Registry.Add(Probe("failing-task",
	                   [](FPluginContext& InContext)
	                   {
		                   InContext.Defer(
		                       []
		                       {
			                       throw std::runtime_error("task failed during shutdown");
		                       });
	                   }));
	Host.Start(Registry, {{"failing-task"}});
	Host.Run(1);
	Host.Stop();
	Rejects(
	    [&]
	    {
		    Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	    });
}
} // namespace

int main()
{
	try
	{
		CheckServices(false);
		CheckServices(true);
		CheckOrderAndDisable();
		CheckInvalidPlans();
		CheckHost();
		CheckEventStateAndReentrancy();
		CheckStrictPreflight();
		CheckStartupCleanupFailure(false);
		CheckStartupCleanupFailure(true);
		CheckScopeFailures();
		std::cout << "Plugin services, isolation, ordering, shutdown and empty application host passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
