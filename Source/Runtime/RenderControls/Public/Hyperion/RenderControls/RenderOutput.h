#pragma once
#include "Hyperion/Reflection/RecordValue.h"
#include <memory>
#include <optional>
#include <variant>

namespace Hyperion
{
enum class EImageOutputWindow
{
	Main,
	Assets
};

// Keep opaque protocol tokens without allowing them to masquerade as a native window kind.
class FImageOutputWindow
{
public:
	FImageOutputWindow() = default;
	explicit FImageOutputWindow(EImageOutputWindow InKind);
	static FImageOutputWindow FromWire(std::string InName);
	std::optional<EImageOutputWindow> Kind() const;
	std::string_view WireName() const;

private:
	std::variant<EImageOutputWindow, std::string> Value = EImageOutputWindow::Main;
};

struct FImageOutputRequest
{
	std::string Path;
	FImageOutputWindow Window;
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

template<> const FRecordDescriptor& RecordType<FImageOutputRequest>();
template<> const FRecordDescriptor& RecordType<FImageArtifact>();
} // namespace Hyperion
