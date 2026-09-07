#include "Hyperion/Gui/Gui.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <imgui.h>
#include <implot.h>
#include <limits>
#include <stdexcept>
#include <thread>

namespace Hyperion
{
namespace
{
ImGuiKey Key(EKey InK)
{
	switch (InK)
	{
		case EKey::Tab:
			return ImGuiKey_Tab;
		case EKey::Left:
			return ImGuiKey_LeftArrow;
		case EKey::Right:
			return ImGuiKey_RightArrow;
		case EKey::Up:
			return ImGuiKey_UpArrow;
		case EKey::Down:
			return ImGuiKey_DownArrow;
		case EKey::PageUp:
			return ImGuiKey_PageUp;
		case EKey::PageDown:
			return ImGuiKey_PageDown;
		case EKey::Home:
			return ImGuiKey_Home;
		case EKey::End:
			return ImGuiKey_End;
		case EKey::Insert:
			return ImGuiKey_Insert;
		case EKey::Delete:
			return ImGuiKey_Delete;
		case EKey::Backspace:
			return ImGuiKey_Backspace;
		case EKey::Space:
			return ImGuiKey_Space;
		case EKey::Enter:
			return ImGuiKey_Enter;
		case EKey::Escape:
			return ImGuiKey_Escape;
		case EKey::A:
			return ImGuiKey_A;
		case EKey::C:
			return ImGuiKey_C;
		case EKey::V:
			return ImGuiKey_V;
		case EKey::X:
			return ImGuiKey_X;
		case EKey::Y:
			return ImGuiKey_Y;
		case EKey::Z:
			return ImGuiKey_Z;
		default:
			return ImGuiKey_None;
	}
}
} // namespace

struct FGui::FImpl
{
	ImGuiContext* Context{};
	ImPlotContext* Plot{};
	std::thread::id Thread = std::this_thread::get_id();
	FWindow* Window{};
	std::string Clipboard;

	void Select()
	{
		if (Thread != std::this_thread::get_id())
		{
			throw std::logic_error("GUI called from non-owning thread");
		}
		ImGui::SetCurrentContext(Context);
		ImPlot::SetCurrentContext(Plot);
	}

	~FImpl()
	{
		if (Plot)
		{
			ImPlot::DestroyContext(Plot);
		}
		if (Context)
		{
			ImGui::DestroyContext(Context);
		}
	}
};

FGui::FGui(FWindow* InWindow) : Impl(std::make_unique<FImpl>())
{
	ImGui::SetAllocatorFunctions(
	    [](size_t InSize, void*) -> void*
	    {
		    return Allocate(InSize, alignof(std::max_align_t), EMemoryTag::Gui);
	    },
	    [](void* InP, void*)
	    {
		    Deallocate(InP);
	    });
	Impl->Context = ImGui::CreateContext();
	Impl->Plot = ImPlot::CreateContext();
	Impl->Window = InWindow;
	auto& Io = ImGui::GetIO();
	Io.IniFilename = nullptr;
	Io.LogFilename = nullptr;
	Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	Io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	Io.BackendRendererName = "Hyperion RHI";
	Io.BackendPlatformName = "Hyperion Platform";
	auto& Platform = ImGui::GetPlatformIO();
	Platform.Platform_ClipboardUserData = Impl.get();
	Platform.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char*
	{
		auto Self = static_cast<FImpl*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
		if (Self->Window)
		{
			Self->Clipboard = Self->Window->Clipboard();
		}
		return Self->Clipboard.c_str();
	};
	Platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* InText)
	{
		auto Self = static_cast<FImpl*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
		Self->Clipboard = InText;
		if (Self->Window)
		{
			Self->Window->SetClipboard(InText);
		}
	};
	ImGui::StyleColorsDark();
	auto& Style = ImGui::GetStyle();
	Style.WindowRounding = 10;
	Style.FrameRounding = 4;
	Style.GrabRounding = 4;
	Style.WindowPadding = {18, 16};
	Style.ItemSpacing = {8, 5};
	Style.FramePadding = {8, 3};
	Style.WindowBorderSize = 1;
	Style.Colors[ImGuiCol_WindowBg] = {.045f, .065f, .095f, .97f};
	Style.Colors[ImGuiCol_Border] = {.16f, .23f, .31f, 1};
	Style.Colors[ImGuiCol_Text] = {.86f, .91f, .97f, 1};
	Style.Colors[ImGuiCol_TextDisabled] = {.44f, .54f, .64f, 1};
	Style.Colors[ImGuiCol_FrameBg] = {.09f, .14f, .2f, 1};
	Style.Colors[ImGuiCol_Button] = {.09f, .27f, .32f, 1};
	Style.Colors[ImGuiCol_ButtonHovered] = {.12f, .4f, .46f, 1};
	Style.Colors[ImGuiCol_CheckMark] = {.27f, .87f, .74f, 1};
	Style.Colors[ImGuiCol_SliderGrab] = {.27f, .87f, .74f, 1};
	ImPlot::StyleColorsDark();
}

FGui::~FGui() = default;

FImage FGui::FontImage()
{
	Impl->Select();
	unsigned char* Pixels{};
	int Width{};
	int Height{};
	auto Fonts = ImGui::GetIO().Fonts;
	Fonts->GetTexDataAsRGBA32(&Pixels, &Width, &Height);
	Fonts->SetTexID(ImTextureID(1));
	FImage Image{static_cast<std::uint32_t>(Width), static_cast<std::uint32_t>(Height), EColorSpace::Linear, {}};
	Image.Rgba.resize(std::size_t(Width) * Height * 4);
	for (std::size_t I = 0; I < Image.Rgba.size(); ++I)
	{
		Image.Rgba[I] = Pixels[I] / 255.f;
	}
	return Image;
}

void FGui::BeginFrame(FSize InLogical, FSize InPixels, float InDelta, std::span<const FInputEvent> InEvents)
{
	Impl->Select();
	if (!InLogical.Width || !InLogical.Height || !InPixels.Width || !InPixels.Height)
	{
		throw std::invalid_argument("GUI frame dimensions");
	}
	auto& Io = ImGui::GetIO();
	Io.DisplaySize = {float(InLogical.Width), float(InLogical.Height)};
	Io.DisplayFramebufferScale = {float(InPixels.Width) / InLogical.Width, float(InPixels.Height) / InLogical.Height};
	Io.DeltaTime = std::max(InDelta, .00001f);
	for (const auto& E : InEvents)
	{
		switch (E.Type)
		{
			case EEventType::MouseMove:
				Io.AddMousePosEvent(E.X, E.Y);
				break;
			case EEventType::MouseButton:
				if (E.Button < 5)
				{
					Io.AddMouseButtonEvent(static_cast<int>(E.Button), E.bDown);
				}
				break;
			case EEventType::MouseWheel:
				Io.AddMouseWheelEvent(E.X, E.Y);
				break;
			case EEventType::Focus:
				Io.AddFocusEvent(E.bDown);
				break;
			case EEventType::Text:
				Io.AddInputCharactersUTF8(E.Text.c_str());
				break;
			case EEventType::Key:
				Io.AddKeyEvent(ImGuiMod_Ctrl, (E.Modifiers & 1) != 0);
				Io.AddKeyEvent(ImGuiMod_Shift, (E.Modifiers & 2) != 0);
				Io.AddKeyEvent(ImGuiMod_Alt, (E.Modifiers & 4) != 0);
				Io.AddKeyEvent(ImGuiMod_Super, (E.Modifiers & 8) != 0);
				if (auto Mapped = Key(E.Key); Mapped != ImGuiKey_None)
				{
					Io.AddKeyEvent(Mapped, E.bDown);
				}
				break;
			default:
				break;
		}
	}
	ImGui::NewFrame();
}

bool FGui::BeginPanel(const char* InTitle, FVec2 InPosition, FVec2 InSize)
{
	Impl->Select();
	ImGui::SetNextWindowPos({InPosition.X, InPosition.Y});
	ImGui::SetNextWindowSize({InSize.X, InSize.Y});
	return ImGui::Begin(InTitle, nullptr,
	                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
	                        ImGuiWindowFlags_NoCollapse);
}

void FGui::EndPanel()
{
	Impl->Select();
	ImGui::End();
}

void FGui::Text(const std::string& InValue)
{
	Impl->Select();
	ImGui::TextUnformatted(InValue.c_str());
}

void FGui::TextWrapped(const std::string& InValue)
{
	Impl->Select();
	ImGui::PushTextWrapPos(0);
	ImGui::TextUnformatted(InValue.c_str());
	ImGui::PopTextWrapPos();
}

bool FGui::WantsMouse() const
{
	Impl->Select();
	return ImGui::GetIO().WantCaptureMouse;
}

bool FGui::WantsKeyboard() const
{
	Impl->Select();
	return ImGui::GetIO().WantCaptureKeyboard;
}

void FGui::Separator()
{
	Impl->Select();
	ImGui::Separator();
}

bool FGui::Button(const char* InLabel, bool bInEnabled)
{
	Impl->Select();
	ImGui::BeginDisabled(!bInEnabled);
	const bool bPressed = ImGui::Button(InLabel);
	ImGui::EndDisabled();
	return bPressed;
}

bool FGui::Checkbox(const char* InLabel, bool& bInValue)
{
	Impl->Select();
	return ImGui::Checkbox(InLabel, &bInValue);
}

bool FGui::Slider(const char* InLabel, float& InValue, float InMinimum, float InMaximum)
{
	Impl->Select();
	return ImGui::SliderFloat(InLabel, &InValue, InMinimum, InMaximum, "%.2f");
}

FVec4 FGui::LastItemBounds()
{
	Impl->Select();
	auto A = ImGui::GetItemRectMin();
	auto B = ImGui::GetItemRectMax();
	return {A.x, A.y, B.x, B.y};
}

bool FGui::EditProperties(const FTypeDescriptor& InType, void* InObject, std::span<const std::string_view> InIds)
{
	Impl->Select();
	bool bEdited = false;
	ImGui::PushItemWidth(155);
	for (const auto& P : InType.Properties)
	{
		if (std::find(InIds.begin(), InIds.end(), P.Id) == InIds.end())
		{
			continue;
		}
		auto Value = P.Get(InObject);
		bool bChanged = false;
		ImGui::PushID(P.Id.c_str());
		if (P.Kind == EPropertyKind::Boolean)
		{
			auto bV = std::get<bool>(Value);
			bChanged = ImGui::Checkbox(P.Label.c_str(), &bV);
			Value = bV;
		}
		if (P.Kind == EPropertyKind::Number)
		{
			auto V = std::get<double>(Value);
			bChanged = ImGui::SliderScalar(P.Label.c_str(), ImGuiDataType_Double, &V, &P.Minimum, &P.Maximum, "%.3f");
			Value = V;
		}
		if (P.Kind == EPropertyKind::Integer)
		{
			auto V = std::get<std::int64_t>(Value);
			auto Low = static_cast<std::int64_t>(P.Minimum);
			auto High = static_cast<std::int64_t>(P.Maximum);
			bChanged = ImGui::SliderScalar(P.Label.c_str(), ImGuiDataType_S64, &V, &Low, &High);
			Value = V;
		}
		if (bChanged)
		{
			P.Set(InObject, Value);
			bEdited = true;
		}
		ImGui::PopID();
	}
	ImGui::PopItemWidth();
	return bEdited;
}

void FGui::Plot(const char* InLabel, std::span<const float> InValues, float InMaximum)
{
	Impl->Select();
	if (ImPlot::BeginPlot(InLabel, {-1, 105},
	                      ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText |
	                          ImPlotFlags_NoBoxSelect))
	{
		ImPlot::SetupAxes(nullptr, "ms", ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines,
		                  ImPlotAxisFlags_AutoFit);
		ImPlot::SetupAxesLimits(0, std::max(1.f, float(InValues.size() - std::min<std::size_t>(1, InValues.size()))), 0,
		                        InMaximum, ImPlotCond_Always);
		if (!InValues.empty())
		{
			ImPlot::PlotLine("CPU frame", InValues.data(), static_cast<int>(InValues.size()));
		}
		ImPlot::EndPlot();
	}
}

FVec2 FGui::DisplaySize() const
{
	Impl->Select();
	const auto Size = ImGui::GetIO().DisplaySize;
	return {Size.x, Size.y};
}

void FGui::OverlayLine(FVec2 InNormalizedA, FVec2 InNormalizedB, std::uint32_t InColor)
{
	Impl->Select();
	const auto Size = ImGui::GetIO().DisplaySize;
	ImGui::GetBackgroundDrawList()->AddLine({InNormalizedA.X * Size.x, InNormalizedA.Y * Size.y},
	                                        {InNormalizedB.X * Size.x, InNormalizedB.Y * Size.y}, InColor, 1.5f);
}

FGuiDrawData FGui::Render()
{
	Impl->Select();
	ImGui::Render();
	const auto* Source = ImGui::GetDrawData();
	FGuiDrawData Out;
	Out.DisplayPosition = {Source->DisplayPos.x, Source->DisplayPos.y};
	Out.DisplaySize = {Source->DisplaySize.x, Source->DisplaySize.y};
	Out.FramebufferScale = {Source->FramebufferScale.x, Source->FramebufferScale.y};
	Out.Vertices.reserve(Source->TotalVtxCount);
	Out.Indices.reserve(Source->TotalIdxCount);
	for (const auto* List : Source->CmdLists)
	{
		auto VertexBase = static_cast<std::int32_t>(Out.Vertices.size());
		auto IndexBase = static_cast<std::uint32_t>(Out.Indices.size());
		for (const auto& V : List->VtxBuffer)
		{
			Out.Vertices.push_back({{V.pos.x, V.pos.y}, {V.uv.x, V.uv.y}, V.col});
		}
		for (auto I : List->IdxBuffer)
		{
			Out.Indices.push_back(I);
		}
		for (const auto& Cmd : List->CmdBuffer)
		{
			if (Cmd.UserCallback == ImDrawCallback_ResetRenderState)
			{
				continue;
			}
			if (Cmd.UserCallback)
			{
				throw std::runtime_error("Custom GUI callbacks are unsupported");
			}
			if (Cmd.GetTexID() != 1)
			{
				throw std::runtime_error("GUI currently supports its static font atlas only");
			}
			Out.Commands.push_back({{Cmd.ClipRect.x, Cmd.ClipRect.y, Cmd.ClipRect.z, Cmd.ClipRect.w},
			                        Cmd.ElemCount,
			                        IndexBase + Cmd.IdxOffset,
			                        VertexBase + static_cast<std::int32_t>(Cmd.VtxOffset)});
		}
	}
	return Out;
}
} // namespace Hyperion
