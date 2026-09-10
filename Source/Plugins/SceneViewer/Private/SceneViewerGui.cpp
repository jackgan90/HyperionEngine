#include "SceneViewerInternal.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Hyperion
{
namespace
{
void DrawBatchControl(FGui& InGui, bool& bInEnabled, bool bInForceOrdinary)
{
	if (bInForceOrdinary)
	{
		InGui.TextWrapped("Instance batching: off (--no-instance-batching)");
	}
	else
	{
		InGui.Checkbox("Instance batching", bInEnabled);
	}
}

void DrawStatistics(FGui& InGui, const FSceneVisibilityStats& InStats)
{
	InGui.Text("Groups " + std::to_string(InStats.Groups) + " | unbounded " + std::to_string(InStats.UnboundedGroups));
	InGui.Text("BVH visits " + std::to_string(InStats.VisitedNodes) + " | group tests " +
	           std::to_string(InStats.GroupTests));
	InGui.Text("Primitives " + std::to_string(InStats.Primitives) + " | candidates " +
	           std::to_string(InStats.CandidatePrimitives));
	InGui.Text("Collect calls " + std::to_string(InStats.CollectedPrimitives) + " | emitted items " +
	           std::to_string(InStats.EmittedItems));
	InGui.Text("Visible items " + std::to_string(InStats.VisibleItems) + " | scene draws " +
	           std::to_string(InStats.Draws));
	InGui.Text("Membership reused " + std::to_string(InStats.MembershipReuses) + " | entered/left " +
	           std::to_string(InStats.MembershipAdded) + "/" + std::to_string(InStats.MembershipRemoved));
	const auto& Batch = InStats.Batches;
	InGui.Text("Incremental updates " + std::to_string(Batch.IncrementalPlanUpdates) + " | affected/retained blocks " +
	           std::to_string(Batch.AffectedBatches) + "/" + std::to_string(Batch.RetainedBatches));
	InGui.Text("Batch admissions reused " + std::to_string(Batch.BatchAdmissionReuses) + " | cached members/blocks " +
	           std::to_string(Batch.CachedPlanItems) + "/" + std::to_string(Batch.CachedPlanBlocks));
	InGui.Text("Instanced " + std::to_string(Batch.InstancedItems) + " in " + std::to_string(Batch.InstancedDraws) +
	           " draws | singles " + std::to_string(Batch.SingleDraws));
	std::string Fallbacks;
	for (std::size_t Index = 0; Index < Batch.Fallbacks.size(); ++Index)
	{
		if (Batch.Fallbacks[Index])
		{
			Fallbacks += Fallbacks.empty() ? "" : " | ";
			Fallbacks += GetRenderBatchFallbackName(static_cast<ERenderBatchFallback>(Index));
			Fallbacks += " " + std::to_string(Batch.Fallbacks[Index]);
		}
	}
	InGui.TextWrapped("Fallbacks: " + (Fallbacks.empty() ? "none" : Fallbacks));
	InGui.Text("Chunks reused/rebuilt " + std::to_string(Batch.ReusedChunks) + "/" +
	           std::to_string(Batch.RebuiltChunks) + " | upload " + std::to_string(Batch.UploadBytes) + " B");
	std::ostringstream BatchTime;
	BatchTime << std::fixed << std::setprecision(3) << "Batch plan " << Batch.PlanningMilliseconds << " ms | draw prep "
	          << Batch.PreparationMilliseconds << " ms";
	InGui.Text(BatchTime.str());
	std::ostringstream Timing;
	Timing << std::fixed << std::setprecision(3) << "Index update " << InStats.UpdateMilliseconds << " ms | query "
	       << InStats.QueryMilliseconds << " ms";
	InGui.Text(Timing.str());
	InGui.Text("Rebuilds " + std::to_string(InStats.IndexRebuilds) + " | refit leaves " +
	           std::to_string(InStats.IndexRefits) + " (last frame)");
}
} // namespace

void FSceneViewerPlugin::FImpl::DrawBounds(FGui& InGui) const
{
	for (const auto Handle : Scene.GetHandles())
	{
		const auto Model = Scene.Find(Handle);
		if (!Model->Data || !Model->bVisible || !IsUsable(Model->Data->Bounds))
		{
			continue;
		}
		const auto Clip = Multiply(LastView.ViewProjection, Model->World);
		std::array<FVec4, 8> Corners;
		for (unsigned Index = 0; Index < 8; ++Index)
		{
			const auto Point = BoundsCorner(Model->Data->Bounds, Index);
			Corners[Index] = Transform(Clip, {Point.X, Point.Y, Point.Z, 1});
		}
		for (unsigned Index = 0; Index < 8; ++Index)
		{
			for (unsigned Axis = 1; Axis <= 4; Axis *= 2)
			{
				if (Index & Axis)
				{
					continue;
				}
				const auto A = Corners[Index];
				const auto B = Corners[Index | Axis];
				if (A.W > .0001f && B.W > .0001f && A.Z >= 0 && B.Z >= 0 &&
				    IsFinite({A.X / A.W, A.Y / A.W, B.X / B.W}) && std::isfinite(B.Y / B.W))
				{
					InGui.OverlayLine({A.X / A.W * .5f + .5f, .5f - A.Y / A.W * .5f},
					                  {B.X / B.W * .5f + .5f, .5f - B.Y / B.W * .5f}, 0xFF66CCFF);
				}
			}
		}
	}
}

void FSceneViewerPlugin::DrawGui(FGui& InGui, const FSceneVisibilityStats& InStats, bool bInForceOrdinary)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.bBounds)
	{
		P.DrawBounds(InGui);
	}
	const auto Size = InGui.DisplaySize();
	if (InGui.BeginPanel("Scene Viewer", {std::max(12.f, Size.X - 410), 16}, {394, std::min(720.f, Size.Y - 32)}))
	{
		InGui.TextWrapped(P.Status);
		DrawStatistics(InGui, InStats);
		InGui.Separator();
		const std::array Names{"None", "Linear frustum", "BVH frustum"};
		InGui.Text(std::string("Culling: ") + Names[static_cast<unsigned>(P.Mode)]);
		if (InGui.Button("Cycle culling [C]"))
		{
			SetCullingMode(static_cast<ESceneCullingMode>((static_cast<unsigned>(P.Mode) + 1) % 3));
		}
		bool bFrozen = P.bFrozen;
		if (InGui.Checkbox("Freeze culling camera", bFrozen))
		{
			SetFrozen(bFrozen);
		}
		DrawBatchControl(InGui, P.bInstanceBatching, bInForceOrdinary);
		InGui.Checkbox("Show model bounds", P.bBounds);
		if (InGui.Button("Fit all [Home]"))
		{
			Fit();
		}
		InGui.TextWrapped("Drag: orbit | Wheel: dolly | Arrows: move | Page Up/Down: elevate | Tab: panels");
		InGui.Separator();
		if (!P.Instances.empty())
		{
			P.Selected %= P.Instances.size();
			const auto Handle = P.Instances[P.Selected].Handle;
			InGui.Text("Selected: " + P.Scene.Find(Handle)->Name);
			if (InGui.Button("Next model"))
			{
				P.Selected = (P.Selected + 1) % P.Instances.size();
			}
			if (InGui.Button("Duplicate [Insert]"))
			{
				DuplicateSelected();
			}
			if (InGui.Button("Remove [Delete]"))
			{
				RemoveSelected();
			}
			if (InGui.Button("Show / hide [Space]"))
			{
				ToggleSelected();
			}
			if (InGui.Button("Move X -2"))
			{
				MoveSelected(-2);
			}
			if (InGui.Button("Move X +2"))
			{
				MoveSelected(2);
			}
			InGui.Checkbox("Animate selected across view", P.bAnimate);
		}
		if (InGui.Button("Add loaded model"))
		{
			AddModel();
		}
		for (const auto& [Id, Load] : P.Loads)
		{
			if (!Load.Error.empty())
			{
				InGui.TextWrapped(Id + ": " + Load.Error);
			}
		}
		for (const auto& Instance : P.Instances)
		{
			if (P.Bridge)
			{
				for (const auto& Draw : P.Bridge->GetDrawResults(Instance.Handle))
				{
					if (!Draw.Error.empty())
					{
						InGui.TextWrapped(P.Scene.Find(Instance.Handle)->Name + " (view " + std::to_string(Draw.View) +
						                  "): " + Draw.Error);
					}
				}
			}
			const auto Error = P.Bridge ? P.Bridge->GetError(Instance.Handle) : std::string{};
			if (!Error.empty())
			{
				InGui.TextWrapped(P.Scene.Find(Instance.Handle)->Name + ": " + Error);
			}
		}
	}
	InGui.EndPanel();
}
} // namespace Hyperion
