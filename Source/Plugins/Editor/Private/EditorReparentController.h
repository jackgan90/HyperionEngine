#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <unordered_set>
#include <variant>

namespace Hyperion
{
struct FReparentGesture
{
	std::string Token;
	std::string Document;
	std::uint64_t Revision{};
	std::vector<FSceneHandle> Handles;
	FSceneHandle Source;
	FVec2 Start;
	FVec4 Bounds;
	bool bToggle{};
	bool bRange{};
	bool bDragging{};
	bool bTargetPreview{};
};

// Transient, synchronous coordination with other Editor interactions. Never retained by the controller.
class IEditorReparentActions
{
public:
	virtual ~IEditorReparentActions() = default;
	virtual void FinishInspectorEdit() = 0;
	virtual void SelectObject(std::optional<FSceneHandle> InHandle) = 0;
	virtual void SetSelection(FSceneSelection InSelection) = 0;
	virtual void ClickOutlinerObject(FSceneHandle InHandle, bool bInToggle, bool bInRange) = 0;
	virtual void BeginReparentDrag() = 0;
	virtual void UpdateDocumentInteraction() = 0;
};

struct FEditorReparentFeedback
{
	std::optional<std::string> Error;
	std::optional<FVec4> RootBounds;
};

// Main-only GUI responsibility. SceneEditing remains the sole document/selection/history owner.
class FEditorReparentController
{
public:
	bool HasGesture() const;
	bool IsDragging() const;
	const std::optional<FReparentGesture>& GetGesture() const;
	void Cancel(FGui* InGui);
	void Reset(FGui* InGui);
	bool ConsumeExpansion(const std::string& InNodeId);
	void Update(FGui& InGui, FSceneEditDocument& InDocument, IEditorReparentActions& InActions,
	            std::span<const FInputEvent> InEvents, bool bInAllowed, bool bInOutlinerVisible);
	FEditorReparentFeedback RouteRow(FGui& InGui, FSceneEditDocument& InDocument, IEditorReparentActions& InActions,
	                                 FSceneHandle InHandle, bool bInActivated, bool bInAllowed);
	FEditorReparentFeedback DrawRoot(FGui& InGui, FSceneEditDocument& InDocument);
	std::optional<std::string> Finish(FGui& InGui, FSceneEditDocument& InDocument, IEditorReparentActions& InActions);

private:
	struct FRootDelivery
	{
	};

	struct FNodeDelivery
	{
		FSceneHandle Parent;
	};

	void Begin(FGui& InGui, FSceneEditDocument& InDocument, IEditorReparentActions& InActions, FSceneHandle InHandle,
	           FVec4 InBounds, bool bInAllowed);
	std::optional<std::string> DrawTarget(FGui& InGui, FSceneEditDocument& InDocument,
	                                      std::optional<FSceneHandle> InParent);
	std::optional<FReparentGesture> Gesture;
	std::variant<std::monostate, FRootDelivery, FNodeDelivery> Delivery;
	std::unordered_set<std::string> OpenNodes;
	std::uint64_t Serial{};
};
} // namespace Hyperion
