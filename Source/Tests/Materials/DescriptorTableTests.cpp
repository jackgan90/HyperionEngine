#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>

namespace
{
void CheckSharedSamplerCapacity()
{
	using namespace Hyperion;
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12);
	const auto Sampler = Device->CreateSampler({});
	const auto Buffer = Device->CreateBuffer({96 * 16, BufferUsage(ERHIBufferUsage::RawRead)});
	const auto Layout =
	    Device->CreateBindingLayout({{{ERHIBindingKind::RawBuffer, ERHIShaderVisibility::Pixel},
	                                  {ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel, 0, 0, 7}}});
	const auto Before = Device->Statistics();
	std::vector<FResourceBindingSet> Retained;
	// Three live resource generations of 32 materials share the same seven sampler values.
	for (unsigned Index = 0; Index < 96; ++Index)
	{
		FResourceBindingSetDesc Description;
		Description.Layout = Layout;
		Description.Entries = {{0, {FReadBufferView{Buffer, ERHIBufferViewKind::Raw, Index * 16, 16, 0}}},
		                       {1, std::vector<FResourceBindingValue>(7, Sampler)}};
		Retained.push_back(Device->CreateBindingSet(Description));
	}
	const auto After = Device->Statistics();
	HYP_CHECK(After.BindingSetsCreated - Before.BindingSetsCreated == 96);
	HYP_CHECK(After.DescriptorAllocations - Before.DescriptorAllocations == 96 + 7);
	HYP_CHECK(After.DescriptorCopies - Before.DescriptorCopies == 96 + 7);
	HYP_CHECK(After.ValidationErrors == 0);
	std::cout << "96 live resource sets share seven sampler descriptors within the default heap\n";
}

void CheckSamplerTableRetirement()
{
	using namespace Hyperion;
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	FRHIDeviceDesc Description;
	Description.SamplerDescriptorCapacity = 4;
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, Description);
	const auto A = Device->CreateSampler({});
	const auto Equivalent = Device->CreateSampler({});
	FSamplerDesc Other;
	Other.U = ERHIAddressMode::Clamp;
	const auto B = Device->CreateSampler(Other);
	Other.U = ERHIAddressMode::Mirror;
	const auto C = Device->CreateSampler(Other);
	const auto Layout = Device->CreateBindingLayout({{{ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel, 0},
	                                                  {ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel, 3}}});
	const auto Create = [&](const FSampler& InFirst, const FSampler& InSecond, bool bInReverse = false)
	{
		FResourceBindingSetDesc Set{Layout, {{0, {InFirst}}, {1, {InSecond}}}};
		if (bInReverse)
		{
			std::reverse(Set.Entries.begin(), Set.Entries.end());
		}
		return Device->CreateBindingSet(Set);
	};
	auto First = Create(A, B);
	const auto Before = Device->Statistics();
	auto Alias = Create(Equivalent, B, true);
	HYP_CHECK(Device->Statistics().DescriptorCopies == Before.DescriptorCopies);
	auto Reversed = Create(B, A);
	HYP_CHECK(Device->Statistics().DescriptorCopies == Before.DescriptorCopies + 2);
	const auto RejectThird = [&]
	{
		bool bRejected = false;
		try
		{
			Create(C, C);
		}
		catch (const std::runtime_error& Error)
		{
			bRejected = std::string_view(Error.what()).find("descriptor capacity exceeded") != std::string_view::npos;
		}
		HYP_CHECK(bRejected);
	};
	RejectThird();
	First = {};
	RejectThird(); // Another binding set still owns the shared range.
	Alias = {};
	auto Recovered = Create(C, C);
	HYP_CHECK(Recovered);
	Reversed = {};
	Recovered = {};
	for (unsigned Index = 0; Index < 100; ++Index)
	{
		auto Set = Index % 2 == 0 ? Create(A, B) : Create(B, C);
		HYP_CHECK(Set);
	}
	HYP_CHECK(Device->Statistics().ValidationErrors == 0);
	std::cout << "Sampler table ordering, alias ownership, exhaustion and retirement passed\n";
}
} // namespace

void RunDescriptorTableTests()
{
	CheckSharedSamplerCapacity();
	CheckSamplerTableRetirement();
}
