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
};

struct FGuiImageRegion
{
	FVec4 Bounds;
	bool bHovered{};
	bool bFocused{};
};

struct FGuiDockLayout
{
	std::string Center;
	std::string RightTop;
	std::string RightBottom;
	std::string Bottom;
};

// Interaction tokens identify one activation/drag/popup, rather than individual changed frames.
struct FGuiEditState
{
	std::uint64_t ChangedInteraction{};
	std::uint64_t ActiveInteraction{};
};

// All GUI context and widget operations belong to the creating Main thread.
class FGui
{
public:
	explicit FGui(FWindow* InClipboardWindow = nullptr);
	~FGui();
	FImage FontImage();
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
	bool Checkbox(const char* InLabel, bool& bInValue);
	bool Slider(const char* InLabel, float& InValue, float InMinimum, float InMaximum);
	bool Selectable(const char* InLabel, bool bInSelected, unsigned InDepth = 0);
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
	bool InputVector(const char* InLabel, FVec3& InValue);
	bool InputVectorRow(const char* InLabel, FVec3& InValue, std::string_view InUnit, std::string_view InTooltip,
	                    std::array<FVec4, 3>& OutBounds);
	// Linear RGB value; UI uses sRGB, with integer channels in [0, 255].
	bool InputColor(const char* InLabel, FVec3& InValue,
	                const std::function<void(std::string_view, FVec4)>& InObserve = {});
	bool InputMatrix(const char* InLabel, FMat4& InValue);
	FVec4 LastItemBounds();
	bool EditProperties(const FTypeDescriptor& InType, void* InObject, std::span<const std::string_view> InIds);
	void Plot(const char* InLabel, std::span<const float> InValues, float InMaximum);
	FGuiDrawData Render();
	void UseEditorStyle();
	void LoadLayout(std::string_view InLayout);
	std::string SaveLayout();
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
	void OpenPopup(const char* InTitle);
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
