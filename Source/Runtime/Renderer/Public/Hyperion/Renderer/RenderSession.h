#pragma once
#include "Hyperion/Renderer/RenderResources.h"
#include "Hyperion/Renderer/RenderScene.h"

namespace Hyperion
{
// Main constructs/closes the session; Render builds its scene passes.
class FRenderSession
{
public:
	FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler);
	~FRenderSession();
	FRenderSession(const FRenderSession&) = delete;
	FRenderSession& operator=(const FRenderSession&) = delete;
	FRenderSceneClient& GetScene();
	FRenderResourceService& GetResources();
	std::size_t Build(FRenderGraph& InGraph, FRenderView InView);
	void Close();

private:
	FTaskSystem& Tasks;
	FRenderResourceService Resources;
	FRenderSceneClient Scene;
	bool bClosed{};
};
} // namespace Hyperion
