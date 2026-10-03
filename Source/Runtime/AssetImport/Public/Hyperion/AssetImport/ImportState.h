#pragma once

namespace Hyperion
{
enum class EImportTaskState
{
	Running,
	Completed,
	Failed
};

enum class EImportDraftState
{
	// An empty preview snapshot before a workspace draft has been admitted.
	None,
	Preparing,
	Ready,
	Publishing,
	Failed,
	Discarded
};
} // namespace Hyperion
