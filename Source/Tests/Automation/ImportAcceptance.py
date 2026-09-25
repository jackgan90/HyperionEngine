"""Exercise shared external import through shipped JSONL/MCP and real Editor widgets."""
import base64
import json
import pathlib
import subprocess
import sys
import tempfile
import time

from AutomationAcceptance import Session, completed
from AttachmentAcceptance import Application, AttachedSession

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from GenerateModelFixtures import JPEG, generate, png


def prepare(directory):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "Game").mkdir()
    (directory / "Color.png").write_bytes(png(8, 8))
    (directory / "Color.jpeg").write_bytes(base64.b64decode(JPEG))
    (directory / "Sky.hdr").write_bytes(
        b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n" + bytes([128, 128, 128, 129]) * 2)
    (directory / "Sky.json").write_text(json.dumps({
        "type": "hyperion.skyasset", "schema_version": 1, "name": "Recipe sky",
        "source": "Sky.hdr", "radiance_size": 8, "specular_size": 4, "samples": 8}), encoding="utf-8")


def workflow(cli, tool, directory, mcp):
    prepare(directory)
    generate(directory / "Models")
    session = Session(cli, directory / "Game", mcp=mcp)
    try:
        found = session.request("api.search", {"query": "import", "limit": 50})
        assert {"asset.import", "asset.import.validate", "asset.import.tasks", "asset.import.task",
                "asset.import.capabilities"} <= {item["id"] for item in found["items"]}
        schema = session.request("api.describe", {"operation": "asset.import"})["inputSchema"]
        assert {"rootId", "textureEncoding", "sky"} <= schema["properties"].keys()
        formats = completed(session.call("asset.import.capabilities"))["formats"]
        assert len(formats) == 5
        generation = completed(session.call("content.root.get"))["generation"]
        request = dict(generation=generation, source=str(directory / "Color.png"),
                       output="/Game/Color.hasset", library="/Game")
        completed(session.call("asset.import.validate", **request))
        assert not list((directory / "Game").rglob("*.hasset"))
        first = completed(session.wait(session.call("asset.import", **request)))
        assert first["writtenAssets"] == "1" and not first["warning"]
        repeat = completed(session.wait(session.call("asset.import", **request)))
        assert repeat["upToDate"] and repeat["writtenAssets"] == "0"
        changed = completed(session.wait(session.call("asset.import", **request, textureEncoding=0)))
        assert changed["asset"]["id"] == first["asset"]["id"] and changed["writtenAssets"] == "1"
        task = completed(session.call("asset.import.task", task=changed["task"]))
        assert task["result"] == changed and task["status"] == "completed"
        for source in ("Color.jpeg", "Models/Showcase.gltf", "Models/Showcase.glb", "Sky.json", "Sky.hdr"):
            options = dict(request, source=str(directory / source), output="/Game/" + source.replace("/", "-") + ".hasset")
            if source.endswith(".hdr"):
                options["sky"] = {"radianceSize": 8, "specularSize": 4, "samples": 8}
            result = completed(session.wait(session.call("asset.import", **options)))
            assert int(result["writtenAssets"]) > 0 and not result["warning"], result
            assert completed(session.wait(session.call("asset.import", **options)))["upToDate"]
        recipe = dict(request, source=str(directory / "Sky.json"), output="/Game/Sky.json.hasset",
                      sky={"radianceSize": 16, "specularSize": 8, "samples": 8})
        assert not completed(session.wait(session.call("asset.import", **recipe)))["upToDate"]
        assert completed(session.wait(session.call("asset.import", **recipe)))["upToDate"]
        wrapped = dict(request, source=str(directory / "Models/Showcase.gltf"), output="/Game/Scene.hasset", scene=True)
        assert completed(session.wait(session.call("asset.import", **wrapped)))["asset"]["type"] == "hyperion.scene"
        # Existing JSON and native paths for every registered authoring type.
        for filename in ("Color.hasset", "Scene.hasset", "Models-Showcase.gltf.hasset", "Sky.json.hasset"):
            source = directory / (filename + ".json")
            subprocess.run([str(tool), "--asset-root", str(directory / "Game"), "export-json",
                            "/Game/" + filename, str(source)], check=True, capture_output=True)
            options = dict(request, source=str(source), output="/Game/Copy-" + filename)
            completed(session.wait(session.call("asset.import", **options)))
        material = directory / "Material.json"
        subprocess.run([str(tool), "export-json", "/Engine/Materials/DefaultPrimitive.hasset", str(material)],
                       check=True, capture_output=True)
        completed(session.wait(session.call("asset.import", **dict(request, source=str(material), output="/Game/Material.hasset"))))
        native = dict(request, source="/Game/Color.hasset", output="/Game/NativeCopy.hasset")
        completed(session.wait(session.call("asset.import", **native)))
        broken = directory / "Broken.png"
        broken.write_bytes(b"invalid image")
        failure = session.wait(session.call("asset.import", **dict(request, source=str(broken), output="/Game/Broken.hasset")))
        assert failure["status"] == "failed" and failure["error"]["code"] == "operation_failed", failure
        failed_task = completed(session.call("asset.import.tasks"))["tasks"][0]
        assert failed_task["status"] == "failed" and failed_task["error"]
        assert not (directory / "Game/Broken.hasset").exists()
        before = {path: path.read_bytes() for path in (directory / "Game").rglob("*.hasset")}
        for changes in ({"generation": "0"}, {"rootId": "invalid"}, {"sourceId": "unpaired"},
                        {"sky": {"radianceSize": 3}}, {"textureEncoding": 7}, {"type": "hyperion.skyasset"}):
            rejected = session.wait(session.call("asset.import", **(request | changes)))
            assert rejected["status"] == "failed", rejected
        assert before == {path: path.read_bytes() for path in (directory / "Game").rglob("*.hasset")}
        root = completed(session.call("content.root.set", directory=str(directory / "Game"), generation=generation, readOnly=True))
        assert completed(session.call("asset.import.tasks"))["total"] == 0
        assert session.call("asset.import", **(request | {"generation": root["generation"]}))["error"]["code"] == "read_only"
        assert session.call("asset.import.task", task=changed["task"])["error"]["code"] == "not_found"
    finally:
        session.close()


def gui(cli, editor, directory, disabled):
    prepare(directory)
    extra = ["--exercise-import", str(directory / "Color.png"), "--ui-scale", "1"]
    if disabled:
        extra += ["--disable-plugin", "automation-assets", "--disable-plugin", "automation-local"]
    app = Application(editor, directory, "Editor", "", directory / "Game", frames=400 if disabled else 10000, extra=extra)
    agent = None
    try:
        if not disabled:
            agent = AttachedSession(cli, app.target(), True)
        deadline = time.monotonic() + 80
        while "Import GUI acceptance passed" not in app.path.read_text(encoding="utf-8", errors="replace"):
            assert app.process.poll() is None, app.path.read_text(encoding="utf-8", errors="replace")
            assert time.monotonic() < deadline, app.path.read_text(encoding="utf-8", errors="replace")
            time.sleep(0.05)
        if agent:
            drafts = completed(agent.call("asset.import.drafts"))["drafts"]
            assert len(drafts) == 1
            draft = completed(agent.call("asset.import.draft.get", draft=drafts[0]))
            assert draft["name"] == "GUI draft texture" and not draft["dirty"], draft
            tasks = completed(agent.call("asset.import.tasks"))["tasks"]
            assert len(tasks) == 2 and tasks[0]["result"]["upToDate"]
            assert completed(agent.call("asset.import.task", task=tasks[0]["task"])) == tasks[0]
            opened = completed(agent.wait(agent.call("asset.open", path="/Game/Color.hasset")))
            assert opened["document"]
            draft = completed(agent.call("asset.import.draft.edit", draft=draft["draft"],
                                         generation=draft["generation"], properties={"name": "Unpublished change"}))
            assert completed(agent.call("application.close.status"))["dirty"]
            assert agent.call("application.close.request")["error"]["code"] == "dirty_document"
            assert agent.call("application.close.request", action=1)["error"]["code"] == "dirty_document"
            # A confirmed root switch must invalidate the GUI cache as well as the shared draft.
            # Give the GUI a frame to observe the dirty state before switching roots.
            time.sleep(0.1)
            other = directory / "Other"
            other.mkdir()
            current_root = completed(agent.call("content.root.get"))
            completed(agent.call("content.root.set", directory=str(other),
                                 generation=current_root["generation"], discard=True))
            deadline = time.monotonic() + 10
            while True:
                draft_ids = completed(agent.call("asset.import.drafts"))["drafts"]
                if draft_ids:
                    refreshed = completed(agent.call("asset.import.draft.get", draft=draft_ids[0]))
                    if refreshed["status"] != "preparing":
                        break
                assert time.monotonic() < deadline, "GUI did not prepare after discarding dirty draft on root switch"
                time.sleep(0.02)
            assert refreshed["status"] == "ready" and refreshed["name"] == "Color" and not refreshed["dirty"], refreshed
            assert agent.call("asset.import.draft.get", draft=draft["draft"])["error"]["code"] == "not_found"
            assert not list(other.rglob("*.hasset"))
            completed(agent.call("application.close.request", action=2))
        app.finish()
        assert (directory / "ImportPanel.png").is_file()
    finally:
        if agent:
            agent.close()
        app.close()


if __name__ == "__main__":
    cli, tool, editor, output = map(lambda value: pathlib.Path(value).resolve(), sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    run = pathlib.Path(tempfile.mkdtemp(prefix="Import-", dir=output))
    for mode in (False, True):
        workflow(cli, tool, run / ("Mcp" if mode else "Jsonl"), mode)
    for disabled in (False, True):
        gui(cli, editor, run / ("GuiDisabled" if disabled else "Gui"), disabled)
    print(f"Import formats, automation parity and GUI acceptance passed: {run}")
