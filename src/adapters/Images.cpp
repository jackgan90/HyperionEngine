#include "hyperion/Assets.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
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
