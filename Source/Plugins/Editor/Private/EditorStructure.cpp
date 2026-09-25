#include "EditorApplication.h"

namespace Hyperion
{
namespace
{
bool ChooseSceneNode(FGui& InGui, const char* InLabel, const FSceneInstance& InScene,
                     std::span<const FSceneHandle> InHandles, std::optional<FSceneHandle>& InSelection)
{
	std::vector<std::string> Labels{"None"};
	std::size_t Index{};
	for (const auto Handle : InHandles)
	{
		const auto* Node = InScene.FindNode(Handle);
		Labels.push_back(Node->Name + " [" + Node->Id + "]");
		if (InSelection == Handle)
		{
			Index = Labels.size() - 1;
		}
	}
	if (!InGui.Combo(InLabel, Labels, Index))
	{
		return false;
	}
	InSelection = Index ? std::optional{InHandles[Index - 1]} : std::nullopt;
	return true;
}
} // namespace

void FEditorPlugin::DrawHierarchy(const FSceneNodeView& InView)
{
	if (!Gui->Section("Hierarchy", false))
	{
		return;
	}
	const auto Parent = InView.Node->Parent();
	for (const bool bKeepWorld : {true, false})
	{
		auto ParentSelection = Parent.empty() ? std::optional<FSceneHandle>{} : Scene->FindHandle(Parent);
		if (ChooseSceneNode(*Gui, bKeepWorld ? "Parent (keep world)" : "Parent (keep local)", *Scene, Scene->GetNodes(),
		                    ParentSelection))
		{
			try
			{
				FinishInspectorEdit();
				SceneDocument.CommitReparent(InView.Handle, ParentSelection,
				                             bKeepWorld ? ESceneReparentMode::KeepWorld
				                                        : ESceneReparentMode::KeepLocal);
			}
			catch (const std::exception& Failure)
			{
				Error = Failure.what();
			}
			return;
		}
	}
}
} // namespace Hyperion
