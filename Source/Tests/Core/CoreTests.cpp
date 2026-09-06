#include "Support/TestSupport.h"
#include <Hyperion/Core/Core.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
	try
	{
		Hyperion::InitializeLog("test-core.log");
		Hyperion::Log(Hyperion::ELogLevel::Info, "core integration marker");
		std::ifstream File("test-core.log");
		const std::string Text((std::istreambuf_iterator<char>(File)), {});
		HYP_CHECK(Text.find("core integration marker") != std::string::npos);
		const auto Start = Hyperion::ClockNanoseconds();
		{
			Hyperion::FProfileScope Scope("Core integration");
			std::vector<std::thread> Threads;
			for (int I = 0; I < 4; ++I)
			{
				Threads.emplace_back(
				    []
				    {
					    for (int J = 0; J < 1000; ++J)
					    {
						    void* P = Hyperion::Allocate(257, 256);
						    HYP_CHECK(reinterpret_cast<std::uintptr_t>(P) % 256 == 0);
						    std::memset(P, 0xA5, 257);
						    Hyperion::Deallocate(P);
					    }
				    });
			}
			for (auto& T : Threads)
			{
				T.join();
			}
			Hyperion::FMemoryResource Resource(Hyperion::EMemoryTag::Assets);
			std::pmr::vector<int> Values(&Resource);
			Values.resize(4096);
			HYP_CHECK(Hyperion::MemoryStats(Hyperion::EMemoryTag::Assets).LiveBytes >= 4096 * sizeof(int));
		}
		HYP_CHECK(Hyperion::ClockNanoseconds() >= Start);
		HYP_CHECK(Hyperion::ProfileStats().Scopes == 1);
		HYP_CHECK(Hyperion::ProfileStats().Nanoseconds > 0);
		HYP_CHECK(Hyperion::MemoryStats(Hyperion::EMemoryTag::Core).LiveBytes == 0);
		HYP_CHECK(Hyperion::MemoryStats(Hyperion::EMemoryTag::Assets).LiveBytes == 0);
		bool Rejected = false;
		try
		{
			(void)Hyperion::Allocate(16, 3);
		}
		catch (const std::invalid_argument&)
		{
			Rejected = true;
		}
		HYP_CHECK(Rejected);
		Hyperion::ShutdownLog();
		std::cout << "Core integration passed\n";
		return 0;
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
