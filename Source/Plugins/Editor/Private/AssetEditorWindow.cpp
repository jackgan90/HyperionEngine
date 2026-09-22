#include "AssetEditorWindow.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include <fstream>

namespace Hyperion
{
FAssetEditorWindow::FAssetEditorWindow(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                                       FRenderSession& InSession, FAssetWorkspace& InWorkspace,
                                       FApplicationControl& InControl, std::filesystem::path InLayout)
    : Tasks(InTasks), Device(InDevice), Compiler(InCompiler), Session(InSession), Workspace(InWorkspace),
      Control(InControl), Layout(std::move(InLayout))
{
}

FAssetEditorWindow::~FAssetEditorWindow()
{
	try
	{
		Stop();
	}
	catch (...)
	{
		Control.ReportFailure(std::current_exception());
	}
}

void FAssetEditorWindow::Initialize(FWindow& InOwner, FIOService& InIO, float InScale, bool bInHidden)
{
	Owner = &InOwner;
	bHidden = bInHidden;
	Window = std::make_unique<FWindow>("Hyperion Asset Editor", FSize{1280, 800}, bHidden);
	Window->SetOwner(Owner);
	(void)Window->SetDarkTitleBar(true);
	Gui = std::make_unique<FGui>(Window.get());
	Gui->UseEditorStyle();
	Gui->SetApplicationScale(InScale);
	Gui->SetPathDisplayRoot("/Game");
	const auto Font = InIO.ReadAsync("/Engine/Fonts/RobotoMedium.ttf").Get(Tasks);
	Gui->LoadFont(*Font, 15);
	std::ifstream Stream(Layout, std::ios::binary);
	if (Stream)
	{
		Gui->LoadLayout(std::string{std::istreambuf_iterator<char>(Stream), {}});
	}
	const auto Surface = Window->Surface();
	const auto Size = Window->PixelSize();
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Swapchain = Device.CreateSwapchain({Surface, Size, ERHIDepthFormat::D32, 0.f});
	                          }));
	Renderer = std::make_unique<FGuiRenderer>(Device, Compiler, Tasks, Gui->FontImage(), &Session.GetResources());
	Renderer->Start();
}

void FAssetEditorWindow::Activate()
{
	if (!bHidden)
	{
		if (Owner->Minimized())
		{
			Owner->Restore();
		}
		if (Window->Minimized())
		{
			Window->Restore();
		}
		Window->Raise();
	}
	bShowPreview = bShowProperties = true;
	Gui->FocusWindow("Asset Preview");
}

void FAssetEditorWindow::SaveLayout()
{
	if (Layout.empty() || !Gui)
	{
		return;
	}
	if (Layout.has_parent_path())
	{
		std::filesystem::create_directories(Layout.parent_path());
	}
	std::ofstream Stream(Layout, std::ios::binary);
	Stream << Gui->SaveLayout();
	if (!Stream)
	{
		throw std::runtime_error("Could not save asset editor layout: " + Layout.string());
	}
}

void FAssetEditorWindow::Stop()
{
	try
	{
		SaveLayout();
	}
	catch (const std::exception& Failure)
	{
		Log(ELogLevel::Warning, Failure.what());
	}
	if (Renderer)
	{
		Renderer->Stop();
		Renderer.reset();
	}
	Gui.reset();
	if (Swapchain)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [this]
		                          {
			                          auto Retired = std::move(Swapchain);
			                          Retired->WaitIdle();
		                          }));
	}
	Window.reset();
}

bool FAssetEditorWindow::IsDrawable() const
{
	return Window && !Owner->Minimized() && !Window->Minimized() && Window->PixelSize().Width &&
	       Window->PixelSize().Height;
}

FWindow& FAssetEditorWindow::NativeWindow()
{
	return *Window;
}

FGui& FAssetEditorWindow::GuiContext()
{
	return *Gui;
}

FVec4 FAssetEditorWindow::ObservedBounds(std::string_view InId) const
{
	const auto It = Bounds.find(InId);
	return It == Bounds.end() ? FVec4{} : It->second;
}

std::uint64_t FAssetEditorWindow::RenderedFrames() const
{
	return FrameCount;
}

void FAssetEditorWindow::Render(FGuiDrawData InData, const std::filesystem::path& InCapture, bool bInVsync)
{
	Workspace.PrepareFrame();
	const auto Size = Window->PixelSize();
	FImage Capture;
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&, Data = std::move(InData)]() mutable
	                          {
		                          FRenderGraph Graph;
		                          std::vector<FGuiTextureBinding> Textures;
		                          Workspace.Build(Graph, Textures);
		                          Renderer->BuildDeferred(Graph, std::move(Data), std::move(Textures), true);
		                          Capture = ExecuteGraph(std::move(Graph), Tasks, *Swapchain, Size, bInVsync,
		                                                 !InCapture.empty());
	                          }));
	++FrameCount;
	if (!InCapture.empty())
	{
		if (InCapture.has_parent_path())
		{
			std::filesystem::create_directories(InCapture.parent_path());
		}
		SaveImage(InCapture, Capture);
	}
}
} // namespace Hyperion
