#include "Hyperion/RenderControls/RenderOutput.h"
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
