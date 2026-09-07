#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Platform/Window.h"
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

// All GUI context and widget operations belong to the creating Main thread.
class FGui
{
public:
	explicit FGui(FWindow* InClipboardWindow = nullptr);
	~FGui();
	FImage FontImage();
	void BeginFrame(FSize InLogical, FSize InPixels, float InDeltaSeconds, std::span<const FInputEvent> InEvents);
	bool BeginPanel(const char* InTitle, FVec2 InPosition, FVec2 InSize);
	void EndPanel();
	void Text(const std::string& InValue);
	void TextWrapped(const std::string& InValue);
	FVec2 DisplaySize() const;
	void OverlayLine(FVec2 InNormalizedA, FVec2 InNormalizedB, std::uint32_t InColor);
	bool WantsMouse() const;
	bool WantsKeyboard() const;
	void Separator();
	bool Button(const char* InLabel, bool bInEnabled = true);
	bool Checkbox(const char* InLabel, bool& bInValue);
	bool Slider(const char* InLabel, float& InValue, float InMinimum, float InMaximum);
	FVec4 LastItemBounds();
	bool EditProperties(const FTypeDescriptor& InType, void* InObject, std::span<const std::string_view> InIds);
	void Plot(const char* InLabel, std::span<const float> InValues, float InMaximum);
	FGuiDrawData Render();

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
