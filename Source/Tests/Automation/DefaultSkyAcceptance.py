"""Verify built-in sky authoring and previews with no Game root."""
import pathlib
import shutil
import sys
import time

from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready
from ParityRegressionAcceptance import screenshot


def current(agent):
    info = ready(agent)
    return {"document": info["document"], "revision": info["revision"]}


def environment(agent):
    query = current(agent)
    handle = completed(agent.call("scene.settings.get", **query))["environmentLight"]
    components = completed(agent.call("scene.components.list", document=query["document"], handle=handle))["components"]
    component = next(item for item in components if item["type"] == "hyperion.sceneenvironmentlight")
    return completed(agent.call("scene.component.hyperion.sceneenvironmentlight.get", **query,
                                handle=handle, component=component["component"]))


def workflow(cli, editor, output):
    assets = output / "EmptyGame"
    assets.mkdir(exist_ok=True)
    app = Application(editor, output, "default-sky", "", assets, frames=0)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), True)
        ready(agent)
        root = completed(agent.call("content.root.get"))
        completed(agent.call("content.root.clear", generation=root["generation"]))
        search = agent.request("api.search", {"query": "scene.sky.use_default"})
        assert any(item["id"] == "scene.sky.use_default" for item in search["items"]), search
        schema = agent.request("api.describe", {"operation": "scene.sky.use_default"})
        assert not schema["unavailable"] and "document" in schema["inputSchema"]["properties"], schema
        query = current(agent)
        completed(agent.call("scene.sky.use_default", **query))
        ready(agent)
        failed = agent.call("scene.sky.use_default", **query)
        assert failed["error"]["code"] == "stale_revision", failed
        sky = environment(agent)
        assert sky["sky"]["path"] == "/Engine/Skies/Cloudy.hasset" and sky["visible"], sky
        completed(agent.call("scene.undo", **current(agent)))
        assert completed(agent.call("scene.settings.get", **current(agent)))["environmentLight"] is None
        completed(agent.call("scene.redo", **current(agent)))
        assert environment(agent) == sky
        saved_path = output / "DefaultSky.hasset"
        completed(agent.wait(agent.call("scene.save", **current(agent), path=str(saved_path))))
        completed(agent.wait(agent.call("scene.open", **current(agent), path=str(saved_path))))
        assert environment(agent) == sky
        assert not ready(agent)["dirty"]
        screenshot(agent, output / "SceneSky.png")
        scene_before = ready(agent)
        for name, path in (("Model", "/Engine/Models/Primitives/Cube.hasset"),
                           ("Material", "/Engine/Materials/DefaultPrimitive.hasset")):
            document = completed(agent.wait(agent.call("asset.open", path=path)))
            deadline = time.monotonic() + 30
            while True:
                preview = completed(agent.call("asset.preview.get", document=document["document"]))
                assert not preview["error"], preview
                if preview["ready"]:
                    break
                assert time.monotonic() < deadline, preview
                time.sleep(.02)
            screenshot(agent, output / (name + "Sky.png"), "assets")
            assert not completed(agent.call("asset.info", document=document["document"]))["dirty"]
            assert ready(agent)["revision"] == scene_before["revision"]
            completed(agent.call("asset.close", document=document["document"], generation=document["generation"]))
        completed(agent.call("application.close.request"))
        app.process.wait(timeout=30)
        assert app.process.returncode == 0, app.path.read_text()
        assert "validation errors: 0" in app.path.read_text(), app.path.read_text()
        print("Default sky discovery, history, save/reload and Engine-only previews passed", flush=True)
    finally:
        if agent:
            agent.close()
        app.close()


def missing_sky(cli, editor, output):
    engine = output / "MissingSkyEngine"
    source = pathlib.Path(__file__).resolve().parents[3] / "Content"
    shutil.copytree(source, engine, dirs_exist_ok=True, ignore=shutil.ignore_patterns("Skies"))
    app = Application(editor, output, "missing-default-sky", "", output / "EmptyGame", frames=0,
                      extra=("--engine-content", engine))
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), False)
        before = ready(agent)
        opened = agent.wait(agent.call("asset.open", path="/Engine/Models/Primitives/Cube.hasset"))
        if opened["status"] == "failed":
            assert "Cloudy" in opened["error"]["message"], opened
        else:
            document = completed(opened)
            deadline = time.monotonic() + 15
            while True:
                preview = completed(agent.call("asset.preview.get", document=document["document"]))
                assert not preview["ready"], preview
                if preview["error"]:
                    assert "Cloudy" in preview["error"], preview
                    break
                assert time.monotonic() < deadline, preview
                time.sleep(.02)
        assert ready(agent)["revision"] == before["revision"]
        completed(agent.call("application.close.request"))
        app.process.wait(timeout=30)
        assert app.process.returncode == 0, app.path.read_text()
        print("Missing built-in sky reports a preview failure and closes cleanly", flush=True)
    finally:
        if agent:
            agent.close()
        app.close()


if __name__ == "__main__":
    cli_path, editor_path, output_path = [pathlib.Path(value).resolve() for value in sys.argv[1:]]
    output_path.mkdir(parents=True, exist_ok=True)
    workflow(cli_path, editor_path, output_path)
    missing_sky(cli_path, editor_path, output_path)
