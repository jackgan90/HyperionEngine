#pragma once
#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Plugins/PluginRuntime.h"
#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Renderer/RenderPrimitive.h"
#include "Hyperion/Renderer/SceneFrame.h"

namespace Hyperion
{
class FGui;
class FSceneInstance;

struct FRenderFrame
{
	FSize Size;
	FAppSettings Settings;
	FRenderView View;
	std::optional<FSceneViewRequest> SceneView;
	float DeltaSeconds = 1.f / 60.f;
};

// Scene producers run Start, Update and Stop on Main. The session renders their primitives.
class IScenePlugin : public FPlugin
{
public:
	void Start(FPluginContext& InContext) override;
	virtual void Update(FRenderFrame& InFrame) = 0;

	virtual void Input(std::span<const FInputEvent>, bool, bool)
	{
	}

	virtual bool HasScene() const
	{
		return false;
	}

	virtual FSceneInstance& GetSceneInstance();
	virtual bool Ready() const;
	virtual const std::string& Status() const;
	virtual const std::string& Error() const;
};

// Optional editing capability; the producer remains usable without a GUI consumer.
class ISceneEditor : public IScenePlugin
{
public:
	void Start(FPluginContext& InContext) override;
	virtual void SetCullingMode(ESceneCullingMode InMode) = 0;
	virtual void SetRenderedView(const std::optional<FRenderView>& InView) = 0;
	virtual void DrawGui(FGui& InGui, const FSceneVisibilityStats& InStats, bool bInForceOrdinary,
	                     const FLocalLightStatistics& InLights) = 0;
	virtual TAsyncResult<bool> SaveAsync(const std::filesystem::path& InPath) = 0;
};

// Non-scene passes are built on Render; Start and Stop remain on Main.
class IRenderPlugin : public FPlugin
{
public:
	virtual void Build(FRenderGraph& InGraph, const FRenderFrame& InFrame) = 0;
};

} // namespace Hyperion
