#include "Hyperion/Reflection/Record.h"
#include "Support/TestSupport.h"
#include <cmath>
#include <limits>

namespace
{
using namespace Hyperion;

template<class T>
void CheckInspection(T InValue, std::optional<double> InMinimum, std::optional<double> InMaximum, bool bInAccepted,
                     bool bInChoices = false)
{
	struct FFixture
	{
		T Value{};
	};

	auto Options = Inspect("Value", InMinimum, InMaximum);
	if (bInChoices)
	{
		Options.Inspector->Choices = {"First", "Second", "Third"};
	}
	const auto Type = MakeRecord<FFixture>("test.inspection", {Member("value", &FFixture::Value, Options)});
	const FFixture Source;
	FFixture Candidate;
	FRecordDraft Draft(Type, &Source);
	Draft.GetValues().at("value") = FArchiveNode(InValue);
	bool bRejected{};
	try
	{
		Draft.ApplyToCandidate(&Candidate);
	}
	catch (const std::invalid_argument& Error)
	{
		HYP_CHECK(std::string_view(Error.what()).find("test.inspection.value") != std::string_view::npos);
		bRejected = true;
	}
	HYP_CHECK(bRejected != bInAccepted);
	HYP_CHECK(Source.Value == T{});
	if (bInAccepted)
	{
		HYP_CHECK(Candidate.Value == InValue);
		const auto Restored = std::static_pointer_cast<FFixture>(ReadRecord(Type, WriteRecord(Type, &Candidate)));
		HYP_CHECK(Restored->Value == InValue);
	}
	else
	{
		HYP_CHECK(Candidate.Value == T{});
	}
}

template<class T> void CheckPositiveIntegerInspection()
{
	constexpr T ExactLimit = T{1} << 53;
	constexpr double Limit = 0x1p53;
	CheckInspection<T>(ExactLimit + 1, {}, Limit, false);
	CheckInspection<T>(ExactLimit, Limit, Limit, true);
	CheckInspection<T>(ExactLimit - 1, Limit, {}, false);
	CheckInspection<T>(ExactLimit + 1, Limit, {}, true);
	CheckInspection<T>(ExactLimit + 1, {}, Limit + 2, true);
	CheckInspection<T>(ExactLimit + 1, Limit + 2, {}, false);
	constexpr T Maximum = std::numeric_limits<T>::max();
	const double UpperExclusive = std::ldexp(1.0, std::numeric_limits<T>::digits);
	CheckInspection<T>(Maximum, {}, {}, true);
	CheckInspection<T>(Maximum, UpperExclusive, {}, false);
	CheckInspection<T>(Maximum, {}, UpperExclusive, true);
	CheckInspection<T>(Maximum, {}, std::nextafter(UpperExclusive, 0.0), false);
	CheckInspection<T>(Maximum, std::nextafter(UpperExclusive, 0.0), {}, true);
	CheckInspection<T>(0, -0.5, 0.5, true);
	CheckInspection<T>(0, std::numeric_limits<double>::denorm_min(), {}, false);
	CheckInspection<T>(0, {}, -std::numeric_limits<double>::denorm_min(), false);
	CheckInspection<T>(1, 0.5, 1.5, true);
	CheckInspection<T>(0, 0.5, {}, false);
	CheckInspection<T>(2, {}, 1.5, false);
	CheckInspection<T>(Maximum, -std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), true);
	CheckInspection<T>(0, std::numeric_limits<double>::max(), {}, false);
	CheckInspection<T>(0, {}, -std::numeric_limits<double>::max(), false);
	CheckInspection<T>(0, {}, {}, true, true);
	CheckInspection<T>(2, {}, {}, true, true);
	CheckInspection<T>(3, {}, {}, false, true);
	CheckInspection<T>(Maximum, {}, {}, false, true);
}

void CheckNegativeIntegerInspection()
{
	constexpr std::int64_t ExactLimit = -(std::int64_t{1} << 53);
	constexpr double Limit = -0x1p53;
	CheckInspection<std::int64_t>(ExactLimit - 1, Limit, {}, false);
	CheckInspection<std::int64_t>(ExactLimit, Limit, Limit, true);
	CheckInspection<std::int64_t>(ExactLimit + 1, {}, Limit, false);
	CheckInspection<std::int64_t>(ExactLimit - 1, {}, Limit, true);
	CheckInspection<std::int64_t>(ExactLimit - 1, Limit - 2, {}, true);
	CheckInspection<std::int64_t>(ExactLimit - 1, {}, Limit - 2, false);
	constexpr auto Minimum = std::numeric_limits<std::int64_t>::lowest();
	CheckInspection<std::int64_t>(Minimum, {}, {}, true);
	CheckInspection<std::int64_t>(Minimum, -0x1p63, -0x1p63, true);
	CheckInspection<std::int64_t>(Minimum + 1, {}, -0x1p63, false);
	CheckInspection<std::int64_t>(Minimum, {}, std::nextafter(-0x1p63, -0x1p64), false);
	CheckInspection<std::int64_t>(Minimum, std::nextafter(-0x1p63, 0.0), {}, false);
	CheckInspection<std::int64_t>(Minimum, std::nextafter(-0x1p63, -0x1p64), {}, true);
	CheckInspection<std::int64_t>(-1, -1.5, -0.5, true);
	CheckInspection<std::int64_t>(-2, -1.5, {}, false);
	CheckInspection<std::int64_t>(0, {}, -0.5, false);
	CheckInspection<std::int64_t>(-1, {}, {}, false, true);
}

void CheckFloatingInspection()
{
	CheckInspection(0.0, {}, {}, true);
	CheckInspection(1.25, 1.25, 2.5, true);
	CheckInspection(2.5, 1.25, 2.5, true);
	CheckInspection(std::nextafter(1.25, 0.0), 1.25, 2.5, false);
	CheckInspection(std::nextafter(2.5, 3.0), 1.25, 2.5, false);
	CheckInspection(-1.25, -2.5, -0.5, true);
	CheckInspection(std::numeric_limits<double>::max(), {}, {}, true);
	CheckInspection(std::numeric_limits<double>::quiet_NaN(), {}, {}, false);
	CheckInspection(std::numeric_limits<double>::infinity(), {}, {}, false);
	CheckInspection(-std::numeric_limits<double>::infinity(), {}, {}, false);
	CheckInspection(0.0, {}, {}, true, true);
	CheckInspection(2.0, {}, {}, true, true);
	CheckInspection(-1.0, {}, {}, false, true);
	CheckInspection(3.0, {}, {}, false, true);
}
} // namespace

void CheckInspectionRanges()
{
	CheckPositiveIntegerInspection<std::int64_t>();
	CheckPositiveIntegerInspection<std::uint64_t>();
	CheckNegativeIntegerInspection();
	CheckFloatingInspection();
}
