#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Viewer/ViewerPlugin.h"
#include <iostream>

int main(int InCount, char** InValues)
{
	try
	{
		Hyperion::InitializeLog(std::filesystem::path(HYP_SOURCE_DIR) / "out/logs/viewer.log");
		Hyperion::RunViewerApplication(InCount, InValues, Hyperion::RegisterD3D12RHIBackend);
		Hyperion::ShutdownLog();
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << "Hyperion Viewer: " << Error.what() << '\n';
		return 1;
	}
}
