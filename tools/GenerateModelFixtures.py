"""Generate deterministic, original glTF fixtures using only the Python standard library."""
import argparse
import base64
import copy
import json
import math
import pathlib
import struct
import zlib

JPEG = "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAACAAIDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDTooor4o+8P//Z"

def png(width, height):
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            color = (235, 178, 70, 255) if ((x // 8) + (y // 8)) % 2 else (30, 105, 135, 255)
            rows.extend(color)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")

def generate(output):
    output.mkdir(parents=True, exist_ok=True)
    binary = bytearray()
    doc = {"asset": {"version": "2.0", "generator": "Hyperion deterministic fixtures"},
           "bufferViews": [], "accessors": [], "meshes": [], "nodes": [],
           "materials": [
               {"name": "Checker ceramic", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0, "roughnessFactor": 0.5}},
               {"name": "Copper", "pbrMetallicRoughness": {"baseColorFactor": [0.95, 0.48, 0.2, 1], "metallicFactor": 0.9, "roughnessFactor": 0.18}},
               {"name": "Matte teal", "pbrMetallicRoughness": {"baseColorFactor": [0.06, 0.52, 0.45, 1], "metallicFactor": 0.1, "roughnessFactor": 0.8}},
               {"name": "Platform", "pbrMetallicRoughness": {"baseColorFactor": [0.18, 0.22, 0.29, 1], "metallicFactor": 0.15, "roughnessFactor": 0.7}}],
           "images": [{"uri": "Checker.png"}], "textures": [{"source": 0, "sampler": 0}],
           "samplers": [{"wrapS": 10497, "wrapT": 10497, "minFilter": 9987, "magFilter": 9729}],
           "scenes": [{"nodes": [0]}], "scene": 0}
    def view(data):
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        binary.extend(data)
        doc["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(data)})
        return len(doc["bufferViews"]) - 1
    def accessor(values, components, index=False):
        flat = [item for vertex in values for item in vertex] if components > 1 else values
        data = struct.pack("<" + ("I" if index else "f") * len(flat), *flat)
        acc = {"bufferView": view(data), "componentType": 5125 if index else 5126,
               "count": len(values), "type": {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4"}[components]}
        if components == 3:
            acc["min"] = [min(v[i] for v in values) for i in range(3)]
            acc["max"] = [max(v[i] for v in values) for i in range(3)]
        doc["accessors"].append(acc)
        return len(doc["accessors"]) - 1
    def mesh(name, positions, normals, uvs, indices, material):
        attrs = {"POSITION": accessor(positions, 3), "NORMAL": accessor(normals, 3), "TEXCOORD_0": accessor(uvs, 2)}
        doc["meshes"].append({"name": name, "primitives": [{"attributes": attrs, "indices": accessor(indices, 1, True), "material": material}]})
        return len(doc["meshes"]) - 1
    cube_p, cube_n, cube_uv, cube_i = [], [], [], []
    for normal, axis_u, axis_v in [((0,0,1),(1,0,0),(0,1,0)), ((0,0,-1),(-1,0,0),(0,1,0)),
                                   ((1,0,0),(0,0,-1),(0,1,0)), ((-1,0,0),(0,0,1),(0,1,0)),
                                   ((0,1,0),(1,0,0),(0,0,-1)), ((0,-1,0),(1,0,0),(0,0,1))]:
        start = len(cube_p)
        for u, v in [(-1,-1),(1,-1),(1,1),(-1,1)]:
            cube_p.append(tuple((normal[k] + axis_u[k] * u + axis_v[k] * v) * .5 for k in range(3)))
            cube_n.append(normal)
            cube_uv.append(((u+1)/2, (1-v)/2))
        cube_i.extend(start + i for i in [0,1,2,0,2,3])
    cube = mesh("Checker cube", cube_p, cube_n, cube_uv, cube_i, 0)
    sphere_p, sphere_n, sphere_uv, sphere_i = [], [], [], []
    for y in range(25):
        theta = y / 24 * math.pi
        for x in range(49):
            phi = x / 48 * math.tau
            point = (math.sin(theta)*math.cos(phi), math.cos(theta), math.sin(theta)*math.sin(phi))
            sphere_p.append(point)
            sphere_n.append(point)
            sphere_uv.append((x/48,y/24))
    for y in range(24):
        for x in range(48):
            a = y*49+x
            sphere_i.extend([a,a+1,a+49,a+1,a+50,a+49])
    copper = mesh("Copper sphere", sphere_p, sphere_n, sphere_uv, sphere_i, 1)
    teal = copy.deepcopy(doc["meshes"][copper])
    teal["name"] = "Teal sphere"
    teal["primitives"][0]["material"] = 2
    doc["meshes"].append(teal)
    platform = copy.deepcopy(doc["meshes"][cube])
    platform["name"] = "Platform"
    platform["primitives"][0]["material"] = 3
    doc["meshes"].append(platform)
    doc["nodes"] = [
        {"name":"Material study", "children":[1,2,3,4]},
        {"name":"Checker", "mesh":cube, "translation":[-1.35,0.9,0], "scale":[1.35,1.8,1.35], "rotation":[0,math.sin(.22),0,math.cos(.22)]},
        {"name":"Copper", "mesh":copper, "translation":[.8,1.05,0], "scale":[.9,.9,.9]},
        {"name":"Teal", "mesh":2, "translation":[2.3,.6,.5], "scale":[.5,.5,.5]},
        {"name":"Pedestal", "mesh":3, "translation":[.2,-.12,0], "scale":[5.8,.24,3.2]}]
    doc["buffers"] = [{"uri":"Showcase.bin", "byteLength":len(binary)}]
    (output / "Checker.png").write_bytes(png(64,64))
    (output / "Tiny.jpg").write_bytes(base64.b64decode(JPEG))
    (output / "Showcase.bin").write_bytes(binary)
    def save(name, value):
        (output / name).write_text(json.dumps(value, indent=2)+"\n", encoding="utf-8")
    save("Showcase.gltf", doc)
    data_uri = copy.deepcopy(doc)
    data_uri["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(binary).decode()
    data_uri["images"][0]["uri"] = "data:image/png;base64," + base64.b64encode(png(64,64)).decode()
    save("DataUri.gltf", data_uri)
    jpeg = copy.deepcopy(doc)
    jpeg["images"][0]["uri"] = "Tiny.jpg"
    save("Jpeg.gltf", jpeg)
    glb = copy.deepcopy(doc)
    glb["buffers"][0].pop("uri")
    glb_binary = bytearray(binary)
    while len(glb_binary) % 4:
        glb_binary.append(0)
    glb["bufferViews"].append({"buffer":0, "byteOffset":len(glb_binary), "byteLength":len(png(64,64))})
    glb["images"] = [{"bufferView":len(glb["bufferViews"])-1, "mimeType":"image/png"}]
    glb_binary.extend(png(64,64))
    glb["buffers"][0]["byteLength"] = len(glb_binary)
    while len(glb_binary) % 4:
        glb_binary.append(0)
    json_chunk = json.dumps(glb).encode()
    json_chunk += b" " * (-len(json_chunk) % 4)
    chunks = struct.pack("<II", len(json_chunk), 0x4e4f534a) + json_chunk + struct.pack("<II", len(glb_binary), 0x004e4942) + glb_binary
    (output / "Showcase.glb").write_bytes(struct.pack("<III",0x46546c67,2,12+len(chunks))+chunks)
    bad = copy.deepcopy(doc)
    bad["extensionsRequired"] = ["KHR_draco_mesh_compression"]
    bad["extensionsUsed"] = ["KHR_draco_mesh_compression"]
    save("Unsupported.gltf", bad)
    bad = copy.deepcopy(doc)
    bad["buffers"][0]["uri"] = "Missing.bin"
    save("Missing.gltf", bad)
    sparse_bytes = bytes([0,1,2,0]) + struct.pack("<9f",0,0,0,2,0,0,0,3,0)
    sparse = {"asset":{"version":"2.0"}, "buffers":[{"uri":"data:application/octet-stream;base64,"+base64.b64encode(sparse_bytes).decode(),"byteLength":len(sparse_bytes)}],
              "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":3},{"buffer":0,"byteOffset":4,"byteLength":36}],
              "accessors":[{"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[2,3,0],
                  "sparse":{"count":3,"indices":{"bufferView":0,"componentType":5121},"values":{"bufferView":1}}}],
              "meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],
              "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0}
    save("Sparse.gltf", sparse)
    interleaved = copy.deepcopy(sparse)
    packed = struct.pack("<12f",0,0,0,99,2,0,0,99,0,3,0,99)
    interleaved["buffers"][0] = {"uri":"data:application/octet-stream;base64,"+base64.b64encode(packed).decode(),"byteLength":len(packed)}
    interleaved["bufferViews"] = [{"buffer":0,"byteLength":48,"byteStride":16}]
    interleaved["accessors"][0].pop("sparse")
    interleaved["accessors"][0]["bufferView"] = 0
    save("Interleaved.gltf", interleaved)
    sparse_interleaved = copy.deepcopy(interleaved)
    combined = packed + bytes([0, 1, 2, 0]) + struct.pack("<9f", 0, 0, 0, 2, 0, 0, 0, 3, 0)
    # Sentinel bytes make a wrong 16-byte sparse stride fail deterministically without an accidental overread.
    combined += struct.pack("<3f", 91, 92, 93)
    sparse_interleaved["buffers"][0] = {"uri": "data:application/octet-stream;base64," + base64.b64encode(combined).decode(), "byteLength": len(combined)}
    sparse_interleaved["bufferViews"].extend([
        {"buffer": 0, "byteOffset": 48, "byteLength": 3},
        {"buffer": 0, "byteOffset": 52, "byteLength": 36}])
    sparse_interleaved["accessors"][0]["sparse"] = {
        "count": 3, "indices": {"bufferView": 1, "componentType": 5121}, "values": {"bufferView": 2}}
    save("SparseInterleaved.gltf", sparse_interleaved)
    invalid_view = copy.deepcopy(sparse_interleaved)
    invalid_view["bufferViews"][1]["byteOffset"] = 1000000000
    save("InvalidSparseView.gltf", invalid_view)
    invalid_view = copy.deepcopy(sparse_interleaved)
    invalid_view["bufferViews"][0]["byteOffset"] = 9223372036854775807
    invalid_view["bufferViews"][0]["byteLength"] = 9223372036854775807
    save("OverflowView.gltf", invalid_view)
    invalid_sparse = copy.deepcopy(sparse_interleaved)
    invalid_sparse["accessors"][0]["sparse"]["count"] = 4
    save("InvalidSparseCount.gltf", invalid_sparse)
    deep = copy.deepcopy(interleaved)
    deep["nodes"] = [{"mesh": 0}] + [{"children": [index - 1]} for index in range(1, 300)]
    deep["scenes"][0]["nodes"] = [299]
    save("DeepNodes.gltf", deep)
    bad = copy.deepcopy(doc)
    bad["accessors"][0]["count"] = 10000000
    save("BadAccessor.gltf", bad)
    bad = copy.deepcopy(doc)
    bad["nodes"][1]["children"] = [0]
    save("Cycle.gltf", bad)
    bad = copy.deepcopy(doc)
    bad["meshes"][0]["primitives"][0]["mode"] = 1
    save("Lines.gltf", bad)
    (output / "Truncated.glb").write_bytes((output / "Showcase.glb").read_bytes()[:25])
    normalized = copy.deepcopy(interleaved)
    packed += bytes([255, 0, 0, 255, 0, 128, 0, 255, 0, 0, 255, 255])
    normalized["buffers"][0] = {"uri": "data:application/octet-stream;base64," + base64.b64encode(packed).decode(), "byteLength": len(packed)}
    normalized["bufferViews"].append({"buffer": 0, "byteOffset": 48, "byteLength": 12})
    normalized["accessors"].append({"bufferView": 1, "componentType": 5121, "normalized": True, "count": 3, "type": "VEC4"})
    normalized["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"] = 1
    save("Normalized.gltf", normalized)
    for name, mode, points in [
        ("Strip.gltf", 5, [(-1,-1,0), (1,-1,0), (-1,1,0), (1,1,0)]),
        ("Fan.gltf", 6, [(-1,-1,0), (1,-1,0), (1,1,0), (-1,1,0)])
    ]:
        topology = copy.deepcopy(interleaved)
        packed_quad = b"".join(struct.pack("<4f", *point, 0) for point in points)
        topology["buffers"][0] = {"uri": "data:application/octet-stream;base64," + base64.b64encode(packed_quad).decode(), "byteLength": 64}
        topology["bufferViews"][0]["byteLength"] = 64
        topology["accessors"][0].update(count=4, min=[-1,-1,0], max=[1,1,0])
        topology["meshes"][0]["primitives"][0]["mode"] = mode
        save(name, topology)
    print("Generated glTF fixtures in", output)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("out/fixtures"))
    generate(parser.parse_args().output)
