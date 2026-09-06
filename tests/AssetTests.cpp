#include "hyperion/Assets.h"
#include "hyperion/Core.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace Hyperion;

void Check(bool InValue, const char* InMessage)
{
	if (!InValue)
	{
		throw std::runtime_error(InMessage);
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		auto V = Transform(Multiply(Translation({1, 2, 3}), Scale({2, 3, 4})), {1, 1, 1, 1});
		Check(V.X == 3 && V.Y == 5 && V.Z == 7 && V.W == 1, "Column-vector matrix semantics");
		std::filesystem::create_directories("asset-test");
		const float Positions[] = {0, 1, 0, -1, -1, 0, 1, -1, 0};
		{
			std::ofstream F("asset-test/triangle.bin", std::ios::binary);
			F.write(reinterpret_cast<const char*>(Positions), sizeof(Positions));
		}
		{
			std::ofstream F("asset-test/triangle.gltf");
			F << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"triangle.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-1,-1,0],"max":[1,1,0]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}]})";
		}
		auto Mesh = LoadGltfPrimitive("asset-test/triangle.gltf");
		Check(Mesh.Vertices.size() == 3 && Mesh.Indices == std::vector<std::uint32_t>({0, 1, 2}) &&
		          Mesh.Vertices[0].Position.Y == 1,
		      "glTF triangle");
		Check(MemoryStats(EMemoryTag::Assets).LiveBytes == 0, "cgltf frees tracked allocations");
		bool Failed = false;
		try
		{
			LoadGltfPrimitive("asset-test/missing.gltf");
		}
		catch (...)
		{
			Failed = true;
		}
		Check(Failed, "Missing mesh error");
		FImage Source{2, 2, EColorSpace::Linear, {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 3, 1, .25f, .5f, .75f, 1}};
		SaveImage("asset-test/test.exr", Source);
		auto Exr = LoadImageFile("asset-test/test.exr");
		Check(Exr.Width == 2 && Exr.Height == 2 && Exr.Encoding == EColorSpace::Linear, "EXR dimensions");
		for (std::size_t I = 0; I < Source.Rgba.size(); ++I)
		{
			Check(std::abs(Exr.Rgba[I] - Source.Rgba[I]) < .00001f, "EXR float precision");
		}
		Source.Encoding = EColorSpace::Srgb;
		Source.Rgba[10] = 1;
		SaveImage("asset-test/test.png", Source);
		auto Png = LoadImageFile("asset-test/test.png");
		Check(Png.Width == 2 && Png.Height == 2 && Png.Encoding == EColorSpace::Srgb, "PNG dimensions");
		for (std::size_t I = 0; I < Source.Rgba.size(); ++I)
		{
			Check(std::abs(Png.Rgba[I] - Source.Rgba[I]) <= 1.f / 255.f, "PNG quantization");
		}
		FAssetReference Original{"mesh:triangle", "asset-test/triangle.gltf"};
		FAssetReference Restored;
		SaveReflected("asset-test/reference.json", AssetReferenceType(), &Original);
		LoadReflected("asset-test/reference.json", AssetReferenceType(), &Restored);
		Check(Original.Id == Restored.Id && Original.Source == Restored.Source, "Asset reflection");
		std::cout << "Asset/math checks passed\n";
		return 0;
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
