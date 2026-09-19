#include "Hyperion/Renderer/RenderPlugin.h"
#include <stdexcept>

namespace Hyperion
{
void IScenePlugin::Start(FPluginContext& InContext)
{
	static_cast<FPlugin&>(*this).Start();
	InContext.Provide<IScenePlugin>(*this);
}

void ISceneEditor::Start(FPluginContext& InContext)
{
	IScenePlugin::Start(InContext);
	InContext.Provide<ISceneEditor>(*this);
}

FSceneInstance& IScenePlugin::GetSceneInstance()
{
	throw std::logic_error("This scene producer has no logical scene");
}

bool IScenePlugin::Ready() const
{
	return true;
}

const std::string& IScenePlugin::Status() const
{
	static const std::string Value = "Ready";
	return Value;
}

const std::string& IScenePlugin::Error() const
{
	static const std::string Value;
	return Value;
}
} // namespace Hyperion
