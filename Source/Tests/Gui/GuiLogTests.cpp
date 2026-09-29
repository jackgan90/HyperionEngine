#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>
#include <regex>

namespace
{
using namespace Hyperion;

std::string WindowSection(const std::string& InLayout, const std::string& InName)
{
	const auto Start = InLayout.find("[Window][" + InName + "]");
	HYP_CHECK(Start != std::string::npos);
	const auto End = InLayout.find("\n[", Start + 1);
	return InLayout.substr(Start, End - Start);
}

std::string DockId(const std::string& InLayout, const std::string& InName)
{
	std::smatch Match;
	const auto Section = WindowSection(InLayout, InName);
	HYP_CHECK(std::regex_search(Section, Match, std::regex("DockId=(0x[0-9A-Fa-f]+)")));
	return Match[1];
}

void CheckDocking()
{
	FGui Gui;
	Gui.UseEditorStyle();
	bool bOpen = true;
	const auto Frame = [&](bool bInLog, bool bInReset)
	{
		Gui.BeginFrame({900, 700}, {900, 700}, .016f, {});
		Gui.DockSpace({"View", "Outliner", "Details", "Content Browser", "Place Object", .25f,
		               bInLog ? std::vector<std::string>{"Log"} : std::vector<std::string>{}},
		              bInReset);
		for (const char* Name : {"View", "Outliner", "Details", "Content Browser", "Place Object"})
		{
			Gui.BeginWindow(Name, bOpen);
			Gui.EndWindow();
		}
		if (bInLog)
		{
			Gui.BeginWindow("Log", bOpen);
			Gui.EndWindow();
		}
		Gui.Render();
	};
	Frame(false, false);
	Frame(false, false);
	const auto Old = Gui.SaveLayout();
	Frame(true, false);
	const auto Added = Gui.SaveLayout();
	HYP_CHECK(DockId(Old, "View") == DockId(Added, "View"));
	HYP_CHECK(DockId(Added, "Log") == DockId(Added, "Content Browser"));
	auto Floating = Added;
	const auto Section = WindowSection(Floating, "Log");
	Floating.replace(Floating.find(Section), Section.size(), "[Window][Log]\nPos=73,91\nSize=420,230\nCollapsed=0\n");
	Gui.LoadLayout(Floating);
	Frame(true, false);
	Frame(true, false);
	HYP_CHECK(WindowSection(Gui.SaveLayout(), "Log").find("DockId=") == std::string::npos);
	Frame(true, true);
	const auto Reset = Gui.SaveLayout();
	HYP_CHECK(DockId(Reset, "Log") == DockId(Reset, "Content Browser"));
}

void CheckRows()
{
	FGui Gui;
	Gui.UseEditorStyle();
	Gui.FontImage();
	Gui.SetPathDisplayRoot("/Game");
	bool bOpen = true;
	std::uint64_t Count = 10000;
	std::uint64_t First{};
	std::uint32_t Requested{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Requested = 0;
		Gui.BeginFrame({800, 600}, {800, 600}, .016f, InEvents);
		Gui.BeginWindow("Rows", bOpen, {600, 400}, {20, 20});
		Gui.TextRows("RowsBody", Count,
		             [&](std::uint64_t InFirst, std::uint32_t InCount)
		             {
			             First = InFirst;
			             Requested += InCount;
			             std::vector<FGuiTextRow> Rows;
			             for (std::uint32_t Index = 0; Index < InCount; ++Index)
			             {
				             Rows.push_back({"/Game/raw/log " + std::to_string(InFirst + Index),
				                             Index % 3 == 0   ? FVec4{1, .25f, .25f, 1}
				                             : Index % 3 == 1 ? FVec4{1, .85f, .15f, 1}
				                                              : FVec4{1, 1, 1, 1}});
			             }
			             return Rows;
		             });
		Gui.EndWindow();
		return Gui.Render();
	};
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		Frame();
	}
	const auto Draw = Frame();
	HYP_CHECK(First > 9900 && Requested < 100);
	for (std::uint32_t Color : {0xff4040ffu, 0xff26d9ffu, 0xffffffffu})
	{
		HYP_CHECK(std::any_of(Draw.Vertices.begin(), Draw.Vertices.end(),
		                      [Color](const auto& InVertex)
		                      {
			                      return InVertex.Color == Color;
		                      }));
	}
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = 250;
	Move.Y = 200;
	Frame({&Move, 1});
	FInputEvent Wheel;
	Wheel.Type = EEventType::MouseWheel;
	Wheel.Y = 30;
	Frame({&Wheel, 1});
	Frame();
	const auto Scrolled = First;
	HYP_CHECK(Scrolled < 9900);
	Count += 500;
	Frame();
	Frame();
	HYP_CHECK(First == Scrolled && Requested < 100);
}
} // namespace

int main()
{
	try
	{
		CheckDocking();
		CheckRows();
		std::cout << "Log docking upgrade/reset, virtual rows, colors and scroll retention passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
