#include "Hyperion/Config/ApplicationSettings.h"
#include "Hyperion/Core/Profiling.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
struct FProfilingState
{
	bool bCompiled{};
	bool bConnected{};
	std::uint32_t Mask{};
	std::uint32_t Sampling{};
};

struct FProfilingEdit
{
	std::optional<std::uint32_t> Mask;
	std::optional<bool> Sampling;
};

FProfilingState State()
{
	const auto Current = GetProfilingStatus();
	return {Current.bCompiled, Current.bConnected, Current.Mask, static_cast<std::uint32_t>(Current.Sampling)};
}
} // namespace

template<> const FRecordDescriptor& RecordType<FProfilingState>()
{
	static const auto Type = MakeRecord<FProfilingState>(
	    "automation.profiling.state",
	    {Member("compiled", &FProfilingState::bCompiled), Member("connected", &FProfilingState::bConnected),
	     Member("mask", &FProfilingState::Mask),
	     Member(
	         "sampling", &FProfilingState::Sampling,
	         {.Description = "0 disabled, 1 requested, 2 unavailable. Requested does not prove collector sampling."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FProfilingEdit>()
{
	static const auto Type = MakeRecord<FProfilingEdit>(
	    "automation.profiling.edit", {Member("mask", &FProfilingEdit::Mask,
	                                         {.Description = "Bits: frame 1, render 2, material 4, RHI 8, tasks 16, "
	                                                         "assets 32, detail 64, GPU 128. Zero disables."}),
	                                  Member("sampling", &FProfilingEdit::Sampling)});
	return Type;
}

void RegisterProfilingOperations(FOperationCatalog& InCatalog, IApplicationSettings* InSettings)
{
	FOperationInfo Info;
	Info.Id = "profiling.get";
	Info.Owner = "automation-scene";
	Info.Summary = "Read target profiler build, connection and capture mask";
	Info.Description = "Uses the same Core profiler as Viewer diagnostics. Collector connection and sampling "
	                   "availability remain platform/build dependent.";
	Info.Effects = "Reads profiler status.";
	Info.Completion = "Current target process snapshot.";
	Info.bReadOnly = true;
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FProfilingState>(Info,
	                                                                     [](const auto&)
	                                                                     {
		                                                                     return State();
	                                                                     }));
	Info.Id = "profiling.set";
	Info.Summary = "Change Viewer profiling categories and sampling request";
	Info.Description = "Shares the Viewer GUI control path, including draining submitted frames before changing the "
	                   "category mask. Omitted fields retain their value. Does not launch an external collector.";
	Info.Effects = "Changes target profiling only; no scene or asset mutation.";
	Info.bReadOnly = false;
	Info.Unavailable = !InSettings                       ? "Host has no profiling controls."
	                   : !GetProfilingStatus().bCompiled ? "Profiling is not compiled in this build."
	                                                     : "";
	const FProfilingEdit Example{ProfileBasicMask, {}};
	Info.Example = WriteRecordWire(RecordType<FProfilingEdit>(), &Example);
	InCatalog.Register(MakeOperation<FProfilingEdit, FProfilingState>(Info,
	                                                                  [InSettings](const auto& InRequest)
	                                                                  {
		                                                                  InSettings->ChangeProfiling(
		                                                                      InRequest.Mask, InRequest.Sampling);
		                                                                  return State();
	                                                                  }));
}
} // namespace Hyperion
