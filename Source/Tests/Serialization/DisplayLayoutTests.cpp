#include "Hyperion/Reflection/Record.h"
#include "Support/TestSupport.h"

namespace Hyperion
{
struct FDisplaySource
{
	double Stored = 2;
	std::string Hidden = "retained";
};

struct FDisplayView
{
	double Percent{};
	std::string Identity = "readonly";
};

template<> const FRecordDescriptor& RecordType<FDisplayView>()
{
	static const auto Type = MakeRecord<FDisplayView>(
	    "test.display-view", {Member("percent", &FDisplayView::Percent, Inspect("Percent", 0, 1000)),
	                          Member("identity", &FDisplayView::Identity, Inspect("Identity", {}, {}, true))});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FDisplaySource>()
{
	static const auto Type = []
	{
		auto Result = MakeRecord<FDisplaySource>(
		    "test.display-source",
		    {Member("stored", &FDisplaySource::Stored), Member("hidden", &FDisplaySource::Hidden)}, 1,
		    [](const FDisplaySource& InValue)
		    {
			    if (InValue.Stored > 5)
			    {
				    throw std::invalid_argument("Source limit");
			    }
		    });
		Result.DisplayLayout = MakeRecordDisplayLayout<FDisplaySource, FDisplayView>(
		    [](const FDisplaySource& InValue)
		    {
			    return FDisplayView{InValue.Stored * 100};
		    },
		    [](FDisplaySource& InTarget, const FDisplayView& InEdited, const FDisplayView& InOriginal)
		    {
			    HYP_CHECK(InEdited.Identity == InOriginal.Identity);
			    if (InEdited.Percent != InOriginal.Percent)
			    {
				    InTarget.Stored = InEdited.Percent / 100;
			    }
		    });
		return Result;
	}();
	return Type;
}
} // namespace Hyperion

void CheckDisplayLayouts()
{
	using namespace Hyperion;
	const FDisplaySource Source;
	FRecordDraft Draft(RecordType<FDisplaySource>(), &Source);
	HYP_CHECK(ReadValue<double>(Draft.GetValues().at("percent")) == 200);
	Draft.GetValues().at("percent") = WriteValue(350.0);
	Draft.GetValues().at("identity") = WriteValue(std::string("forged"));
	auto Candidate = Source;
	Draft.ApplyToCandidate(&Candidate);
	HYP_CHECK(Candidate.Stored == 3.5 && Candidate.Hidden == "retained" && Source.Stored == 2);
	HYP_CHECK(ReadValue<FDisplaySource>(WriteValue(Candidate)).Stored == 3.5);
	for (const double Invalid : {-1.0, 600.0})
	{
		Draft.GetValues().at("percent") = WriteValue(Invalid);
		bool bRejected{};
		try
		{
			Draft.ApplyToCandidate(&Candidate);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected && Source.Stored == 2);
	}
	FRecordRegistry Registry;
	Registry.Register<FDisplaySource>();
	auto Conflict = RecordType<FDisplaySource>();
	Conflict.DisplayLayout.reset();
	bool bRejected{};
	try
	{
		Registry.Register(Conflict);
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
