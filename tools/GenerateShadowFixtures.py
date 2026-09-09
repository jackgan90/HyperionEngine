"""Generate original, self-contained glTF fixtures for CSM contact/alpha/bias inspection."""
import base64
import copy
import json
import math
import pathlib
import struct
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[1]


def alpha_image():
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
    rows = bytearray()
    for y in range(32):
        rows.append(0)
        for x in range(32):
            alpha = 255 if (x // 4 + y // 4) % 2 else 0
            rows.extend((110, 190, 105, alpha))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", 32, 32, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def card():
    binary = bytearray()
    doc = {"asset": {"version": "2.0", "generator": "Hyperion original shadow fixtures"},
           "bufferViews": [], "accessors": [], "scenes": [{"nodes": [0]}], "scene": 0,
           "nodes": [{"name": "UV1 alpha card", "mesh": 0}],
           "materials": [{"name": "Masked lattice", "doubleSided": True, "alphaMode": "MASK", "alphaCutoff": .5,
                          "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, .8], "metallicFactor": 0,
                                                  "roughnessFactor": .8, "baseColorTexture": {"index": 0, "texCoord": 1}}}],
           "images": [{"uri": "data:image/png;base64," + base64.b64encode(alpha_image()).decode()}],
           "textures": [{"source": 0, "sampler": 0}],
           "samplers": [{"wrapS": 33071, "wrapT": 33071, "minFilter": 9987, "magFilter": 9729}]}

    def accessor(values, components, indices=False):
        offset = len(binary)
        payload = struct.pack("<" + ("I" if indices else "f") * len(values), *values)
        binary.extend(payload)
        doc["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload)})
        result = {"bufferView": len(doc["bufferViews"]) - 1, "componentType": 5125 if indices else 5126,
                  "count": len(values) // components, "type": {1: "SCALAR", 2: "VEC2", 3: "VEC3", 4: "VEC4"}[components]}
        if components == 3:
            result["min"] = [min(values[index::3]) for index in range(3)]
            result["max"] = [max(values[index::3]) for index in range(3)]
        doc["accessors"].append(result)
        return len(doc["accessors"]) - 1

    attributes = {"POSITION": accessor([-1.2, 0, 0, 1.2, 0, 0, 1.2, 3, 0, -1.2, 3, 0], 3),
                  "NORMAL": accessor([0, 0, 1] * 4, 3), "COLOR_0": accessor([1, 1, 1, .75] * 4, 4),
                  "TEXCOORD_0": accessor([0, 0] * 4, 2), "TEXCOORD_1": accessor([0, 1, 1, 1, 1, 0, 0, 0], 2)}
    doc["meshes"] = [{"primitives": [{"attributes": attributes, "indices": accessor([0, 1, 2, 0, 2, 3], 1, True), "material": 0}]}]
    doc["buffers"] = [{"byteLength": len(binary), "uri": "data:application/octet-stream;base64," + base64.b64encode(binary).decode()}]
    return doc


def generate():
    models = ROOT / "assets/Models"
    mask = card()
    (models / "ShadowMask.gltf").write_text(json.dumps(mask, indent=2) + "\n", encoding="utf-8")
    thin = copy.deepcopy(mask)
    thin["materials"] = [{"name": "Thin double-sided sheet", "doubleSided": True,
                          "pbrMetallicRoughness": {"baseColorFactor": [.15, .42, .7, 1], "metallicFactor": 0, "roughnessFactor": .7}}]
    thin.pop("images")
    thin.pop("textures")
    thin.pop("samplers")
    (models / "ShadowThin.gltf").write_text(json.dumps(thin, indent=2) + "\n", encoding="utf-8")
    angle = math.radians(16) / 2
    instances = [
        {"id": "receiver-ground", "asset": "cube", "translation": [0, -.15, -3], "scale": [22, .3, 24]},
        {"id": "contact-column", "asset": "cube", "translation": [-5, 1.5, 0], "scale": [1.1, 3, 1.1]},
        {"id": "slender-contact", "asset": "cube", "translation": [-3, 2, 2], "scale": [.15, 4, .15]},
        {"id": "uv1-alpha-card", "asset": "mask", "translation": [0, 0, 0]},
        {"id": "mirrored-alpha-card", "asset": "mask", "translation": [4, 0, -2], "scale": [-1, 1, 1]},
        {"id": "thin-double-sided", "asset": "thin", "translation": [-5, 0, -5], "scale": [.15, 1.4, 1]},
        {"id": "sloped-receiver", "asset": "cube", "translation": [0, .9, -7], "scale": [5, .25, 5],
         "rotation": [0, 0, math.sin(angle), math.cos(angle)]},
        {"id": "sloped-occluder", "asset": "cube", "translation": [0, 2, -7], "scale": [.6, 2, .6]},
        {"id": "distant-caster", "asset": "cube", "translation": [0, 2, -35], "scale": [2, 4, 2]},
        {"id": "distant-receiver", "asset": "cube", "translation": [0, -.15, -32], "scale": [12, .3, 28]}]
    scene = {"type": "hyperion.scene", "schema_version": 1,
             "assets": [{"id": "cube", "path": "../Models/Ground.gltf"},
                        {"id": "mask", "path": "../Models/ShadowMask.gltf"},
                        {"id": "thin", "path": "../Models/ShadowThin.gltf"}],
             "instances": instances,
             "camera": {"eye": [9, 8, 14], "target": [0, 1, -4], "near": .05, "far": 250}}
    (ROOT / "assets/Scenes/Shadows.json").write_text(json.dumps(scene, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    generate()
