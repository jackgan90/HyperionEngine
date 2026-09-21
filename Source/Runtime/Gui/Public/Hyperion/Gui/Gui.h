#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Platform/Window.h"
#include "Hyperion/Reflection/Record.h"
#include <span>

namespace Hyperion
{
struct FGuiVertex
{
	FVec2 Position;
	FVec2 Uv;
	std::uint32_t Color{};
};

struct FGuiCommand
{
	FVec4 Clip;
	std::uint32_t IndexCount{};
	std::uint32_t FirstIndex{};
	std::int32_t VertexOffset{};
	std::uint64_t TextureId = 1;
};

struct FGuiDrawData
{
	FVec2 DisplayPosition;
	FVec2 DisplaySize;
	FVec2 FramebufferScale;
	std::vector<FGuiVertex> Vertices;
	std::vector<std::uint32_t> Indices;
	std::vector<FGuiCommand> Commands;
	// Immutable atlas retained across queued frames and scale changes.
	std::shared_ptr<const FImage> FontAtlas;
};

struct FGuiImageRegion
{
	FVec4 Bounds;
	bool bHovered{};
	bool bFocused{};
};

struct FGuiPointerState
{
	FVec2 Position;
	bool bPressed{};
	bool bDown{};
	bool bReleased{};
	bool bCancel{};
	bool bRightDown{};
	bool bPositionValid{};
	bool bCtrl{};
};

struct FGuiDockLayout
{
	std::string Center;
	std::string RightTop;
	std::string RightBottom;
	std::string Bottom;
	std::string Left;
};

struct FGuiDragPayload
{
	std::string Type;
	std::string Value;
	bool bDelivery{};
};

// Interaction tokens identify one activation/drag/popup, rather than individual changed frames.
struct FGuiEditState
{
	std::uint64_t ChangedInteraction{};
	std::uint64_t ActiveInteraction{};
};

enum class EGuiIcon
{
	Translate,
	Rotate,
	Scale,
	Options,
	Capture
};

// All GUI context and widget operations belong to the creating Main thread.
class FGui
{
public:
	explicit FGui(FWindow* InClipboardWindow = nullptr);
	~FGui();
	FImage FontImage();
	// Requested scale is applied at the next BeginFrame; valid range is [1, 2].
	void SetApplicationScale(float InScale);
	float ApplicationScale() const;
	// Relative dragging and Ctrl-click input avoid scale-dependent slider feedback.
	bool ApplicationScaleControl(const char* InLabel);
	// Convert a baseline logical dimension using the currently applied scale.
	float Scale(float InSize) const;
	void LoadFont(std::span<const std::byte> InBytes, float InPixels);
	void BeginFrame(FSize InLogical, FSize InPixels, float InDeltaSeconds, std::span<const FInputEvent> InEvents);
	bool BeginPanel(const char* InTitle, FVec2 InPosition, FVec2 InSize);
	void EndPanel();
	void BeginScrollRegion(const char* InId, float InHeight);
	void EndScrollRegion();
	void Text(const std::string& InValue);
	void TextWrapped(const std::string& InValue);
	FVec2 DisplaySize() const;
	void OverlayLine(FVec2 InNormalizedA, FVec2 InNormalizedB, std::uint32_t InColor);
	EMouseCursor MouseCursor() const;
	bool WantsMouse() const;
	bool WantsKeyboard() const;
	void Separator();
	bool Button(const char* InLabel, bool bInEnabled = true);
	bool IconButton(const char* InId, EGuiIcon InIcon, const char* InTooltip, bool bInSelected = false);
	void Tooltip(const char* InText);
	float AvailableWidth() const;
	bool BeginSplitPane(const char* InId, float InLeftWidth = 220);
	bool BeginTileGrid(const char* InId, float InTileWidth = 108);
	bool FileTile(const char* InId, const char* InLabel, bool bInFolder, bool bInSelected, bool& bOutDoubleClicked);
	void SetNextTreeOpen(bool bInOpen);
	void ClosePopups();
	bool Checkbox(const char* InLabel, bool& bInValue);
	bool Slider(const char* InLabel, float& InValue, float InMinimum, float InMaximum);
	bool Selectable(const char* InLabel, bool bInSelected, unsigned InDepth = 0, bool* bOutDoubleClicked = nullptr);
	bool Combo(const char* InLabel, std::span<const std::string> InChoices, std::size_t& InIndex);
	bool InputText(const char* InLabel, std::string& InValue, bool bInCommitOnEnter = true);
	bool InputFloat(const char* InLabel, float& InValue);
	bool InputNumber(const char* InLabel, double& InValue);
	bool InputInteger(const char* InLabel, std::int64_t& InValue);
	bool InputInteger(const char* InLabel, std::uint64_t& InValue);
	// Pair around a value widget with a hidden label to draw a left-aligned property name.
	void BeginPropertyRow(const char* InLabel, bool* bOutExpanded = nullptr);
	void EndPropertyRow();
	void BeginDisabled(bool bInDisabled);
	void EndDisabled();
	bool EditRecord(FRecordDraft& InDraft, std::string_view InIdentity,
	                const std::function<void(std::string_view, FVec4)>& InObserve = {});
	bool EditRecord(FRecordSelectionDraft& InDraft, std::string_view InIdentity,
	                const std::function<void(std::string_view, FVec4)>& InObserve = {},
	                std::span<const std::string> InReadOnlyFields = {});
	bool EditMixedScalar(FArchiveNode& InValue, const FRecordValueShape& InShape,
	                     const FPropertyPresentation& InPresentation, bool bInMixed);
	bool InputVector(const char* InLabel, FVec3& InValue);
	bool InputVectorRow(const char* InLabel, FVec3& InValue, std::string_view InUnit, std::string_view InTooltip,
	                    std::array<FVec4, 3>& OutBounds, const std::array<bool, 3>& InMixed = {},
	                    std::array<bool, 3>* OutEdited = nullptr);
	// Linear RGB value; UI uses sRGB, with integer channels in [0, 255].
	bool InputColor(const char* InLabel, FVec3& InValue,
	                const std::function<void(std::string_view, FVec4)>& InObserve = {},
	                const std::array<bool, 3>& InMixed = {}, std::array<bool, 3>* OutEdited = nullptr);
	bool InputMatrix(const char* InLabel, FMat4& InValue);
	FVec4 LastItemBounds();
	bool EditProperties(const FTypeDescriptor& InType, void* InObject, std::span<const std::string_view> InIds);
	void Plot(const char* InLabel, std::span<const float> InValues, float InMaximum);
	FGuiDrawData Render();
	void UseEditorStyle();
	void LoadLayout(std::string_view InLayout);
	std::string SaveLayout();
	// Call after menu/tool/status bars so docking consumes their current-frame reservations.
	void DockSpace(const FGuiDockLayout& InLayout, bool bInReset = false);
	bool BeginWindow(const char* InTitle, bool& bInOpen);
	void EndWindow();
	bool BeginMenuBar();
	void EndMenuBar();
	bool BeginToolbar();
	void EndToolbar();
	bool BeginMenu(const char* InLabel);
	void EndMenu();
	bool MenuItem(const char* InLabel, const char* InShortcut = nullptr, bool bInSelected = false);
	void SameLine();
	// Keep the next button on this line only when its label and padding fit.
	void SameLineIfFits(const char* InButtonLabel);
	void SetNextItemWidth(float InWidth);
	bool Section(const char* InLabel, bool bInDefaultOpen = true);
	bool BeginTable(const char* InId, const char* InFirst, const char* InSecond);
	void NextRow();
	void NextColumn();
	void EndTable();
	bool TreeItem(const char* InId, const char* InLabel, bool bInLeaf, bool bInSelected, bool& bOutClicked,
	              bool bInDefaultOpen = true);
	void EndTree();
	void Property(const char* InLabel, const std::string& InValue);
	FGuiImageRegion Image(std::uint64_t InTextureId);
	void Image(std::uint64_t InTextureId, FVec2 InSize);
	void FocusWindow(const char* InTitle);
	// Attach to the preceding item. Payload bytes are copied for the entire gesture.
	bool DragSource(const char* InType, std::string_view InValue, const char* InLabel);
	std::optional<FGuiDragPayload> DragPayload() const;
	// Delivery consumes the gesture; the returned owned payload remains valid after this call.
	std::optional<FGuiDragPayload> DropTarget(const char* InType);
	void CancelDragDrop();
	void DrawImageOverlay(std::uint64_t InTextureId, FVec4 InClip, FVec4 InBounds, FVec4 InTint = {1, 1, 1, 1});
	FGuiPointerState PointerState() const;
	// Own a left drag started on an image overlay, including motion beyond the image bounds.
	void CaptureImagePointer(bool bInCapture);
	// Current window draw list, clipped to a logical image rectangle; never draws over other windows.
	void DrawImageOverlay(FVec4 InClip, std::span<const FVec2> InPoints, FVec4 InColor, float InThickness,
	                      bool bInFilled = false);
	void OpenPopup(const char* InTitle);
	bool BeginPopup(const char* InId);
	void EndPopup();
	bool BeginModal(const char* InTitle, bool& bInOpen);
	void EndModal();
	void ClosePopup();
	bool IsEditingText() const;
	void BeginLiveEdit();
	FGuiEditState EndLiveEdit();
	void FinishEditing();
	void StatusBar(const std::string& InText);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
