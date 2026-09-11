#include "GltfImportInternal.h"

namespace Hyperion
{
FMesh LoadGltfPrimitive(const std::filesystem::path& InPath, std::size_t InMesh, std::size_t InPrimitive)
{
	FTaskSystem Tasks(1, 1);
	FIOService IO(Tasks);
	auto Request = DispatchAsync<FMesh>(
	    Tasks, {EDomain::Worker},
	    [&]
	    {
		    FAssetImportContext Context{IO, std::filesystem::absolute(InPath).lexically_normal()};
		    Context.Bytes = Context.Read(Context.Path);
		    const auto Primitive = Private::ConvertGltfPrimitive(Context, InMesh, InPrimitive);
		    FMesh Mesh;
		    Mesh.Indices = Primitive.Indices;
		    for (std::size_t Index = 0; Index < Primitive.Positions.size() / 3; ++Index)
		    {
			    FVertex Vertex;
			    Vertex.Position = {Primitive.Positions[Index * 3], Primitive.Positions[Index * 3 + 1],
			                       Primitive.Positions[Index * 3 + 2]};
			    if (!Primitive.Colors.empty())
			    {
				    Vertex.Color = {Primitive.Colors[Index * 4], Primitive.Colors[Index * 4 + 1],
				                    Primitive.Colors[Index * 4 + 2], Primitive.Colors[Index * 4 + 3]};
			    }
			    if (!Primitive.TexCoords0.empty())
			    {
				    Vertex.Uv = {Primitive.TexCoords0[Index * 2], Primitive.TexCoords0[Index * 2 + 1]};
			    }
			    Mesh.Vertices.push_back(Vertex);
		    }
		    return Mesh;
	    });
	return *Request.Get(Tasks);
}
} // namespace Hyperion
