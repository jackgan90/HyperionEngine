#pragma once
#include "Hyperion/Core/Logging/LogHistory.h"
#include <algorithm>

namespace Hyperion
{
class FTestLogCapture
{
public:
	explicit FTestLogCapture(const std::filesystem::path& InDirectory)
	    : History(std::make_shared<FLogHistory>(InDirectory / "History.bin"))
	{
		InitializeEditorLog(InDirectory / "Output.log", History, {});
	}

	~FTestLogCapture()
	{
		ShutdownLog();
		InitializeStderrLog();
	}

	std::size_t Count(ELogLevel InLevel, std::initializer_list<std::string_view> InText) const
	{
		std::size_t Result{};
		std::uint64_t Cursor{};
		while (Cursor < History->Count())
		{
			const auto Page = History->Read({Cursor, 256});
			for (const auto& Entry : Page.Entries)
			{
				Result +=
				    Entry.Level == InLevel && std::all_of(InText.begin(), InText.end(),
				                                          [&](std::string_view InPart)
				                                          {
					                                          return Entry.Message.find(InPart) != std::string::npos;
				                                          });
			}
			Cursor = Page.Next;
		}
		return Result;
	}

	std::shared_ptr<FLogHistory> History;
};
} // namespace Hyperion
