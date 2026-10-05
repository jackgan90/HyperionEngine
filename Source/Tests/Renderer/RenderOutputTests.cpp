#include "Hyperion/Assets/Assets.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>

using namespace Hyperion;

namespace
{
void CheckWindowProtocol()
{
	const auto& Type = RecordType<FImageOutputRequest>();
	for (const std::string Token :
	     {R"("main")", R"("assets")", R"("")", R"("other")", R"("Main")", R"(" assets")", R"("main\u0000suffix")"})
	{
		const auto Wire = ParseJson("{\"path\":\"capture.png\",\"window\":" + Token + ",\"overwrite\":true}");
		const auto Request = std::static_pointer_cast<FImageOutputRequest>(ReadRecordWire(Type, Wire));
		const auto Name = ReadValue<std::string>(ParseJson(Token));
		const auto ExpectedKind = Name == "main"     ? std::optional{EImageOutputWindow::Main}
		                          : Name == "assets" ? std::optional{EImageOutputWindow::Assets}
		                                             : std::nullopt;
		HYP_CHECK(Request->Window.WireName() == Name && Request->Window.Kind() == ExpectedKind);
		HYP_CHECK(WriteJson(WriteRecordWire(Type, Request.get())) == WriteJson(Wire));
		const auto Archive =
		    ParseJson("{\"type\":\"hyperion.render.image.request\",\"version\":1,\"fields\":" + WriteJson(Wire) + "}");
		const auto Restored = std::static_pointer_cast<FImageOutputRequest>(ReadRecord(Type, Archive));
		HYP_CHECK(Restored->Window.WireName() == Name && Restored->Window.Kind() == ExpectedKind);
		HYP_CHECK(WriteJson(WriteRecord(Type, Restored.get())) == WriteJson(Archive));
	}
	const auto Default =
	    std::static_pointer_cast<FImageOutputRequest>(ReadRecordWire(Type, ParseJson(R"({"path":"capture.png"})")));
	HYP_CHECK(Default->Window.Kind() == EImageOutputWindow::Main && Default->Window.WireName() == "main");
	for (const char* Invalid :
	     {R"({"path":"x.png","window":1})", R"({"path":"x.png","window":null})", R"({"path":"x.png","window":false})"})
	{
		bool bRejected{};
		try
		{
			ReadRecordWire(Type, ParseJson(Invalid));
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
	const auto Schema = RecordWireSchema(Type);
	const auto& Properties =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Schema.Value).at("properties").Value);
	HYP_CHECK(
	    WriteJson(Properties.at("window")) ==
	    R"({"default":"main","description":"main, or assets for the Editor's active asset window. Includes the GUI.","type":"string"})");
	HYP_CHECK(ResolveRecordMember(Type, &FImageOutputRequest::Window).FieldId == "window");
}

void CheckNativeWindow()
{
	FImageOutputWindow Window;
	HYP_CHECK(Window.Kind() == EImageOutputWindow::Main && Window.WireName() == "main");
	Window = FImageOutputWindow(EImageOutputWindow::Assets);
	HYP_CHECK(Window.Kind() == EImageOutputWindow::Assets && Window.WireName() == "assets");
	Window = FImageOutputWindow::FromWire("future");
	HYP_CHECK(!Window.Kind() && Window.WireName() == "future");
	Window = FImageOutputWindow(EImageOutputWindow::Main);
	HYP_CHECK(Window.Kind() == EImageOutputWindow::Main && Window.WireName() == "main");
	bool bRejected{};
	try
	{
		static_cast<void>(FImageOutputWindow(static_cast<EImageOutputWindow>(999)));
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckPreparationAndCompletion(const std::filesystem::path& InDirectory)
{
	const auto Path = InDirectory / "Completed.png";
	std::filesystem::remove(Path);
	FImageOutputRequest Request{PathToUtf8(Path), FImageOutputWindow(EImageOutputWindow::Assets), false};
	const auto Pending = PrepareImageOutput(Request);
	HYP_CHECK(Pending->Request.Window.Kind() == EImageOutputWindow::Assets);
	HYP_CHECK(!Pending->Result && Pending->Error.empty() && !std::filesystem::exists(Path));
	Request.Window = FImageOutputWindow(EImageOutputWindow::Main);
	HYP_CHECK(Pending->Request.Window.Kind() == EImageOutputWindow::Assets);
	const FImage Image{1, 1, EColorSpace::Srgb, {1, 0, 0, 1}};
	CompleteImageOutput(*Pending, Image, 42);
	HYP_CHECK(Pending->Error.empty() && Pending->Result);
	const auto& Result = *Pending->Result;
	HYP_CHECK(Result.Path == PathToUtf8(Path) && Result.MediaType == "image/png");
	HYP_CHECK(Result.Width == 1 && Result.Height == 1 && Result.Frame == 42);
	HYP_CHECK(Result.Bytes == std::filesystem::file_size(Path) && Result.Bytes > 0);
	const auto Restored = LoadImageFile(Path);
	HYP_CHECK(Restored.Width == 1 && Restored.Height == 1 && Restored.Rgba == Image.Rgba);
	bool bConflict{};
	try
	{
		PrepareImageOutput(Request);
	}
	catch (const FSceneEditError& Error)
	{
		bConflict =
		    Error.Code == SceneEditErrors::Conflict &&
		    std::string_view(Error.what()) == "Screenshot exists; choose another path or explicitly enable overwrite";
	}
	HYP_CHECK(bConflict);
	Request.bOverwrite = true;
	const auto Replacement = PrepareImageOutput(Request);
	HYP_CHECK(Replacement->Request.Window.Kind() == EImageOutputWindow::Main);
	CompleteImageOutput(*Replacement, Image, 43);
	HYP_CHECK(Replacement->Result && Replacement->Result->Frame == 43 && Replacement->Error.empty());
	std::filesystem::remove(Path);
}

void CheckPreparationFailures(const std::filesystem::path& InDirectory)
{
	for (const std::string Path : {"", "invalid.txt"})
	{
		bool bRejected{};
		try
		{
			PrepareImageOutput(FImageOutputRequest{Path});
		}
		catch (const std::invalid_argument& Error)
		{
			bRejected = std::string_view(Error.what()) == "Screenshot requires a target-local .png destination";
		}
		HYP_CHECK(bRejected);
	}
	const auto Path = InDirectory / "Race.png";
	std::filesystem::remove(Path);
	// Preparation never owned window validation; opaque tokens remain accepted here.
	const auto Pending = PrepareImageOutput({PathToUtf8(Path), FImageOutputWindow::FromWire("future")});
	HYP_CHECK(!Pending->Request.Window.Kind() && Pending->Request.Window.WireName() == "future");
	{
		std::ofstream File(Path);
		File << "existing";
	}
	CompleteImageOutput(*Pending, FImage{1, 1, EColorSpace::Srgb, {1, 0, 0, 1}}, 44);
	HYP_CHECK(!Pending->Result && Pending->Error == "Screenshot destination was created before readback completed");
	std::ifstream File(Path);
	std::string Contents;
	File >> Contents;
	File.close();
	HYP_CHECK(Contents == "existing");
	std::filesystem::remove(Path);
}
} // namespace

int main()
{
	try
	{
		const auto Directory = std::filesystem::absolute("image-output-tests");
		std::filesystem::create_directories(Directory);
		CheckWindowProtocol();
		CheckNativeWindow();
		CheckPreparationAndCompletion(Directory);
		CheckPreparationFailures(Directory);
		std::cout << "PASS: typed screenshot windows, legacy protocol and output completion\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
