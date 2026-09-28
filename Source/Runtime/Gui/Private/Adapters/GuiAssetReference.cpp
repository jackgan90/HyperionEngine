#include "GuiInternal.h"
#include <filesystem>
#include <imgui_internal.h>

namespace Hyperion
{
namespace
{
std::string CandidateLabel(const FAssetRef& InReference)
{
	const auto Stem = std::filesystem::path(InReference.Path).stem().string();
	return Stem.empty() ? InReference.Path : Stem;
}

bool SameAsset(const FAssetRef& InLeft, const FAssetRef& InRight)
{
	// Resolved references gain pinned revisions; the requested identity is unchanged.
	return InLeft.Id.empty() || InRight.Id.empty() ? InLeft.Path == InRight.Path : InLeft.Id == InRight.Id;
}
} // namespace

void FGui::SetAssetReferenceProvider(FGuiAssetReferenceProvider InProvider)
{
	Impl->Select();
	Impl->AssetReferenceProvider = std::move(InProvider);
}

bool FGui::EditAssetReference(FAssetRef& InValue, std::string_view InTypeId, bool bInMixed)
{
	Impl->Select();
	const auto Candidates =
	    Impl->AssetReferenceProvider ? Impl->AssetReferenceProvider(InTypeId) : std::vector<FAssetRef>{};
	const auto Current = std::ranges::find_if(Candidates,
	                                          [&](const FAssetRef& InCandidate)
	                                          {
		                                          return SameAsset(InCandidate, InValue);
	                                          });
	const auto CurrentLabel = CandidateLabel(InValue);
	const char* Preview = bInMixed ? "Multiple Values" : CurrentLabel.empty() ? "None" : CurrentLabel.c_str();
	const ImGuiID Id = ImGui::GetID("##value");
	const bool bOpen = ImGui::BeginCombo("##value", Preview);
	const bool bActivated = ImGui::IsItemActivated();
	const bool bDisabled = (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
	bool bChanged{};
	if (bOpen)
	{
		for (std::size_t Index = 0; Index < Candidates.size(); ++Index)
		{
			const auto& Candidate = Candidates[Index];
			ImGui::PushID(static_cast<int>(Index));
			const bool bSelected = !bInMixed && Current == Candidates.begin() + std::ptrdiff_t(Index);
			if (ImGui::Selectable(CandidateLabel(Candidate).c_str(), bSelected))
			{
				// Selection highlighting uses identity, but an explicit choice also updates path/revision.
				bChanged = bInMixed || Candidate != InValue;
				if (bChanged)
				{
					InValue = Candidate;
				}
			}
			ImGui::SetItemTooltip("%s", Candidate.Path.c_str());
			ImGui::PopID();
		}
		if (Candidates.empty())
		{
			ImGui::TextDisabled("No compatible assets");
		}
		ImGui::EndCombo();
	}
	else if (!bInMixed && !InValue.Path.empty())
	{
		ImGui::SetItemTooltip("%s", InValue.Path.c_str());
	}
	bool bDropped{};
	if (const auto Drop = bDisabled ? std::nullopt : DropTarget(AssetPathPayloadType))
	{
		const auto Match = std::ranges::find(Candidates, Drop->Value, &FAssetRef::Path);
		DrawDropFeedback(Match != Candidates.end(),
		                 Match != Candidates.end() ? "Use this asset" : "Asset type is not compatible");
		if (Drop->bDelivery && Match != Candidates.end() && (bInMixed || *Match != InValue))
		{
			InValue = *Match;
			bDropped = true;
		}
	}
	return Impl->TrackEdit(Id, bOpen && !bChanged, bActivated || bDropped, bChanged || bDropped);
}
} // namespace Hyperion
