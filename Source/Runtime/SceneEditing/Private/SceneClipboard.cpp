#include "Hyperion/Scene/SceneClipboard.h"
#include "Hyperion/Core/Identity.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <charconv>
#include <set>

namespace Hyperion
{
namespace
{
std::string CopyName(const std::string& InName, std::set<std::string>& InUsed)
{
	std::string Base = InName;
	std::uint64_t Suffix = 0;
	const auto Start = Base.rfind(" (");
	if (Start != std::string::npos && Base.back() == ')')
	{
		const auto* End = Base.data() + Base.size() - 1;
		const auto Parsed = std::from_chars(Base.data() + Start + 2, End, Suffix);
		if (Parsed.ec == std::errc{} && Parsed.ptr == End && Suffix < UINT64_MAX)
		{
			Base.resize(Start);
		}
		else
		{
			Suffix = 0;
		}
	}
	for (;;)
	{
		if (Suffix == UINT64_MAX)
		{
			throw std::invalid_argument("Object name suffix is exhausted");
		}
		auto Name = Base + " (" + std::to_string(++Suffix) + ")";
		if (InUsed.insert(Name).second)
		{
			return Name;
		}
	}
}

FSceneClipboardSnapshot CaptureClipboard(const FSceneEditDocument& InDocument)
{
	FSceneClipboardSnapshot Result;
	Result.Document = InDocument.Id();
	Result.Token = CreateEphemeralIdentity();
	Result.Selection = InDocument.Selection();
	for (const auto Handle : Result.Selection.All())
	{
		if (!InDocument.Target().FindNode(Handle))
		{
			throw FSceneEditError(SceneEditErrors::StaleHandle, "A selected object no longer exists");
		}
	}
	Result.Roots = InDocument.SelectedRoots();
	std::vector<FSceneHandle> Pending = Result.Roots;
	std::set<std::string> Ids;
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		if (Pending.size() > SceneClipboardMaxNodes)
		{
			throw std::invalid_argument("Scene clipboard exceeds 16384 nodes");
		}
		const auto Handle = Pending[Index];
		auto Node = CloneSceneClipboardNode(InDocument.Target().CaptureNode(Handle), Result.Bytes);
		Ids.insert(Node.Id);
		Result.Nodes.emplace_back(Handle, std::move(Node));
		const auto Children = InDocument.Target().Children(Handle);
		Pending.insert(Pending.end(), Children.begin(), Children.end());
	}
	for (auto& [Handle, Node] : Result.Nodes)
	{
		VisitSceneClipboardReferences(Node,
		                              [&](std::string& InId)
		                              {
			                              if (!InId.empty() && !Ids.contains(InId))
			                              {
				                              const auto Reference = InDocument.Target().FindHandle(InId);
				                              if (!Reference.Scene)
				                              {
					                              throw FSceneEditError(SceneEditErrors::StaleHandle,
					                                                    "Clipboard refers to a missing scene object: " +
					                                                        InId);
				                              }
				                              Result.External.emplace(InId, Reference);
			                              }
		                              });
	}
	return Result;
}
} // namespace

void FSceneEditDocument::SetClipboardProvider(FSceneClipboardProvider InProvider)
{
	ClipboardSnapshot.reset();
	ClipboardProvider = std::move(InProvider);
}

bool FSceneEditDocument::HasClipboardProvider() const
{
	return bool(ClipboardProvider.Read) && bool(ClipboardProvider.Write);
}

FSceneClipboardInfo FSceneEditDocument::ClipboardInfo() const
{
	FSceneClipboardInfo Info;
	Info.bAvailable = HasClipboardProvider();
	if (!Info.bAvailable)
	{
		Info.Reason = "This host has no scene clipboard provider";
		return Info;
	}
	std::string Token;
	try
	{
		Token = ClipboardProvider.Read();
	}
	catch (const std::exception& Error)
	{
		throw FSceneEditError(SceneEditErrors::ClipboardError, Error.what());
	}
	if (!ClipboardSnapshot || ClipboardSnapshot->Document != Id() || Token.empty() || Token != ClipboardSnapshot->Token)
	{
		Info.Reason = "Clipboard does not contain objects from this scene document";
		return Info;
	}
	Info.Nodes = static_cast<std::uint32_t>(ClipboardSnapshot->Nodes.size());
	Info.Selected = static_cast<std::uint32_t>(ClipboardSnapshot->Selection.All().size());
	for (const auto& [Id, Handle] : ClipboardSnapshot->External)
	{
		const auto* Node = Target().FindNode(Handle);
		if (!Node || Node->Id != Id)
		{
			Info.Reason = "A copied object's external parent or reference no longer exists: " + Id;
			return Info;
		}
	}
	Info.bCanPaste = Target().IsLoaded() && !IsBusy() && !Target().IsPreparing();
	if (!Info.bCanPaste)
	{
		Info.Reason = "Scene is unavailable, preparing resources or busy";
	}
	return Info;
}

FSceneClipboardInfo FSceneEditDocument::CopySelection(const std::string& InDocument, std::uint64_t InRevision)
{
	RequireIdle(InDocument, InRevision);
	if (!HasClipboardProvider())
	{
		throw FSceneEditError(SceneEditErrors::Unavailable, "This host has no scene clipboard provider");
	}
	if (!Selected)
	{
		return ClipboardInfo();
	}
	auto Snapshot = CaptureClipboard(*this);
	Snapshot.AssetGeneration = AssetGeneration;
	std::string Text;
	for (const auto Handle : Selected.All())
	{
		if (!Text.empty())
		{
			Text += '\n';
		}
		Text += Target().FindNode(Handle)->Name;
	}
	try
	{
		ClipboardProvider.Write(Snapshot.Token, Text);
	}
	catch (const std::exception& Error)
	{
		throw FSceneEditError(SceneEditErrors::ClipboardError, Error.what());
	}
	ClipboardSnapshot = std::move(Snapshot);
	return {true,
	        true,
	        static_cast<std::uint32_t>(ClipboardSnapshot->Nodes.size()),
	        static_cast<std::uint32_t>(Selected.All().size()),
	        {}};
}

std::vector<FSceneNode> FSceneEditDocument::PrepareClipboardPaste()
{
	const auto& Snapshot = *ClipboardSnapshot;
	std::map<std::string, std::string, std::less<>> Ids;
	std::set<std::string> CreatedIds;
	std::set<std::string> Names;
	for (const auto Handle : Target().Nodes())
	{
		Names.insert(Target().FindNode(Handle)->Name);
	}
	for (const auto& [Handle, Node] : Snapshot.Nodes)
	{
		std::string Id;
		do
		{
			Id = "object-" + CreateEphemeralIdentity();
		} while (Target().FindHandle(Id).Scene || !CreatedIds.insert(Id).second);
		Ids.emplace(Node.Id, std::move(Id));
	}
	std::vector<FSceneNode> Nodes;
	std::size_t Bytes{};
	for (const auto& [Handle, Source] : Snapshot.Nodes)
	{
		auto Node = CloneSceneClipboardNode(Source, Bytes);
		Node.Id = Ids.at(Source.Id);
		Node.Name = CopyName(Source.Name, Names);
		VisitSceneClipboardReferences(Node,
		                              [&](std::string& InId)
		                              {
			                              if (const auto It = Ids.find(InId); It != Ids.end())
			                              {
				                              InId = It->second;
			                              }
		                              });
		if (Snapshot.AssetGeneration != AssetGeneration)
		{
			Node = Target().Rebind(std::move(Node));
		}
		ValidateSceneNode(Node);
		Nodes.push_back(std::move(Node));
	}
	return Nodes;
}

void FSceneEditDocument::PasteClipboard(const std::string& InDocument, std::uint64_t InRevision)
{
	RequireIdle(InDocument, InRevision);
	const auto Info = ClipboardInfo();
	if (!Info.bCanPaste)
	{
		throw FSceneEditError(Info.bAvailable ? SceneEditErrors::ClipboardUnavailable : SceneEditErrors::Unavailable,
		                      Info.Reason);
	}
	auto Nodes = PrepareClipboardPaste();
	RequireIdle(InDocument, InRevision);
	CommitClipboardBatch(std::move(Nodes));
}

template<> const FRecordDescriptor& RecordType<FSceneClipboardInfo>()
{
	static const auto Type = MakeRecord<FSceneClipboardInfo>(
	    "hyperion.sceneclipboardinfo",
	    {Member("available", &FSceneClipboardInfo::bAvailable, {.Description = "Host has a typed clipboard provider."}),
	     Member("canPaste", &FSceneClipboardInfo::bCanPaste,
	            {.Description = "Current token, document, external references and interaction state permit paste."}),
	     Member("nodes", &FSceneClipboardInfo::Nodes,
	            {.Description = "Captured nodes including descendants; zero when the system token does not match."}),
	     Member("selected", &FSceneClipboardInfo::Selected,
	            {.Description = "Explicit selected nodes whose ordered selection will be restored on paste."}),
	     Member("reason", &FSceneClipboardInfo::Reason,
	            {.Description = "Explanation when paste is unavailable; empty when it is allowed."})});
	return Type;
}
} // namespace Hyperion
