"""Verify authoring against the live Editor workspace through CLI and MCP."""
import pathlib
import base64
import json
import subprocess
import struct
import sys
import time
import zlib

from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready


def referenced_types(value):
    if isinstance(value, dict):
        if "x-hyperion-type" in value:
            yield value["x-hyperion-type"]
        for child in value.values():
            yield from referenced_types(child)
    elif isinstance(value, list):
        for child in value:
            yield from referenced_types(child)


def check_screenshot_rejections(sessions, output):
    destination = output / "RejectedWindow.png"
    assert not destination.exists(), destination
    for agent in sessions:
        schema = agent.request("api.describe", {"operation": "render.screenshot"})["inputSchema"]
        assert schema["properties"]["window"]["type"] == "string", schema
        assert schema["properties"]["window"]["default"] == "main", schema
        assert "enum" not in schema["properties"]["window"], schema
        before = ready(agent)
        for window in ("", "other", "Main", " assets", "main\0suffix"):
            for path in (str(destination), str(output / "RejectedWindow.txt")):
                result = agent.wait(agent.call("render.screenshot", window=window, path=path))
                assert result["error"] == {"code": "invalid_arguments", "message": "Window must be main or assets", "details": {}, "path": ""}, result
        result = agent.wait(agent.call("render.screenshot", window="assets", path=""))
        assert result["error"] == {"code": "busy", "message": "Open a drawable asset window", "details": {}, "path": ""}, result
        result = agent.wait(agent.call("render.screenshot", window="main", path=""))
        assert result["error"] == {"code": "invalid_arguments", "message": "Screenshot requires a target-local .png destination", "details": {}, "path": ""}, result
        for window in (None, 1, False):
            result = agent.wait(agent.call("render.screenshot", window=window, path=str(destination)))
            assert result["error"]["code"] == "invalid_arguments", result
        assert ready(agent) == before
        assert not destination.exists()
        assert not (output / "RejectedWindow.txt").exists()


def authoring(cli, editor, assets, output):
    app = Application(editor, output, "capabilities", "", assets, frames=0)
    sessions = []
    try:
        agent = AttachedSession(cli, app.target(), True)
        peer = AttachedSession(cli, app.instance, False)
        sessions += [agent, peer]
        info = ready(agent)
        schema = agent.request("api.describe", {"operation": "material.values.set"})
        assert not schema["unavailable"], schema
        offset = 0
        described_types = set()
        operations = set()
        while True:
            page = agent.request("api.search", {"offset": offset, "limit": 50})
            for item in page["items"]:
                operations.add(item["id"])
                definition = agent.request("api.describe", {"operation": item["id"]})
                assert "inputSchema" in definition and "outputSchema" in definition, definition
                for type_id in referenced_types(definition):
                    if type_id not in described_types:
                        described_types.add(type_id)
                        nested = agent.request("types.describe", {"type": type_id})
                        assert nested.get("x-hyperion-type") == type_id, nested
            if page["nextOffset"] is None:
                break
            offset = page["nextOffset"]
        assert "scene.lighting.get" in operations
        assert not {"light.main.get", "light.main.set"} & operations
        for light_type in ("hyperion.scenedirectionallight", "hyperion.sceneenvironmentlight"):
            light_schema = agent.request("types.describe", {"type": light_type})
            assert light_schema["properties"]["priority"]["type"] == "integer", light_schema
        group = completed(agent.call("scene.node.create", document=info["document"],
                                     revision=info["revision"], name="Agent group"))
        info = ready(peer)
        view = completed(agent.call("view.get"))
        assert view["cameraAuthoring"]
        updated = completed(agent.call("view.set", document=info["document"], revision=info["revision"], options={"exposure": 1.25}))
        assert updated["options"]["exposure"] == 1.25
        assert completed(peer.call("view.get"))["options"]["exposure"] == 1.25
        assert ready(peer)["revision"] == info["revision"]
        camera_type = "hyperion.scene.camera"
        types = completed(agent.call("scene.component_types.list"))["components"]
        camera_type = next(item["type"] for item in types if "camera" in item["type"])
        camera = completed(agent.call("scene.node.create", document=info["document"],
                                      revision=info["revision"], name="Agent camera",
                                      parent=group["handle"], components=[camera_type]))
        info = ready(peer)
        selection = completed(peer.call("scene.selection.get", document=info["document"],
                                        revision=info["revision"]))
        assert selection["primary"] == camera["handle"]
        component = completed(agent.call("scene.component." + camera_type + ".get",
                                        document=info["document"], revision=info["revision"],
                                        handle=camera["handle"], component=camera_type))
        component["verticalRadians"] = 0.9
        info = completed(agent.call("scene.component." + camera_type + ".set",
                                    document=info["document"], revision=info["revision"],
                                    handles=[camera["handle"]], component=camera_type, value=component))
        info = completed(agent.call("scene.nodes.set_metadata", document=info["document"],
                                    revision=info["revision"],
                                    edits=[{"handle": camera["handle"], "name": "Renamed camera"}]))
        preview = completed(agent.call("view.preview_camera", document=info["document"],
                                       revision=info["revision"], camera=camera["handle"]))
        assert preview["previewCamera"] == camera["handle"]
        completed(agent.call("view.preview_camera", document=info["document"],
                             revision=info["revision"], camera=None))
        completed(agent.call("view.apply_to_camera", document=info["document"],
                             revision=info["revision"], camera=camera["handle"]))
        info = ready(agent)
        completed(agent.call("view.save_initial", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        saved = completed(agent.wait(agent.call("scene.save", document=info["document"],
                                               revision=info["revision"],
                                               path=str(output / "Authoring.hasset"))))
        assert not saved["dirty"]
        info = ready(agent)
        placed = completed(agent.wait(agent.call("scene.placement.place", document=info["document"],
                                                 revision=info["revision"], object="Cube", position={"x": 2, "y": 0, "z": 0})))
        assert placed["kind"] == "Model", placed
        info = ready(agent)
        stats = completed(agent.call("render.statistics"))
        assert stats["ready"] and not stats["sceneError"], stats
        diagnostics = completed(agent.call("render.component_diagnostics", handle=placed["handle"], component="hyperion.staticmesh", limit=1))
        assert len(diagnostics["primitives"]) == 1, diagnostics
        completed(agent.wait(agent.call("scene.save", document=info["document"], revision=info["revision"], path=str(output / "AuthoringRender.hasset"))))
        info = ready(agent)
        completed(agent.call("scene.undo", document=info["document"], revision=info["revision"]))
        check_screenshot_rejections(sessions, output)
        artifact = completed(agent.wait(agent.call("render.screenshot", path=str(output / "Main.png"), overwrite=True)))
        assert artifact["width"] > 0 and int(artifact["bytes"]) > 0
        assert pathlib.Path(artifact["path"]).is_file()
        opened = completed(agent.wait(agent.call("asset.open", path="/Game/Texture.hasset")))
        duplicate = completed(peer.wait(peer.call("asset.open", path="/Game/Texture.hasset")))
        assert opened["document"] == duplicate["document"]
        sample = completed(agent.call("texture.sample", document=opened["document"], generation=opened["generation"], x=1))
        assert sample["width"] == 2 and sample["rgba"] == [1, 1, 1, 1], sample
        deadline = time.monotonic() + 20
        while True:
            preview = completed(agent.call("asset.preview.get", document=opened["document"]))
            if preview["ready"]:
                break
            assert time.monotonic() < deadline, preview
            time.sleep(0.02)
        preview = completed(agent.call("asset.preview.set", document=opened["document"],
                                       generation=opened["generation"], settings={"channel": 1, "fit": False, "zoom": 2.0}))
        assert preview["settings"]["channel"] == 1
        assert preview["generation"] == opened["generation"]
        artifact = completed(agent.wait(agent.call("render.screenshot", window="assets",
                                                  path=str(output / "AssetPreview.png"), overwrite=True)))
        assert pathlib.Path(artifact["path"]).is_file() and int(artifact["bytes"]) > 0
        changed = completed(agent.call("asset.rename", document=opened["document"],
                                       generation=opened["generation"], name="Shared workspace"))
        snapshot = completed(peer.call("asset.info", document=opened["document"]))
        assert changed["active"] and all(changed[key] == snapshot[key] for key in ("name", "active", "generation", "dirty")), (changed, snapshot)
        root = completed(agent.call("content.root.get"))
        assert peer.call("content.root.clear", generation=root["generation"])["error"]["code"] == "dirty_document"
        changed = completed(peer.call("asset.undo", document=changed["document"], generation=changed["generation"]))
        assert changed["active"] and not changed["dirty"]
        assert completed(agent.call("asset.workspace.policy"))["retainsFailed"]
        other = completed(agent.wait(agent.call("asset.open", path="/Game/Secondary.hasset")))
        inactive = completed(agent.call("asset.rename", document=changed["document"], generation=changed["generation"], name="Inactive edit"))
        assert not inactive["active"]
        assert inactive["active"] == completed(peer.call("asset.info", document=inactive["document"]))["active"]
        changed = completed(agent.call("asset.undo", document=inactive["document"], generation=inactive["generation"]))
        completed(agent.call("asset.close", document=other["document"], generation=other["generation"]))
        completed(agent.call("asset.close", document=changed["document"], generation=changed["generation"]))
        assert peer.call("asset.info", document=changed["document"])["error"]["code"] == "not_found"
        import_model(agent, output)
        info = ready(agent)
        reopened = completed(agent.wait(agent.call("scene.open", document=info["document"], revision=info["revision"], path=str(output / "Authoring.hasset"), discard=True)))
        assert reopened["scene"]["document"] != info["document"]
        assert reopened["scene"]["ready"]
        root = completed(agent.call("content.root.get"))
        cleared = completed(agent.call("content.root.clear", generation=root["generation"]))
        assert cleared["directory"] == ""
        info = ready(agent)
        assert not info["dirty"]
        completed(agent.call("content.root.set", generation=cleared["generation"], directory=str(assets)))
        info = ready(agent)
        failed = agent.wait(agent.call("scene.open", document=info["document"], revision=info["revision"], path="/Game/MissingScene.hasset"))
        assert failed["status"] == "failed", failed
        status = completed(agent.call("scene.status"))
        assert status["error"], status
        recovered = completed(agent.wait(agent.call("scene.open", document=status["scene"]["document"], revision=status["scene"]["revision"], path="")))
        assert recovered["scene"]["ready"]
        completed(agent.call("gui.scale.set", scale=1.25))
        time.sleep(0.1)
        assert completed(agent.call("gui.scale.get"))["scale"] == 1.25
        capture = completed(agent.call("renderdoc.status"))
        if not capture["available"]:
            assert agent.call("renderdoc.capture")["error"]["code"] == "unavailable"
        print("Live authoring and shared asset workspace passed")
    finally:
        for session in sessions:
            session.close()
        app.close()


def import_model(agent, output):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00\xff")) + chunk(b"IEND", b"")
    data = struct.pack("<15f3H", 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 2)
    source = output / "Import.gltf"
    source.write_text(json.dumps({"asset": {"version": "2.0"},
        "buffers": [{"byteLength": len(data), "uri": "data:application/octet-stream;base64," + base64.b64encode(data).decode()}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}, {"buffer": 0, "byteOffset": 60, "byteLength": 6}, {"buffer": 0, "byteOffset": 36, "byteLength": 24}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]},
                      {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}, {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"}],
        "images": [{"uri": "data:image/png;base64," + base64.b64encode(png).decode()}],
        "textures": [{"source": 0}], "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 2}, "indices": 1, "material": 0}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0}), encoding="utf-8")
    root = completed(agent.call("content.root.get"))
    canonical_id = "6a35c489e052453e86a052b44a0b6df4"
    schema = agent.request("api.describe", {"operation": "asset.import"})
    assert "rootId" in schema["inputSchema"]["properties"], schema
    request = dict(generation=root["generation"], source=str(source), output="/Game/CanonicalImported.hasset", force=True,
                   rootId=canonical_id)
    pending = agent.call("asset.import", **request)
    if pending["status"] == "running":
        assert not agent.request("jobs.get", {"job": pending["job"]})["cancellable"]
    result = completed(agent.wait(pending))
    assert int(result["writtenAssets"]) > 0, result
    assert result["asset"]["id"] == canonical_id, result
    rejected = agent.wait(agent.call("asset.import", **dict(request, rootId="invalid")))
    assert rejected["status"] == "failed", rejected
    rejected = agent.wait(agent.call("asset.import", **dict(request, rootId="11111111111111111111111111111111")))
    assert rejected["status"] == "failed", rejected
    page = completed(agent.call("content.assets.list", generation=root["generation"],
                                query="CanonicalImported.hasset", limit=1))
    assert int(page["total"]) >= 1 and len(page["assets"]) == 1, page
    assert agent.call("content.assets.list", generation="0")["error"]["code"] == "stale_revision"
    imported = completed(agent.wait(agent.call("asset.open", path="/Game/CanonicalImported.hasset")))
    query = dict(document=imported["document"], generation=imported["generation"])
    primitives = completed(agent.call("model.primitives.get", **query))["values"]
    primitives[0]["name"] = "Agent primitive"
    imported = completed(agent.call("model.primitives.set", **query, values=primitives))
    query["generation"] = imported["generation"]
    slots = completed(agent.call("model.material_slots.get", **query))["value"]
    bad_slots = [dict(slots[0], path="/Game/Missing.hasset", id="")]
    rejected = agent.wait(agent.call("model.material_slots.set", **query, value=bad_slots))
    assert rejected["status"] == "failed", rejected
    assert completed(agent.call("asset.info", document=imported["document"]))["generation"] == imported["generation"]
    imported = completed(agent.wait(agent.call("asset.save", **query)))
    completed(agent.call("asset.close", document=imported["document"], generation=imported["generation"]))
    rejected = agent.call("asset.import", **(request | {"generation": "0"}))
    assert rejected["error"]["code"] == "stale_revision", rejected
    material = completed(agent.wait(agent.call("asset.open", path=slots[0]["path"])))
    query = dict(document=material["document"], generation=material["generation"])
    parameters = completed(agent.call("material.parameters.get", **query))["value"]
    roughness = next(item["name"] for item in parameters if item["semantic"] == "Pbr.RoughnessFactor")
    numeric_before = completed(agent.call("material.numeric.get", **query, name=roughness))
    material = completed(agent.call("material.numeric.set", **query, edits=[{"name": roughness, "values": [0.375]}]))
    query["generation"] = material["generation"]
    assert completed(agent.call("material.numeric.get", **query, name=roughness))["values"] == [0.375]
    rejected = agent.call("material.numeric.set", **query, edits=[{"name": roughness, "values": [0.5]}, {"name": "Missing", "values": [1]}])
    assert rejected["status"] == "failed", rejected
    assert completed(agent.call("asset.info", document=material["document"]))["generation"] == material["generation"]
    material = completed(agent.call("asset.undo", **query))
    query["generation"] = material["generation"]
    assert completed(agent.call("material.numeric.get", **query, name=roughness))["values"] == numeric_before["values"]
    values = completed(agent.call("material.values.get", **query))["value"]
    next(item for item in values if item["name"] == roughness)["value"]["words"] = [struct.unpack("<I", struct.pack("<f", 2.0))[0]]
    material = completed(agent.wait(agent.call("material.values.set", **query, value=values)))
    query["generation"] = material["generation"]
    after = completed(agent.call("material.values.get", **query))["value"]
    assert next(item for item in after if item["name"] == roughness)["value"]["words"] == [1065353216]
    completed(agent.call("asset.close", **query, discard=True))


def render_controls(cli, editor, assets, output):
    app = Application(editor, output, "render-controls", output / "AuthoringRender.hasset", assets, frames=0)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), True)
        info = ready(agent)
        settings = completed(agent.call("render.settings.get"))
        values = dict(settings["values"])
        exposure_key = next(key for key in values if key.lower() == "exposure")
        values[exposure_key] = 1.3
        changed = completed(agent.call("render.settings.set", revision=settings["revision"], values=values))
        assert abs(changed["values"][exposure_key] - 1.3) < 1e-6
        assert agent.call("render.settings.set", revision=settings["revision"], values=values)["error"]["code"] == "stale_revision"
        path = output / "RenderSettings.json"
        completed(agent.wait(agent.call("render.settings.save", revision=changed["revision"], path=str(path))))
        assert path.is_file()
        shadows = completed(agent.call("render.shadows.get"))
        shadows["distance"] = 80
        assert completed(agent.call("render.shadows.set", **shadows))["distance"] == 80
        view = completed(agent.call("view.set", document=info["document"], revision=info["revision"], options={"culling": 1, "modelBounds": True}))
        assert view["options"]["culling"] == 1
        catalog = completed(agent.call("scene.placement.list"))
        available = next(item for item in catalog["items"] if not item["unavailable"])
        placed = completed(agent.wait(agent.call("scene.placement.place", document=info["document"], revision=info["revision"], object=available["id"], position={"x": 5, "y": 0, "z": 0})))
        info = ready(agent)
        selected = completed(agent.call("scene.selection.get", document=info["document"], revision=info["revision"]))
        assert selected["primary"] == placed["handle"]
        completed(agent.call("scene.selection.duplicate", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        completed(agent.call("scene.selection.remove_keep_children", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        assert info["canUndo"]
        completed(agent.call("scene.undo", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        selected = completed(agent.call("scene.selection.get", document=info["document"], revision=info["revision"]))
        assert selected["primary"] is not None
        light_type = "hyperion.scenedirectionallight"
        sun = completed(agent.call("scene.node.create", document=info["document"], revision=info["revision"], name="Sun", components=[light_type]))["handle"]
        info = ready(agent)
        original_light = completed(agent.call(f"scene.component.{light_type}.get", document=info["document"], revision=info["revision"], handle=sun, component=light_type))
        light = dict(original_light, intensity=2.25, priority=10)
        request = dict(document=info["document"], revision=info["revision"], handles=[sun], component=light_type, value=light)
        completed(agent.call(f"scene.component.{light_type}.set", **request))
        lighting_schema = agent.request("api.describe", {"operation": "scene.lighting.get"})["outputSchema"]
        light_schema = lighting_schema["properties"]["lights"]["items"]["properties"]
        assert light_schema["type"] == {"type": "string", "default": ""}, light_schema
        sky_schema = lighting_schema["properties"]["lights"]["items"]["properties"]["asset"]["properties"]
        assert sky_schema["state"] == {"type": "string", "default": ""}, sky_schema
        assert sky_schema["error"] == {"type": "string", "default": ""}, sky_schema
        lighting = completed(agent.call("scene.lighting.get"))
        assert lighting["shadowDirectionalLight"] == sun
        assert next(item for item in lighting["lights"] if item["handle"] == sun)["type"] == "directional", lighting
        assert all(item["type"] in ("directional", "sky") for item in lighting["lights"]), lighting
        assert all(isinstance(item["asset"]["state"], str) for item in lighting["lights"]), lighting
        assert agent.call(f"scene.component.{light_type}.set", **request)["error"]["code"] == "stale_revision"
        info = ready(agent)
        request.update(revision=info["revision"], value=dict(light, priority=2**31))
        assert agent.call(f"scene.component.{light_type}.set", **request)["error"]["code"] == "invalid_arguments"
        info = ready(agent)
        completed(agent.call("scene.undo", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        assert completed(agent.call(f"scene.component.{light_type}.get", document=info["document"], revision=info["revision"], handle=sun, component=light_type)) == original_light
        assert completed(agent.call("render.statistics"))["ready"]
        screenshot = completed(agent.wait(agent.call("render.screenshot", path=str(output / "Render.png"), overwrite=True)))
        assert pathlib.Path(screenshot["path"]).is_file()
        print("Editor render settings, model authoring and render output passed")
    finally:
        if agent:
            agent.close()
        app.close()


if __name__ == "__main__":
    cli, editor, fixture, output = [pathlib.Path(argument).resolve() for argument in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    assets = output / "Assets"
    subprocess.run([str(fixture), str(assets)], check=True)
    authoring(cli, editor, assets / "Game", output)
    render_controls(cli, editor, assets / "Game", output)
