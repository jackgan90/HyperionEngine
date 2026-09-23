#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Platform/Window.h"
#include "Hyperion/Reflection/RecordValue.h"
#include <memory>
#include <optional>

namespace Hyperion
{
struct FImageOutputRequest
{
	std::string Path;
	std::string Window = "main";
	bool bOverwrite{};
};

struct FImageArtifact
{
	std::string Path;
	std::string MediaType = "image/png";
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::uint64_t Frame{};
	std::uint64_t Bytes{};
};

// Main-owned state retained by host and caller until readback and write complete.
struct FPendingImageOutput
{
	FImageOutputRequest Request;
	std::optional<FImageArtifact> Result;
	std::string Error;
};

class IRenderOutput
{
public:
	virtual ~IRenderOutput() = default;
	virtual std::shared_ptr<FPendingImageOutput> RequestImage(const FImageOutputRequest& InRequest) = 0;
	virtual std::optional<FImageArtifact> PollImage(const std::shared_ptr<FPendingImageOutput>& InPending) = 0;
};

std::shared_ptr<FPendingImageOutput> PrepareImageOutput(const FImageOutputRequest& InRequest);
void CompleteImageOutput(FPendingImageOutput& InPending, const FImage& InImage, std::uint64_t InFrame);
void DescribeImageOutput(FPendingImageOutput& InPending, FSize InSize, std::uint64_t InFrame);
template<> const FRecordDescriptor& RecordType<FImageOutputRequest>();
template<> const FRecordDescriptor& RecordType<FImageArtifact>();
} // namespace Hyperion
