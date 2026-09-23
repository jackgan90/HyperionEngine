#pragma once
#include "Hyperion/Reflection/RecordValue.h"

namespace Hyperion
{
enum class EApplicationCloseAction
{
	RejectDirty,
	Save,
	Discard,
	Cancel
};

struct FApplicationCloseRequest
{
	EApplicationCloseAction Action = EApplicationCloseAction::RejectDirty;
	std::string ScenePath;
};

struct FApplicationCloseState
{
	std::string State = "idle";
	std::string Error;
	bool bDirty{};
	bool bBusy{};
	std::string ScenePath;
};

// Main-owned host close decisions. Success means acceptance, not process termination.
// Saving continues independently of the requesting connection and never exits on failure.
class IApplicationClose
{
public:
	virtual ~IApplicationClose() = default;
	virtual FApplicationCloseState ApplicationCloseState() const = 0;
	virtual FApplicationCloseState RequestApplicationClose(const FApplicationCloseRequest& InRequest) = 0;
};

template<> std::span<const EApplicationCloseAction> RecordEnumValues<EApplicationCloseAction>();
template<> const FRecordDescriptor& RecordType<FApplicationCloseRequest>();
template<> const FRecordDescriptor& RecordType<FApplicationCloseState>();
} // namespace Hyperion
