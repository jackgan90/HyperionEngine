#include "Hyperion/IO/IOService.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <thread>

int main()
{
	using namespace Hyperion;
	try
	{
		FTaskSystem Tasks(1, 1);
		FIOService IO(Tasks);
		const auto Path = std::filesystem::path(L"io-test/模型.bin");
		const FBytes Bytes{std::byte{1}, std::byte{2}, std::byte{3}};
		IO.WriteAsync(Path, Bytes).Get(Tasks);
		auto Work = DispatchAsync<bool>(Tasks, {EDomain::Worker},
		                                [&]
		                                {
			                                return *IO.ReadAsync(Path).Get(Tasks) == Bytes;
		                                });
		HYP_CHECK(*Work.Get(Tasks));
		bool Failed = false;
		try
		{
			IO.ReadAsync(Path, {}, 2).Get(Tasks);
		}
		catch (...)
		{
			Failed = true;
		}
		HYP_CHECK(Failed);
		FCancellationToken Token;
		Token.Cancel();
		Failed = false;
		try
		{
			IO.ReadAsync(Path, Token).Get(Tasks);
		}
		catch (...)
		{
			Failed = true;
		}
		HYP_CHECK(Failed);
		std::thread::id IOId;
		Tasks.Wait(Tasks.Dispatch({EDomain::Io},
		                          [&]
		                          {
			                          IOId = std::this_thread::get_id();
		                          }));
		HYP_CHECK(IOId != std::this_thread::get_id());
		for (const auto& Stat : Tasks.Statistics())
		{
			if (Stat.Name == "IO")
			{
				HYP_CHECK(Stat.Executed >= 4);
			}
		}
		auto Memory = std::make_shared<FMemoryFileSystem>();
		FIOService MemoryIO(Tasks, Memory);
		MemoryIO.WriteAsync("virtual", Bytes).Get(Tasks);
		HYP_CHECK(*MemoryIO.ReadAsync("virtual").Get(Tasks) == Bytes);
		HYP_CHECK(IO.Statistics().ReadBytes == 3);
		Tasks.Shutdown();
		std::cout << "IO thread and storage checks passed\n";
	}
	catch (const std::exception& InError)
	{
		std::cerr << InError.what() << '\n';
		return 1;
	}
}
