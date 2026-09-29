#include "Hyperion/Core/Logging/LogHistory.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace Hyperion
{
struct FLogHistory::FImpl
{
	std::filesystem::path Path;
	mutable std::mutex Mutex;
	mutable std::fstream File;
	std::vector<std::uint64_t> Offsets;
	std::string Failure;

	void String(std::string_view InText)
	{
		const auto Size = static_cast<std::uint32_t>(InText.size());
		File.write(reinterpret_cast<const char*>(&Size), sizeof(Size));
		File.write(InText.data(), Size);
	}

	std::string String() const
	{
		std::uint32_t Size{};
		File.read(reinterpret_cast<char*>(&Size), sizeof(Size));
		if (!File || Size > 8192)
		{
			throw std::runtime_error("Invalid log journal record");
		}
		std::string Result(Size, '\0');
		File.read(Result.data(), Size);
		return Result;
	}

	void Write(const FLogEntry& InEntry)
	{
		if (!Failure.empty())
		{
			throw std::runtime_error(Failure);
		}
		File.seekp(0, std::ios::end);
		const auto Offset = static_cast<std::uint64_t>(File.tellp());
		File.write(reinterpret_cast<const char*>(&InEntry.Level), sizeof(InEntry.Level));
		File.write(reinterpret_cast<const char*>(&InEntry.Thread), sizeof(InEntry.Thread));
		String(InEntry.Time);
		String(InEntry.Source);
		String(InEntry.Message);
		File.flush();
		if (!File)
		{
			Failure = "Could not write Editor log history: " + Path.string();
			throw std::runtime_error(Failure);
		}
		Offsets.push_back(Offset);
	}
};

FLogHistory::FLogHistory(const std::filesystem::path& InJournal) : Impl(std::make_unique<FImpl>())
{
	Impl->Path = InJournal;
	if (!InJournal.parent_path().empty())
	{
		std::filesystem::create_directories(InJournal.parent_path());
	}
	Impl->File.open(InJournal, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
	if (!Impl->File)
	{
		throw std::runtime_error("Could not create Editor log history: " + InJournal.string());
	}
}

FLogHistory::~FLogHistory()
{
	Impl->File.close();
	std::error_code Error;
	std::filesystem::remove(Impl->Path, Error);
}

void FLogHistory::Append(ELogLevel InLevel, std::string_view InMessage, std::string_view InSource)
{
	if (InSource.size() > 8192)
	{
		throw std::invalid_argument("Log source is too long");
	}
	const auto Now = std::chrono::system_clock::now();
	const auto Seconds = std::chrono::system_clock::to_time_t(Now);
	std::tm Local{};
	localtime_s(&Local, &Seconds);
	char Time[32]{};
	std::strftime(Time, sizeof(Time), "%H:%M:%S", &Local);
	const auto Millis = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch()).count() % 1000;
	const std::string Stamp = std::string(Time) + "." + std::to_string(1000 + Millis).substr(1);
	FLogEntry Entry{0, Stamp, std::hash<std::thread::id>{}(std::this_thread::get_id()), InLevel, std::string(InSource),
	                {}};
	std::lock_guard Lock(Impl->Mutex);
	do
	{
		const auto Newline = InMessage.find('\n');
		auto Length = std::min<std::size_t>(8192, std::min(Newline, InMessage.size()));
		// Do not split a UTF-8 code point when fragmenting a long physical line.
		if (Length < InMessage.size() && Length != Newline)
		{
			while (Length && (static_cast<unsigned char>(InMessage[Length]) & 0xc0) == 0x80)
			{
				--Length;
			}
			if (!Length)
			{
				Length = std::min<std::size_t>(8192, InMessage.size());
			}
		}
		Entry.Message.assign(InMessage.substr(0, Length));
		if (Length == Newline && !Entry.Message.empty() && Entry.Message.back() == '\r')
		{
			Entry.Message.pop_back();
		}
		Impl->Write(Entry);
		InMessage.remove_prefix(Length + (Length == Newline ? 1 : 0));
	} while (!InMessage.empty());
}

FLogPage FLogHistory::Read(const FLogReadRequest& InRequest) const
{
	std::lock_guard Lock(Impl->Mutex);
	if (!Impl->Failure.empty())
	{
		throw std::runtime_error(Impl->Failure);
	}
	if (!InRequest.Limit || InRequest.Limit > 256 || InRequest.After > Impl->Offsets.size())
	{
		throw std::invalid_argument("Log read requires an existing cursor and limit in [1, 256]");
	}
	FLogPage Result{{}, InRequest.After, Impl->Offsets.size()};
	std::size_t Bytes{};
	while (Result.Next < Result.Total && Result.Entries.size() < InRequest.Limit && Bytes < 48 * 1024)
	{
		Impl->File.seekg(Impl->Offsets[Result.Next]);
		FLogEntry Entry;
		Entry.Sequence = ++Result.Next;
		Impl->File.read(reinterpret_cast<char*>(&Entry.Level), sizeof(Entry.Level));
		Impl->File.read(reinterpret_cast<char*>(&Entry.Thread), sizeof(Entry.Thread));
		Entry.Time = Impl->String();
		Entry.Source = Impl->String();
		Entry.Message = Impl->String();
		if (!Impl->File)
		{
			throw std::runtime_error("Could not read Editor log history");
		}
		Bytes += Entry.Message.size() + Entry.Source.size() + 128;
		Result.Entries.push_back(std::move(Entry));
	}
	return Result;
}

std::uint64_t FLogHistory::Count() const
{
	std::lock_guard Lock(Impl->Mutex);
	return Impl->Offsets.size();
}
} // namespace Hyperion
