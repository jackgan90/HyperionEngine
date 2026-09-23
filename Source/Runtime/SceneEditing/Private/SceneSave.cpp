#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <utility>

namespace Hyperion
{
TAsyncResult<bool> FSceneEditDocument::Save(std::string InDestination)
{
	if (State.Save)
	{
		throw FSceneEditError("busy", "A scene save is already in progress");
	}
	if (InDestination.empty() || PathFromUtf8(InDestination).extension() != ".hasset")
	{
		throw std::invalid_argument("Choose a .hasset scene destination on the target");
	}
	const auto Started = ClockNanoseconds();
	auto Result = Target().Save(PathFromUtf8(InDestination));
	State.Save = FScenePendingSave{Result, std::move(InDestination), State.Epoch, State.State, Started};
	return Result;
}

void FSceneEditDocument::PollSave()
{
	if (!State.Save || !State.Save->Result.Ready())
	{
		return;
	}
	FSceneSaveOutcome Outcome;
	Outcome.bCurrentDocument = State.Save->Epoch == State.Epoch;
	Outcome.Path = State.Save->Destination;
	Outcome.Milliseconds = double(ClockNanoseconds() - State.Save->Started) / 1e6;
	try
	{
		if (!*State.Save->Result.GetReady())
		{
			throw std::runtime_error("Asset service did not commit the scene");
		}
		Outcome.bSucceeded = true;
		if (Outcome.bCurrentDocument)
		{
			MarkSaved(State.Save->Epoch, State.Save->State);
			State.Path = State.Save->Destination;
		}
	}
	catch (const std::exception& Error)
	{
		Outcome.Error = Error.what();
	}
	State.Save.reset();
	SaveOutcome = std::move(Outcome);
}

std::optional<FSceneSaveOutcome> FSceneEditDocument::TakeSaveOutcome()
{
	return std::exchange(SaveOutcome, {});
}
} // namespace Hyperion
