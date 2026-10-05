#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/RenderControls/RenderCaptureControl.h"
#include "Hyperion/RenderControls/RenderDiagnostics.h"
#include "Hyperion/RenderControls/RenderOutput.h"
#include "Hyperion/RenderControls/RenderSettings.h"
#include "Hyperion/RenderControls/SceneLightControls.h"
#include "Hyperion/RenderControls/SceneViewport.h"
#include "Hyperion/RenderControls/ShadowControls.h"
#include <fstream>
#include <iostream>
#include <type_traits>

using namespace Hyperion;

static_assert(std::is_abstract_v<IRenderSettings> && std::is_abstract_v<ISceneViewport> &&
              std::is_abstract_v<IShadowControls> && std::is_abstract_v<ISceneLightControls> &&
              std::is_abstract_v<IRenderOutput> && std::is_abstract_v<IRenderDiagnostics> &&
              std::is_abstract_v<IRenderCaptureControl>);

namespace
{
template<class T> void AddContract(FArchiveNode::FObject& InOutContracts)
{
	const auto& Type = RecordType<T>();
	const T Defaults;
	const auto Wire = WriteRecordWire(Type, &Defaults);
	const auto Restored = ReadRecordWire(Type, Wire);
	if (WriteJson(Wire) != WriteJson(WriteRecordWire(Type, Restored.get())))
	{
		throw std::logic_error("Control contract failed its wire round trip: " + Type.Id);
	}
	InOutContracts.emplace(Type.Id,
	                       FArchiveNode(FArchiveNode::FObject{{"schema", RecordWireSchema(Type)}, {"defaults", Wire}}));
}

FArchiveNode Contracts()
{
	FArchiveNode::FObject Result;
	AddContract<FRenderSettings>(Result);
	AddContract<FRenderSettingsState>(Result);
	AddContract<FCascadedShadowSettings>(Result);
	AddContract<FSceneViewportOptions>(Result);
	AddContract<FSceneViewportState>(Result);
	AddContract<FSceneSkyStatus>(Result);
	AddContract<FSceneLightDiagnostic>(Result);
	AddContract<FSceneLightingInfo>(Result);
	AddContract<FImageOutputRequest>(Result);
	AddContract<FImageArtifact>(Result);
	AddContract<FRenderCaptureInfo>(Result);
	AddContract<FRenderCaptureHudInfo>(Result);
	AddContract<FRenderHealth>(Result);
	AddContract<FRenderDiagnostics>(Result);
	AddContract<FScenePrimitiveDiagnostic>(Result);
	AddContract<FSceneComponentDiagnostics>(Result);
	return FArchiveNode(std::move(Result));
}
} // namespace

int main(int InArgumentCount, char** InArguments)
{
	try
	{
		const auto Value = Contracts();
		const auto Opaque = FImageOutputWindow::FromWire("extension-window");
		if (Opaque.Kind() || Opaque.WireName() != "extension-window")
		{
			throw std::logic_error("Opaque window token changed");
		}
		const FRenderSettings Settings;
		ValidateRenderSettings(Settings);
		ValidateViewportOptions({}, {});
		if (InArgumentCount > 1)
		{
			std::ofstream File(InArguments[1], std::ios::binary);
			File << WriteJson(Value);
			if (!File.good())
			{
				throw std::runtime_error("Cannot write contract verification output");
			}
		}
		return 0;
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
