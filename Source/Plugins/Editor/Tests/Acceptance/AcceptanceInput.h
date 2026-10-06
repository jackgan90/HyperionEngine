#pragma once
#if !HYP_BUILD_TESTING
#error Acceptance input is only available in test-enabled builds
#endif
#include "AcceptanceTransition.h"
#include <cstdint>

namespace Hyperion
{
// Start/Restart do not consume an update. Advance completes on the budget's last eligible update;
// ConsumeFrame instead keeps that update idle, for callers that resume on the following update.
class FAcceptanceFrameWait
{
public:
	explicit FAcceptanceFrameWait(std::uint32_t InFrames = 0) : Budget(InFrames), Remaining(InFrames)
	{
	}

	void Start(std::uint32_t InFrames)
	{
		Budget = InFrames;
		Restart();
	}

	void Restart()
	{
		Remaining = Budget;
	}

	bool Advance()
	{
		ConsumeFrame();
		return Remaining == 0;
	}

	bool ConsumeFrame()
	{
		if (Remaining == 0)
		{
			return false;
		}
		--Remaining;
		return true;
	}

private:
	std::uint32_t Budget{};
	std::uint32_t Remaining{};
};

class FAcceptanceFrameObservation
{
public:
	void Advance()
	{
		++ObservedFrames;
	}

	void Restart()
	{
		ObservedFrames = 0;
	}

	bool IsAt(std::uint32_t InFrames) const
	{
		return ObservedFrames == InFrames;
	}

private:
	std::uint32_t ObservedFrames{};
};

enum class EAcceptanceClickDelay
{
	Settle,
	Immediate
};

enum class EAcceptanceClickAction
{
	None,
	Press,
	Release
};

enum class EAcceptanceClickPhase
{
	DelayBeforePress,
	Press,
	Release,
	DelayBeforeRelease
};

class FAcceptanceClick
{
public:
	EAcceptanceClickAction Advance(EAcceptanceClickDelay InDelay)
	{
		bReachedPressBoundary = false;
		if (Progress.IsAny({EAcceptanceClickPhase::DelayBeforePress, EAcceptanceClickPhase::DelayBeforeRelease}))
		{
			const bool bPress = Progress.Is(EAcceptanceClickPhase::DelayBeforePress);
			if (InDelay == EAcceptanceClickDelay::Settle)
			{
				if (Delay.Advance())
				{
					Progress.TransitionTo(bPress ? EAcceptanceClickPhase::Press : EAcceptanceClickPhase::Release);
					bReachedPressBoundary = bPress;
				}
				return EAcceptanceClickAction::None;
			}
			Progress.TransitionTo(bPress ? EAcceptanceClickPhase::Press : EAcceptanceClickPhase::Release);
		}
		return Progress.Is(EAcceptanceClickPhase::Press) ? EAcceptanceClickAction::Press
		                                                 : EAcceptanceClickAction::Release;
	}

	void Pressed()
	{
		Progress.TransitionTo(EAcceptanceClickPhase::Release);
	}

	void Released()
	{
		Progress.TransitionTo(EAcceptanceClickPhase::DelayBeforePress);
		Delay.Restart();
		bReachedPressBoundary = false;
	}

	void RestartDelay()
	{
		Progress.TransitionTo(IsPressed() ? EAcceptanceClickPhase::DelayBeforeRelease
		                                  : EAcceptanceClickPhase::DelayBeforePress);
		Delay.Restart();
	}

	bool IsPressed() const
	{
		return Progress.IsAny({EAcceptanceClickPhase::Release, EAcceptanceClickPhase::DelayBeforeRelease});
	}

	bool ReachedPressBoundary() const
	{
		return bReachedPressBoundary;
	}

private:
	TAcceptanceState<EAcceptanceClickPhase> Progress;
	FAcceptanceFrameWait Delay{2};
	bool bReachedPressBoundary{};
};

enum class EAcceptanceCadencePhase
{
	ReleaseKeys,
	Settle,
	Execute
};

class FAcceptanceInputCadence
{
public:
	EAcceptanceCadencePhase Advance()
	{
		const auto Phase = Progress.GetState();
		switch (Phase)
		{
			case EAcceptanceCadencePhase::ReleaseKeys:
				Progress.TransitionTo(EAcceptanceCadencePhase::Settle);
				break;
			case EAcceptanceCadencePhase::Settle:
				Progress.TransitionTo(EAcceptanceCadencePhase::Execute);
				break;
			case EAcceptanceCadencePhase::Execute:
				Progress.TransitionTo(EAcceptanceCadencePhase::ReleaseKeys);
				break;
		}
		return Phase;
	}

private:
	TAcceptanceState<EAcceptanceCadencePhase> Progress;
};

enum class EAcceptanceTextPhase
{
	SelectAllPress,
	ReleaseAndType,
	AwaitTextCommit,
	ConfirmRelease
};

enum class EAcceptanceTextCommit
{
	KeepEditing,
	Enter
};

struct FAcceptanceTextInput
{
	explicit FAcceptanceTextInput(EAcceptanceTextCommit InCommit = EAcceptanceTextCommit::KeepEditing,
	                              std::uint32_t InSettleFrames = 2)
	    : Commit(InCommit), TextCommit(InSettleFrames)
	{
	}

	TAcceptanceState<EAcceptanceTextPhase> Progress;
	EAcceptanceTextCommit Commit;
	FAcceptanceFrameWait TextCommit;
};
} // namespace Hyperion
