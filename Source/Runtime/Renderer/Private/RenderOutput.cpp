#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <array>

namespace Hyperion
{
namespace
{
struct FImageWindowName
{
	EImageOutputWindow Kind;
	std::string_view Name;
};

constexpr std::array ImageWindows{FImageWindowName{EImageOutputWindow::Main, "main"},
                                  FImageWindowName{EImageOutputWindow::Assets, "assets"}};

std::string_view ImageWindowName(EImageOutputWindow InKind)
{
	for (const auto& Entry : ImageWindows)
	{
		if (Entry.Kind == InKind)
		{
			return Entry.Name;
		}
	}
	throw std::invalid_argument("Invalid image output window");
}

FRecordMember ImageWindowMember()
{
	FRecordMember Result;
	Result.Id = "window";
	Result.Options.Description = "main, or assets for the Editor's active asset window. Includes the GUI.";
	Result.Shape = &RecordValueShape<std::string>;
	Result.Association = FRecordMemberAssociation(&FImageOutputRequest::Window);
	Result.Write = [](const void* InObject)
	{
		return WriteValue(std::string(static_cast<const FImageOutputRequest*>(InObject)->Window.WireName()));
	};
	Result.Read = [](void* InObject, const FArchiveNode& InNode, const FRecordReadContext& InContext)
	{
		static_cast<FImageOutputRequest*>(InObject)->Window =
		    FImageOutputWindow::FromWire(ReadValue<std::string>(InNode, InContext));
	};
	Result.Visit = [](const void*, const FRecordVisitor&, std::string_view)
	{
	};
	return Result;
}
} // namespace

FImageOutputWindow::FImageOutputWindow(EImageOutputWindow InKind) : Value(InKind)
{
	ImageWindowName(InKind);
}

FImageOutputWindow FImageOutputWindow::FromWire(std::string InName)
{
	for (const auto& Entry : ImageWindows)
	{
		if (Entry.Name == InName)
		{
			return FImageOutputWindow(Entry.Kind);
		}
	}
	FImageOutputWindow Result;
	Result.Value = std::move(InName);
	return Result;
}

std::optional<EImageOutputWindow> FImageOutputWindow::Kind() const
{
	if (const auto* Kind = std::get_if<EImageOutputWindow>(&Value))
	{
		return *Kind;
	}
	return {};
}

std::string_view FImageOutputWindow::WireName() const
{
	if (const auto* Name = std::get_if<std::string>(&Value))
	{
		return *Name;
	}
	return ImageWindowName(std::get<EImageOutputWindow>(Value));
}

std::shared_ptr<FPendingImageOutput> PrepareImageOutput(const FImageOutputRequest& InRequest)
{
	const auto Path = NormalizeFilePath(PathFromUtf8(InRequest.Path));
	if (InRequest.Path.empty() || Path.extension() != ".png")
	{
		throw std::invalid_argument("Screenshot requires a target-local .png destination");
	}
	if (!InRequest.bOverwrite && std::filesystem::exists(Path))
	{
		throw FSceneEditError("conflict", "Screenshot exists; choose another path or explicitly enable overwrite");
	}
	auto Pending = std::make_shared<FPendingImageOutput>();
	Pending->Request = InRequest;
	Pending->Request.Path = PathToUtf8(Path);
	return Pending;
}

void DescribeImageOutput(FPendingImageOutput& InPending, FSize InSize, std::uint64_t InFrame)
{
	InPending.Result = FImageArtifact{
	    InPending.Request.Path, "image/png", InSize.Width,
	    InSize.Height,          InFrame,     std::filesystem::file_size(PathFromUtf8(InPending.Request.Path))};
}

void CompleteImageOutput(FPendingImageOutput& InPending, const FImage& InImage, std::uint64_t InFrame)
{
	try
	{
		const auto Path = PathFromUtf8(InPending.Request.Path);
		if (!InPending.Request.bOverwrite && std::filesystem::exists(Path))
		{
			throw std::runtime_error("Screenshot destination was created before readback completed");
		}
		if (Path.has_parent_path())
		{
			std::filesystem::create_directories(Path.parent_path());
		}
		SaveImage(Path, InImage);
		DescribeImageOutput(InPending, {InImage.Width, InImage.Height}, InFrame);
	}
	catch (const std::exception& Error)
	{
		InPending.Error = Error.what();
	}
}

template<> const FRecordDescriptor& RecordType<FImageOutputRequest>()
{
	static const auto Type = MakeRecord<FImageOutputRequest>(
	    "hyperion.render.image.request", {Member("path", &FImageOutputRequest::Path, {.bRequired = true}),
	                                      ImageWindowMember(), Member("overwrite", &FImageOutputRequest::bOverwrite)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FImageArtifact>()
{
	static const auto Type = MakeRecord<FImageArtifact>(
	    "hyperion.render.image.artifact",
	    {Member("path", &FImageArtifact::Path), Member("mediaType", &FImageArtifact::MediaType),
	     Member("width", &FImageArtifact::Width), Member("height", &FImageArtifact::Height),
	     Member("frame", &FImageArtifact::Frame), Member("bytes", &FImageArtifact::Bytes)});
	return Type;
}
} // namespace Hyperion
