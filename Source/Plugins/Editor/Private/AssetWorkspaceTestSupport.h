#pragma once
#include "AssetWorkspace.h"
#include <chrono>
#include <source_location>
#include <thread>

namespace Hyperion::Tests
{
inline void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Asset workspace check failed: ") + InLocation.file_name() + ":" +
		                         std::to_string(InLocation.line()));
	}
}

struct FWorkspaceFixture
{
	FAssetWorkspace Workspace;
	FGui Gui;

	FWorkspaceFixture(FTaskSystem& InTasks, FAssetService& InAssets, FRenderSession& InSession,
	                  FRHICapabilities InCapabilities)
	    : Workspace(InAssets, InTasks, InSession, InCapabilities)
	{
		(void)Gui.FontImage();
	}

	FGuiDrawData Frame(std::span<const FInputEvent> InEvents = {}, bool bInPreview = false)
	{
		Gui.BeginFrame({1200, 1800}, {1200, 1800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Workspace regression", {0, 0}, {1150, 1750});
		if (bInPreview)
		{
			Workspace.DrawTabs(Gui, 1.f / 60, InEvents);
		}
		else
		{
			Workspace.DrawProperties(Gui);
		}
		Gui.EndPanel();
		return Gui.Render();
	}

	template<class Predicate> void Await(const Predicate& InReady, bool bInPreview = false)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
		while (!InReady())
		{
			Workspace.Poll();
			if (bInPreview)
			{
				Frame({}, true);
			}
			if (std::chrono::steady_clock::now() >= Deadline)
			{
				throw std::runtime_error(Workspace.ActiveStatus());
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void Open(const char* InPath)
	{
		Workspace.Open(InPath);
		Await(
		    [&]
		    {
			    return Workspace.ActiveDocument() != nullptr;
		    });
		Frame();
		Frame();
	}

	void Click(FVec4 InBounds)
	{
		Check(InBounds.Z > InBounds.X && InBounds.W > InBounds.Y);
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (InBounds.X + InBounds.Z) * .5f;
		Move.Y = (InBounds.Y + InBounds.W) * .5f;
		Frame(std::array{Move});
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::array{Button});
		Button.bDown = false;
		Frame(std::array{Button});
		Frame();
	}

	void Type(FVec4 InBounds, const char* InText)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Modifiers = InputModifiers::Control;
		Frame(std::array{Key});
		Click(InBounds);
		Key.Key = EKey::A;
		Key.bDown = true;
		Frame(std::array{Key});
		Key.bDown = false;
		Key.Modifiers = 0;
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		Frame(std::array{Key, Text});
		Frame();
	}
};
} // namespace Hyperion::Tests
