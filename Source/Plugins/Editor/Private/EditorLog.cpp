#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::DrawLog()
{
	if (!bShowLog)
	{
		return;
	}
	const bool bVisible = Gui->BeginWindow("Log", bShowLog);
	if (bFocusLog)
	{
		Gui->FocusWindow("Log");
		bFocusLog = false;
	}
	if (bVisible)
	{
		if (Options.LogHistory)
		{
			try
			{
				Gui->TextRows(
				    "LogRows", Options.LogHistory->Count(),
				    [this](std::uint64_t InFirst, std::uint32_t InCount)
				    {
					    std::vector<FGuiTextRow> Rows;
					    while (Rows.size() < InCount)
					    {
						    const auto Page = Options.LogHistory->Read(
						        {InFirst + Rows.size(),
						         std::min<std::uint32_t>(256, InCount - static_cast<std::uint32_t>(Rows.size()))});
						    if (Page.Entries.empty())
						    {
							    break;
						    }
						    for (const auto& Entry : Page.Entries)
						    {
							    FGuiTextRow Row;
							    Row.Text = "[" + Entry.Time + "] [" + LogLevelName(Entry.Level) + "] [" + Entry.Source +
							               "] " + Entry.Message;
							    if (Entry.Level == ELogLevel::Error)
							    {
								    Row.Color = {1, .25f, .25f, 1};
							    }
							    else if (Entry.Level == ELogLevel::Warning)
							    {
								    Row.Color = {1, .85f, .15f, 1};
							    }
							    Rows.push_back(std::move(Row));
						    }
					    }
					    return Rows;
				    });
			}
			catch (const std::exception& Failure)
			{
				Gui->TextWrapped("Log history unavailable: " + std::string(Failure.what()));
			}
		}
		else
		{
			Gui->Text("Log history is unavailable in this host.");
		}
	}
	Gui->EndWindow();
}
} // namespace Hyperion
