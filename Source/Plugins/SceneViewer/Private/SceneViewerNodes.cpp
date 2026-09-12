#include "SceneViewerInternal.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
bool ChooseNode(FGui& InGui, const char* InLabel, const FSceneInstance& InScene,
                std::span<const FSceneHandle> InHandles, std::optional<FSceneHandle>& InSelection)
{
	std::vector<std::string> Choices{"None"};
	std::size_t Index = 0;
	for (const auto Handle : InHandles)
	{
		const auto& Node = *InScene.FindNode(Handle);
		Choices.push_back(Node.Name + " [" + Node.Id + "]");
		if (InSelection == Handle)
		{
			Index = Choices.size() - 1;
		}
	}
	if (!InGui.Combo(InLabel, Choices, Index))
	{
		return false;
	}
	InSelection = Index ? std::optional<FSceneHandle>{InHandles[Index - 1]} : std::nullopt;
	return true;
}
} // namespace

void FSceneViewerPlugin::FImpl::DrawSceneSettings(FGui& InGui)
{
	auto Settings = Scene.GetSettings();
	bool bChanged =
	    ChooseNode(InGui, "Default camera", Scene, Scene.GetNodes(ESceneNodeKind::Camera), Settings.DefaultCamera);
	bChanged |= ChooseNode(InGui, "Main directional light", Scene, Scene.GetNodes(ESceneNodeKind::DirectionalLight),
	                       Settings.MainDirectionalLight);
	bChanged |= ChooseNode(InGui, "Environment light", Scene, Scene.GetNodes(ESceneNodeKind::EnvironmentLight),
	                       Settings.EnvironmentLight);
	if (bChanged)
	{
		Scene.SetSettings(Settings);
	}
	InGui.TextWrapped("Only the selected main and environment lights contribute. No default camera means clear + GUI.");
}

void FSceneViewerPlugin::FImpl::DrawNodeProperties(FGui& InGui)
{
	const auto Source = Scene.FindNode(Selected);
	if (!Source)
	{
		Selected = {};
		return;
	}
	auto Node = *Source;
	if (PropertySelection != Selected)
	{
		PropertySelection = Selected;
		ProposedParent = Node.Parent.empty() ? std::optional<FSceneHandle>{} : Scene.FindHandle(Node.Parent);
	}
	InGui.Text(std::string(ToString(Node.GetKind())) + " | " + Node.Id);
	if (InGui.InputText("Name (Enter)", Node.Name))
	{
		Scene.SetName(Selected, Node.Name);
	}
	if (InGui.Checkbox("Enabled", Node.bEnabled))
	{
		Scene.SetEnabled(Selected, Node.bEnabled);
	}
	if (InGui.InputMatrix("Local affine transform (Enter)", Node.Local))
	{
		Scene.SetLocalTransform(Selected, Node.Local);
	}
	ChooseNode(InGui, "New parent", Scene, Scene.GetNodes(), ProposedParent);
	InGui.Checkbox("Keep world on reparent", bKeepWorld);
	if (InGui.Button("Apply parent"))
	{
		Scene.Reparent(Selected, ProposedParent,
		               bKeepWorld ? ESceneReparentMode::KeepWorld : ESceneReparentMode::KeepLocal);
	}
	if (Node.Model && InGui.Checkbox("Model visible", Node.Model->bVisible))
	{
		Scene.SetModelVisible(Selected, Node.Model->bVisible);
	}
	if (Node.Camera)
	{
		auto Camera = *Node.Camera;
		bool bChanged = InGui.InputFloat("Vertical FOV (radians)", Camera.VerticalRadians);
		bChanged |= InGui.InputFloat("Near", Camera.Near);
		bChanged |= InGui.InputFloat("Far", Camera.Far);
		bChanged |= InGui.InputFloat("Focus distance", Camera.FocusDistance);
		if (bChanged)
		{
			Scene.SetCamera(Selected, Camera);
		}
	}
	if (Node.DirectionalLight)
	{
		auto Light = *Node.DirectionalLight;
		bool bChanged = InGui.InputVector("Directional color", Light.Color);
		bChanged |= InGui.InputFloat("Directional intensity", Light.Intensity);
		bChanged |= InGui.Checkbox("Cast shadows", Light.bCastShadows);
		if (bChanged)
		{
			Scene.SetDirectionalLight(Selected, Light);
		}
	}
	if (Node.EnvironmentLight)
	{
		auto Light = *Node.EnvironmentLight;
		bool bChanged = InGui.InputVector("Environment color", Light.Color);
		bChanged |= InGui.InputFloat("Environment intensity", Light.Intensity);
		if (bChanged)
		{
			Scene.SetEnvironmentLight(Selected, Light);
		}
	}
	if (InGui.Button("Duplicate node [Insert]"))
	{
		Owner->DuplicateSelected();
	}
	if (InGui.Button("Remove subtree [Delete]"))
	{
		Owner->RemoveSelected();
	}
	if (InGui.Button("Remove node, keep children world"))
	{
		Scene.RemoveNodeKeepChildren(Selected);
		Selected = {};
	}
	if (InGui.Button("Move X -2"))
	{
		Owner->MoveSelected(-2);
	}
	if (InGui.Button("Move X +2"))
	{
		Owner->MoveSelected(2);
	}
	InGui.Checkbox("Animate selected across view", bAnimate);
}

void FSceneViewerPlugin::FImpl::DrawNodes(FGui& InGui)
{
	try
	{
		DrawSceneSettings(InGui);
		std::vector<std::pair<FSceneHandle, unsigned>> Pending;
		const auto Roots = Scene.GetRoots();
		for (auto It = Roots.rbegin(); It != Roots.rend(); ++It)
		{
			Pending.emplace_back(*It, 0);
		}
		InGui.BeginScrollRegion("Scene nodes", 200);
		try
		{
			while (!Pending.empty())
			{
				const auto [Handle, Depth] = Pending.back();
				Pending.pop_back();
				const auto& Node = *Scene.FindNode(Handle);
				const auto Label = std::string(ToString(Node.GetKind())) + ": " + Node.Name + " [" + Node.Id + "]";
				if (InGui.Selectable(Label.c_str(), Selected == Handle, Depth))
				{
					Selected = Handle;
				}
				const auto Children = Scene.GetChildren(Handle);
				for (auto It = Children.rbegin(); It != Children.rend(); ++It)
				{
					Pending.emplace_back(*It, Depth + 1);
				}
			}
		}
		catch (...)
		{
			InGui.EndScrollRegion();
			throw;
		}
		InGui.EndScrollRegion();
		InGui.Text("Nodes " + std::to_string(Scene.GetNodes().size()) + " | Models " +
		           std::to_string(Scene.GetModels().size()));
		if (InGui.Button("Add group"))
		{
			FSceneNode Node;
			Node.Name = "Group";
			Selected = Scene.AddNode(std::move(Node));
		}
		if (InGui.Button("Add camera"))
		{
			auto Node = MakeSceneCameraNode({});
			Node.Name = "Camera";
			Selected = Scene.AddNode(std::move(Node));
		}
		if (InGui.Button("Add directional light"))
		{
			auto Node = MakeSceneDirectionalLightNode({});
			Node.Name = "Directional light";
			Selected = Scene.AddNode(std::move(Node));
		}
		if (InGui.Button("Add environment light"))
		{
			auto Node = MakeSceneEnvironmentLightNode({});
			Node.Name = "Environment light";
			Selected = Scene.AddNode(std::move(Node));
		}
		if (InGui.Button("Add loaded model"))
		{
			Owner->AddModel();
		}
		DrawNodeProperties(InGui);
	}
	catch (const std::exception& Failure)
	{
		EditError = Failure.what();
	}
	if (!EditError.empty())
	{
		InGui.TextWrapped("Edit failed: " + EditError);
		if (InGui.Button("Dismiss edit error"))
		{
			EditError.clear();
		}
	}
}
} // namespace Hyperion
