#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::CommitEdits(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision,
                                std::uint64_t InInteraction)
{
	if (!InInteraction)
	{
		FinishInspectorEdit();
	}
	SceneDocument.CommitEdits(std::move(InEdits), InExpectedRevision, InInteraction);
	Error.clear();
}
} // namespace Hyperion
