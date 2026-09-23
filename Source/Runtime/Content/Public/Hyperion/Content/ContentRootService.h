#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/IO/MountedFileSystem.h"

namespace Hyperion
{
struct FContentRootQuery
{
};

struct FContentRootInfo
{
	std::string Directory;
	std::uint64_t Generation{};
	bool bReadOnly{};
};

struct FContentRootRequest
{
	std::string Directory;
	std::uint64_t Generation{};
	bool bReadOnly{};
	bool bDiscard{};
};

struct FContentRootClearRequest
{
	std::uint64_t Generation{};
	bool bDiscard{};
};

template<> const FRecordDescriptor& RecordType<FContentRootQuery>();
template<> const FRecordDescriptor& RecordType<FContentRootInfo>();
template<> const FRecordDescriptor& RecordType<FContentRootRequest>();
template<> const FRecordDescriptor& RecordType<FContentRootClearRequest>();

class FContentRootError : public std::runtime_error
{
public:
	FContentRootError(std::string InCode, std::string InMessage)
	    : std::runtime_error(std::move(InMessage)), Code(std::move(InCode))
	{
	}

	std::string Code;
};

struct FContentRootParticipantState
{
	bool bDirty{};
	bool bBusy{};
};

// Main-only. Check is side-effect free; Release joins work and retires old content references.
class IContentRootParticipant
{
public:
	virtual ~IContentRootParticipant() = default;
	virtual FContentRootParticipantState ContentRootState() const = 0;
	virtual void ReleaseContentRoot() = 0;
	virtual void ContentRootChanged() = 0;
};

class FContentRootService;

struct FContentRootCandidate
{
	FContentRootCandidate() = default;
	FContentRootCandidate(FContentRootCandidate&&) noexcept = default;
	FContentRootCandidate& operator=(FContentRootCandidate&&) = delete;

	const std::filesystem::path& GetDirectory() const
	{
		return Directory;
	}

private:
	friend class FContentRootService;
	std::filesystem::path Directory;
	FContentRootService* Owner{};
	std::uint64_t Generation{};
	bool bReadOnly{};
	std::shared_ptr<FMountedFileSystem> Files;
	std::unique_ptr<FIOService> IO;
	std::unique_ptr<FAssetService> Assets;
};

std::shared_ptr<FMountedFileSystem> CreateContentFileSystem(const std::filesystem::path& InEngineDirectory,
                                                            bool bInAuthoring = false);

class FContentRootService
{
public:
	FContentRootService(FTaskSystem& InTasks, FMountedFileSystem& InFiles, FAssetService& InAssets);
	FContentRootService(const FContentRootService&) = delete;
	FContentRootService& operator=(const FContentRootService&) = delete;
	FContentRootCandidate Prepare(const std::filesystem::path& InDirectory, bool bInReadOnly = false);
	void Commit(FContentRootCandidate InCandidate, bool bInDiscard = false);
	void Change(const std::filesystem::path& InDirectory, bool bInReadOnly = false, bool bInDiscard = false);
	FContentRootInfo Info() const;
	FContentRootInfo Set(const FContentRootRequest& InRequest);
	FContentRootInfo Clear(const FContentRootClearRequest& InRequest);
	std::filesystem::path Directory() const;
	void RegisterParticipant(IContentRootParticipant& InParticipant);
	void UnregisterParticipant(IContentRootParticipant& InParticipant);
	std::string StartupError;

private:
	void RequireReady() const;
	void RequireGeneration(std::uint64_t InGeneration) const;
	FTaskSystem& Tasks;
	FMountedFileSystem& Files;
	FAssetService& Assets;
	std::vector<IContentRootParticipant*> Participants;
	std::uint64_t Generation{};
	bool bChanging{};
	bool bFailed{};
};
} // namespace Hyperion
