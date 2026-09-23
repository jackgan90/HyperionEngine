#include "WindowsPipeInternal.h"
#include <algorithm>
#include <array>
#include <deque>

namespace Hyperion
{
namespace
{
constexpr std::size_t MaxQueuedBytes = 8 * 1024 * 1024;

struct FPipeOperation
{
	FPipeHandle Event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
	OVERLAPPED Overlapped{};
	bool bPending{};

	FPipeOperation()
	{
		if (!Event.Value)
		{
			throw FTransportError("transport_failed", "Could not create pipe IO event");
		}
		Overlapped.hEvent = Event.Value;
	}

	void Reset()
	{
		ResetEvent(Event.Value);
		Overlapped = {};
		Overlapped.hEvent = Event.Value;
	}

	void Drain(HANDLE InPipe) noexcept
	{
		if (bPending)
		{
			CancelIoEx(InPipe, &Overlapped);
			DWORD Count{};
			GetOverlappedResult(InPipe, &Overlapped, &Count, TRUE);
			bPending = false;
		}
	}
};

class FPipeConnection final : public ITransportConnection
{
public:
	explicit FPipeConnection(FPipeHandle&& InHandle) : Pipe(std::move(InHandle))
	{
		const auto User = CurrentPipeUser();
		// Windows' canonical SID spelling contains ASCII characters only.
		for (const wchar_t Character : User)
		{
			PeerInfo.Principal.push_back(static_cast<char>(Character));
		}
	}

	~FPipeConnection() override
	{
		Close();
	}

	void Poll() override;
	bool Send(FTransportBytes InBytes) override;

	std::size_t WriteCapacity() const noexcept override
	{
		return CurrentState == ETransportState::Connected && Writes.size() < 128 ? MaxQueuedBytes - QueuedBytes : 0;
	}

	bool HasPendingWrites() const noexcept override
	{
		return !Writes.empty();
	}

	FTransportBytes Receive() override
	{
		return std::exchange(Received, {});
	}

	ETransportState State() const noexcept override
	{
		return CurrentState;
	}

	std::string Error() const override
	{
		return Failure;
	}

	const FTransportPeer& Peer() const noexcept override
	{
		return PeerInfo;
	}

	void Close() noexcept override;

private:
	bool Complete(FPipeOperation& InOperation, DWORD& OutCount);
	void Fail(DWORD InError);
	void Read();
	void Write();
	FPipeHandle Pipe;
	FTransportPeer PeerInfo{true, true, true, false, {}, "windows-token"};
	FPipeOperation Reader;
	FPipeOperation Writer;
	std::array<std::byte, 65536> ReadBuffer{};
	FTransportBytes Received;
	std::deque<FTransportBytes> Writes;
	std::size_t WriteOffset{};
	std::size_t QueuedBytes{};
	ETransportState CurrentState = ETransportState::Connected;
	std::string Failure;
};

void FPipeConnection::Fail(DWORD InError)
{
	if (InError == ERROR_BROKEN_PIPE || InError == ERROR_NO_DATA || InError == ERROR_PIPE_NOT_CONNECTED)
	{
		CurrentState = ETransportState::Closed;
	}
	else
	{
		CurrentState = ETransportState::Failed;
		Failure = "Pipe IO failed: " + std::to_string(InError);
	}
}

bool FPipeConnection::Complete(FPipeOperation& InOperation, DWORD& OutCount)
{
	if (GetOverlappedResult(Pipe.Value, &InOperation.Overlapped, &OutCount, FALSE))
	{
		InOperation.bPending = false;
		return true;
	}
	const auto ErrorCode = GetLastError();
	if (ErrorCode != ERROR_IO_INCOMPLETE)
	{
		InOperation.bPending = false;
		Fail(ErrorCode);
	}
	return false;
}

void FPipeConnection::Read()
{
	if (!Received.empty())
	{
		return;
	}
	DWORD Count{};
	if (Reader.bPending)
	{
		if (!Complete(Reader, Count))
		{
			return;
		}
	}
	else
	{
		Reader.Reset();
		if (!ReadFile(Pipe.Value, ReadBuffer.data(), static_cast<DWORD>(ReadBuffer.size()), &Count, &Reader.Overlapped))
		{
			const auto ErrorCode = GetLastError();
			if (ErrorCode == ERROR_IO_PENDING)
			{
				Reader.bPending = true;
			}
			else
			{
				Fail(ErrorCode);
			}
			return;
		}
	}
	Received.assign(ReadBuffer.begin(), ReadBuffer.begin() + Count);
}

void FPipeConnection::Write()
{
	if (Writes.empty())
	{
		return;
	}
	DWORD Count{};
	if (Writer.bPending)
	{
		if (!Complete(Writer, Count))
		{
			return;
		}
	}
	else
	{
		Writer.Reset();
		const auto& Bytes = Writes.front();
		const auto Remaining = static_cast<DWORD>(std::min<std::size_t>(65536, Bytes.size() - WriteOffset));
		if (!WriteFile(Pipe.Value, Bytes.data() + WriteOffset, Remaining, &Count, &Writer.Overlapped))
		{
			const auto ErrorCode = GetLastError();
			if (ErrorCode == ERROR_IO_PENDING)
			{
				Writer.bPending = true;
			}
			else
			{
				Fail(ErrorCode);
			}
			return;
		}
	}
	WriteOffset += Count;
	QueuedBytes -= Count;
	if (WriteOffset == Writes.front().size())
	{
		Writes.pop_front();
		WriteOffset = 0;
	}
}

void FPipeConnection::Poll()
{
	if (CurrentState == ETransportState::Connected)
	{
		Read();
		if (CurrentState == ETransportState::Connected)
		{
			Write();
		}
	}
}

bool FPipeConnection::Send(FTransportBytes InBytes)
{
	if (CurrentState != ETransportState::Connected)
	{
		throw FTransportError("disconnected", "Pipe connection is closed");
	}
	if (InBytes.size() > MaxQueuedBytes - QueuedBytes || Writes.size() >= 128)
	{
		return false;
	}
	if (!InBytes.empty())
	{
		QueuedBytes += InBytes.size();
		Writes.push_back(std::move(InBytes));
	}
	return true;
}

void FPipeConnection::Close() noexcept
{
	if (Pipe.Value != INVALID_HANDLE_VALUE)
	{
		Reader.Drain(Pipe.Value);
		Writer.Drain(Pipe.Value);
		CloseHandle(std::exchange(Pipe.Value, INVALID_HANDLE_VALUE));
	}
	Writes.clear();
	QueuedBytes = 0;
	if (CurrentState != ETransportState::Failed)
	{
		CurrentState = ETransportState::Closed;
	}
}

class FPipeListener final : public ITransportListener
{
public:
	explicit FPipeListener(std::string InAddress) : Address(NativePipeName(InAddress))
	{
		Begin(true);
	}

	~FPipeListener() override
	{
		Close();
	}

	std::unique_ptr<ITransportConnection> Accept() override;

	void Close() noexcept override
	{
		bClosed = true;
		if (Pipe.Value != INVALID_HANDLE_VALUE)
		{
			AcceptOperation.Drain(Pipe.Value);
			CloseHandle(std::exchange(Pipe.Value, INVALID_HANDLE_VALUE));
		}
	}

private:
	void Begin(bool bInFirst);
	std::wstring Address;
	FPipeSecurity Security;
	FPipeHandle Pipe;
	FPipeOperation AcceptOperation;
	bool bClosed{};
};

void FPipeListener::Begin(bool bInFirst)
{
	Pipe.Value = CreateNamedPipeW(
	    Address.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (bInFirst ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
	    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 64, 65536, 65536, 0,
	    &Security.Attributes);
	if (Pipe.Value == INVALID_HANDLE_VALUE)
	{
		throw FTransportError("listen_failed", "Could not create local pipe: " + std::to_string(GetLastError()));
	}
	AcceptOperation.Reset();
	if (!ConnectNamedPipe(Pipe.Value, &AcceptOperation.Overlapped))
	{
		const auto ErrorCode = GetLastError();
		if (ErrorCode == ERROR_IO_PENDING)
		{
			AcceptOperation.bPending = true;
		}
		else if (ErrorCode != ERROR_PIPE_CONNECTED)
		{
			throw FTransportError("listen_failed", "Could not accept pipe connection");
		}
	}
}

std::unique_ptr<ITransportConnection> FPipeListener::Accept()
{
	if (bClosed)
	{
		return {};
	}
	if (Pipe.Value == INVALID_HANDLE_VALUE)
	{
		Begin(false);
	}
	DWORD Count{};
	if (AcceptOperation.bPending && !GetOverlappedResult(Pipe.Value, &AcceptOperation.Overlapped, &Count, FALSE))
	{
		if (GetLastError() == ERROR_IO_INCOMPLETE)
		{
			return {};
		}
		AcceptOperation.bPending = false;
		CloseHandle(std::exchange(Pipe.Value, INVALID_HANDLE_VALUE));
		return {};
	}
	AcceptOperation.bPending = false;
	return std::make_unique<FPipeConnection>(std::move(Pipe));
}

class FPipeProvider final : public ITransportProvider
{
public:
	std::string Scheme() const override
	{
		return "npipe";
	}

	std::unique_ptr<ITransportConnection> Connect(const std::string& InAddress) override
	{
		const auto Name = NativePipeName(InAddress);
		FPipeHandle Pipe(CreateFileW(Name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
		                             FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
		if (Pipe.Value == INVALID_HANDLE_VALUE)
		{
			throw FTransportError("connection_failed", "Local target unavailable: " + std::to_string(GetLastError()));
		}
		VerifyPipeServerUser(Pipe.Value);
		return std::make_unique<FPipeConnection>(std::move(Pipe));
	}

	std::unique_ptr<ITransportListener> Listen(const std::string& InAddress) override
	{
		return std::make_unique<FPipeListener>(InAddress);
	}
};
} // namespace

void RegisterLocalTransport(FTransportRegistry& InRegistry)
{
	InRegistry.Register(std::make_unique<FPipeProvider>());
}
} // namespace Hyperion
