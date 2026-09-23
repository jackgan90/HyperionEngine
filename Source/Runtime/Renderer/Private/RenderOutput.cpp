#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
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
	    "hyperion.render.image.request",
	    {Member("path", &FImageOutputRequest::Path, {.bRequired = true}),
	     Member("window", &FImageOutputRequest::Window,
	            {.Description = "main, or assets for the Editor's active asset window. Includes the GUI."}),
	     Member("overwrite", &FImageOutputRequest::bOverwrite)});
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
