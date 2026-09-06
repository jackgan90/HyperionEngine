#include "Hyperion/Assets/Assets.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>
#include <stb_image_write.h>
#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_OPENMP 0
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tinyexr.h>

namespace Hyperion
{
namespace
{
std::size_t Pixels(std::uint32_t InW, std::uint32_t InH)
{
	if (!InW || !InH || InW > 32768 || InH > 32768)
	{
		throw std::runtime_error("Image dimensions out of range");
	}
	return std::size_t(InW) * InH * 4;
}

void ExrError(int InResult, const char* InError)
{
	std::string Message = InError ? InError : "EXR operation failed";
	if (InError)
	{
		FreeEXRErrorMessage(InError);
	}
	if (InResult != TINYEXR_SUCCESS)
	{
		throw std::runtime_error(Message);
	}
}
} // namespace

FImagePixels DecodeImage(std::span<const std::byte> InBytes)
{
	if (InBytes.empty() || InBytes.size() > std::numeric_limits<int>::max())
	{
		throw std::runtime_error("Invalid encoded image size");
	}
	const auto* Bytes = reinterpret_cast<const unsigned char*>(InBytes.data());
	const auto Size = static_cast<int>(InBytes.size());
	int Width{};
	int Height{};
	int Channels{};
	if (!stbi_info_from_memory(Bytes, Size, &Width, &Height, &Channels) || Width <= 0 || Height <= 0 || Width > 16384 ||
	    Height > 16384 || std::uint64_t(Width) * Height * 4 > 256u * 1024u * 1024u)
	{
		throw std::runtime_error("Invalid image dimensions or decoded image exceeds 256 MiB");
	}
	auto* Raw = stbi_load_from_memory(Bytes, Size, &Width, &Height, &Channels, 4);
	std::unique_ptr<unsigned char, decltype(&stbi_image_free)> Holder(Raw, stbi_image_free);
	if (!Raw)
	{
		throw std::runtime_error("PNG/JPEG decode failed");
	}
	return {static_cast<std::uint32_t>(Width),
	        static_cast<std::uint32_t>(Height),
	        {Raw, Raw + std::size_t(Width) * Height * 4}};
}

std::vector<std::byte> EncodePng(const FImage& InImage)
{
	const auto Count = Pixels(InImage.Width, InImage.Height);
	if (InImage.Encoding != EColorSpace::Srgb || InImage.Rgba.size() != Count)
	{
		throw std::runtime_error("PNG requires encoded RGBA data");
	}
	std::vector<unsigned char> Pixels8(Count);
	for (std::size_t Index = 0; Index < Count; ++Index)
	{
		if (!std::isfinite(InImage.Rgba[Index]))
		{
			throw std::runtime_error("Non-finite PNG value");
		}
		Pixels8[Index] = static_cast<unsigned char>(std::lround(std::clamp(InImage.Rgba[Index], 0.f, 1.f) * 255.f));
	}
	int Size{};
	unsigned char* Raw =
	    stbi_write_png_to_mem(Pixels8.data(), static_cast<int>(InImage.Width * 4), static_cast<int>(InImage.Width),
	                          static_cast<int>(InImage.Height), 4, &Size);
	std::unique_ptr<unsigned char, decltype(&std::free)> Holder(Raw, std::free);
	if (!Raw || Size <= 0)
	{
		throw std::runtime_error("PNG encoding failed");
	}
	const auto* Bytes = reinterpret_cast<const std::byte*>(Raw);
	return {Bytes, Bytes + Size};
}

FImage LoadImageFile(const std::filesystem::path& InPath)
{
	FImage Result;
	int W{};
	int H{};
	auto Name = InPath.string();
	if (InPath.extension() == ".exr")
	{
		float* Raw{};
		const char* Error{};
		auto Status = LoadEXR(&Raw, &W, &H, Name.c_str(), &Error);
		std::unique_ptr<float, decltype(&std::free)> Holder(Raw, std::free);
		ExrError(Status, Error);
		auto N = Pixels(static_cast<std::uint32_t>(W), static_cast<std::uint32_t>(H));
		Result.Rgba.assign(Raw, Raw + N);
	}
	else if (InPath.extension() == ".png")
	{
		int Channels{};
		auto Raw = stbi_load(Name.c_str(), &W, &H, &Channels, 4);
		std::unique_ptr<unsigned char, decltype(&stbi_image_free)> Holder(Raw, stbi_image_free);
		if (!Raw)
		{
			throw std::runtime_error("PNG load failed: " + Name);
		}
		auto N = Pixels(static_cast<std::uint32_t>(W), static_cast<std::uint32_t>(H));
		Result.Rgba.resize(N);
		for (std::size_t I = 0; I < N; ++I)
		{
			Result.Rgba[I] = Raw[I] / 255.f;
		}
		Result.Encoding = EColorSpace::Srgb;
	}
	else
	{
		throw std::runtime_error("Unsupported image extension: " + Name);
	}
	Result.Width = static_cast<std::uint32_t>(W);
	Result.Height = static_cast<std::uint32_t>(H);
	return Result;
}

void SaveImage(const std::filesystem::path& InPath, const FImage& InImage)
{
	auto N = Pixels(InImage.Width, InImage.Height);
	if (InImage.Rgba.size() != N)
	{
		throw std::invalid_argument("RGBA image buffer size mismatch");
	}
	if (!InPath.parent_path().empty())
	{
		std::filesystem::create_directories(InPath.parent_path());
	}
	auto Name = InPath.string();
	auto W = static_cast<int>(InImage.Width);
	auto H = static_cast<int>(InImage.Height);
	if (InPath.extension() == ".exr")
	{
		if (InImage.Encoding != EColorSpace::Linear)
		{
			throw std::invalid_argument("EXR requires linear data");
		}
		const char* Error{};
		auto Status = SaveEXR(InImage.Rgba.data(), W, H, 4, 0, Name.c_str(), &Error);
		ExrError(Status, Error);
	}
	else if (InPath.extension() == ".png")
	{
		if (InImage.Encoding != EColorSpace::Srgb)
		{
			throw std::invalid_argument("PNG requires explicitly encoded sRGB data");
		}
		std::vector<unsigned char> Bytes(N);
		for (std::size_t I = 0; I < N; ++I)
		{
			if (!std::isfinite(InImage.Rgba[I]))
			{
				throw std::invalid_argument("Non-finite PNG channel");
			}
			Bytes[I] = static_cast<unsigned char>(std::lround(std::clamp(InImage.Rgba[I], 0.f, 1.f) * 255.f));
		}
		if (!stbi_write_png(Name.c_str(), W, H, 4, Bytes.data(), W * 4))
		{
			throw std::runtime_error("PNG write failed");
		}
	}
	else
	{
		throw std::runtime_error("Unsupported image extension");
	}
}
} // namespace Hyperion
