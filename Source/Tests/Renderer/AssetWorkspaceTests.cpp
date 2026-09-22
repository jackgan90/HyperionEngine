#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <iostream>

namespace Hyperion
{
void WriteAssetEditorFixture(const std::filesystem::path& InRoot);
void CheckAssetWorkspaceRegressions(FTaskSystem& InTasks, FAssetService& InAssets, FRenderSession& InSession,
                                    FRHICapabilities InCapabilities);
} // namespace Hyperion

int main()
{
	using namespace Hyperion;
	try
	{
		const auto Root = std::filesystem::current_path() / "asset-workspace-regressions";
		WriteAssetEditorFixture(Root);
		FTaskSystem Tasks(2, 1);
		auto Files = std::make_shared<FMountedFileSystem>(std::vector<FContentMount>{
		    {"/Engine", std::filesystem::path(HYP_SOURCE_DIR) / "Content"}, {"/Game", Root, false}});
		FIOService IO(Tasks, Files);
		FAssetService Assets(IO);
		RegisterSceneAssetTypes(Assets.Types());
		FShaderCompiler Compiler("/Engine/Shaders", Root / "ShaderCache", Files);
		std::unique_ptr<IRHIDevice> Device;
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
		                          }));
		try
		{
			FRenderSession Session(Tasks, *Device, Compiler);
			CheckAssetWorkspaceRegressions(Tasks, Assets, Session, Device->GetCapabilities());
		}
		catch (...)
		{
			Assets.Drain();
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          Device.reset();
			                          }));
			throw;
		}
		Assets.Drain();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device.reset();
		                          }));
		std::cout << "Asset workspace edge-case regressions passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
