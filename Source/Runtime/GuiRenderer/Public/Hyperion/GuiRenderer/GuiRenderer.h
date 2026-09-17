#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Renderer/RenderResources.h"

namespace Hyperion
{
// Texture ID 1 is reserved for the font. Sources and scope owners are retained per submitted frame.
// The reuse cache also retains the latest prepared frame's bounded set of image leases until replacement or Stop.
// Images contain display-encoded RGB sampled through an ordinary UNORM view, matching GUI vertex colors.
struct FGuiTextureBinding
{
	std::uint64_t Id{};
	FRenderTargetSource Source;
};

class FGuiRenderer
{
public:
	FGuiRenderer(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont,
	             FRenderResourceService* InResources = nullptr);
	~FGuiRenderer();
	void Start();
	void Stop() noexcept;
	void Prepare(const FGuiDrawData& InData);
	void Build(FRenderGraph& InGraph);
	void BuildDeferred(FRenderGraph& InGraph, FGuiDrawData InData, std::vector<FGuiTextureBinding> InTextures = {},
	                   bool bInClear = false);

private:
	struct FImpl;
	std::shared_ptr<FImpl> Impl;
};
} // namespace Hyperion
