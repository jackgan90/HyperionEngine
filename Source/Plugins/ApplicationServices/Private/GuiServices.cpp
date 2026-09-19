#include "Hyperion/ApplicationServices/ApplicationServices.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <fstream>

namespace Hyperion
{
namespace
{
class FGuiServicesPlugin final : public FPlugin
{
public:
	explicit FGuiServicesPlugin(FGuiServiceOptions InOptions) : Options(std::move(InOptions))
	{
	}

	void Start(FPluginContext& InContext) override
	{
		auto& Tasks = InContext.Require<FTaskSystem>();
		Gui = std::make_unique<FGui>(&InContext.Require<FWindow>());
		if (Options.bEditorStyle)
		{
			Gui->UseEditorStyle();
		}
		if (!Options.Font.empty())
		{
			const auto Font = InContext.Require<FIOService>().ReadAsync(Options.Font).Get(Tasks);
			Gui->LoadFont(*Font, Options.FontSize);
		}
		if (!Options.Layout.empty())
		{
			std::ifstream Stream(Options.Layout, std::ios::binary);
			if (Stream)
			{
				const std::string Layout{std::istreambuf_iterator<char>(Stream), {}};
				Gui->LoadLayout(Layout);
			}
		}
		Renderer =
		    std::make_unique<FGuiRenderer>(InContext.Require<IRHIDevice>(), InContext.Require<FShaderCompiler>(), Tasks,
		                                   Gui->FontImage(), &InContext.Require<FRenderSession>().GetResources());
		Renderer->Start();
		InContext.Provide(*Gui);
		InContext.Provide(*Renderer);
	}

	void Stop() noexcept override
	{
		if (Renderer)
		{
			Renderer->Stop();
			Renderer.reset();
		}
		Gui.reset();
	}

private:
	FGuiServiceOptions Options;
	std::unique_ptr<FGui> Gui;
	std::unique_ptr<FGuiRenderer> Renderer;
};
} // namespace

void RegisterGuiServices(FPluginRegistry& InRegistry, FGuiServiceOptions InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "gui";
	Descriptor.Dependencies = {"graphics"};
	Descriptor.Requires = {typeid(FTaskSystem), typeid(FWindow),         typeid(FIOService),
	                       typeid(IRHIDevice),  typeid(FShaderCompiler), typeid(FRenderSession)};
	Descriptor.Provides = {typeid(FGui), typeid(FGuiRenderer)};
	Descriptor.Create = [Options = std::move(InOptions)]
	{
		return std::make_unique<FGuiServicesPlugin>(Options);
	};
	InRegistry.Add(std::move(Descriptor));
}
} // namespace Hyperion
