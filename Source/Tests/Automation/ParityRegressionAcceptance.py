"""Exercise the parity audit regressions through real attached MCP/CLI hosts."""
import pathlib
import shutil
import subprocess
import sys
import time

from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready


def screenshot(agent, path, window="main"):
    deadline = time.monotonic() + 15
    while True:
        result = agent.wait(agent.call("render.screenshot", path=str(path), window=window, overwrite=True))
        if result["status"] == "completed":
            artifact = completed(result)
            assert int(artifact["bytes"]) > 0 and path.is_file(), artifact
            return
        assert result["error"]["code"] == "busy" and time.monotonic() < deadline, result
        time.sleep(0.02)


def editor_regressions(cli, editor, assets, output):
    app = Application(editor, output, "editor-audit", "", assets, frames=15000)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), True)
        info = ready(agent)
        rejected = agent.call("scene.node.create", document=info["document"], revision=info["revision"],
                              components=["hyperion.staticmesh"])
        assert rejected["error"]["code"] == "invalid_arguments", rejected
        assert ready(agent)["revision"] == info["revision"]
        failed = agent.wait(agent.call("asset.open", path="/Game/Retry.hasset"))
        assert failed["error"]["code"] == "load_failed", failed
        entries = completed(agent.call("asset.documents.list"))["documents"]
        entry = next(item for item in entries if item["path"].endswith("Retry.hasset"))
        assert entry["state"] == "failed" and entry["error"] and entry["generation"] == "0", entry
        assert completed(agent.call("asset.info", document=entry["document"]))["state"] == "failed"
        completed(agent.call("asset.activate", document=entry["document"]))
        screenshot(agent, output / "FailedAsset.png", "assets")
        completed(agent.call("asset.close", document=entry["document"], generation="0"))
        shutil.copyfile(assets / "Texture.hasset", assets / "Retry.hasset")
        opened = completed(agent.wait(agent.call("asset.open", path="/Game/Retry.hasset")))
        assert opened["state"] == "ready" and opened["document"] != entry["document"]
        completed(agent.call("asset.close", document=opened["document"], generation=opened["generation"]))
        (assets / "Retry.hasset").unlink()
        info = ready(agent)
        failed = agent.wait(agent.call("scene.open", document=info["document"], revision=info["revision"], path="/Game/MissingScene.hasset"))
        assert failed["status"] == "failed", failed
        screenshot(agent, output / "FailedScene.png")
        info = completed(agent.call("scene.status"))["scene"]
        completed(agent.wait(agent.call("scene.open", document=info["document"], revision=info["revision"], path="")))
        info = ready(agent)
        completed(agent.call("scene.node.create", document=info["document"], revision=info["revision"], name="Close persisted node"))
        assert agent.call("application.close.request")["error"]["code"] == "dirty_document"
        assert agent.call("application.close.request", action=1)["error"]["code"] == "invalid_arguments"
        blocker = output / "NotDirectory"
        blocker.write_text("force asynchronous save failure", encoding="utf-8")
        saving = agent.call("application.close.request", action=1, scenePath=str(blocker / "Scene.hasset"))
        if saving["status"] == "completed":
            deadline = time.monotonic() + 20
            while True:
                state = completed(agent.call("application.close.status"))
                if state["state"] == "failed":
                    assert state["dirty"] and state["error"], state
                    break
                assert time.monotonic() < deadline, state
                time.sleep(0.02)
        assert app.process.poll() is None
        cancelled = completed(agent.call("application.close.request", action=3))
        assert cancelled["state"] == "idle" and cancelled["dirty"]
        destination = output / "ClosedScene.hasset"
        accepted = completed(agent.call("application.close.request", action=1, scenePath=str(destination)))
        assert accepted["state"] == "saving", accepted
        app.process.wait(timeout=30)
        assert app.process.returncode == 0 and destination.is_file(), app.path.read_text()
        assert "Final GPU validation errors: 0" in app.path.read_text() or "validation errors: 0" in app.path.read_text()
        print("Editor failed workspace/scene screenshots, retry, dirty close rejection and save-close passed", flush=True)
    finally:
        if agent:
            agent.close()
        app.close()


def model_regressions(cli, viewer, assets, output, failed=False):
    name = "model-error" if failed else "model-controls"
    path = "/Engine/Models/Primitives/Missing.hasset" if failed else "/Engine/Models/Primitives/Cube.hasset"
    app = Application(viewer, output, name, path, assets, frames=15000, model=True)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), True)
        deadline = time.monotonic() + 30
        while True:
            stats = completed(agent.call("render.statistics"))
            if (stats["sceneError"] if failed else stats["ready"]):
                break
            assert time.monotonic() < deadline, stats
            time.sleep(0.02)
        if not failed:
            view = completed(agent.call("view.get"))
            assert view["ready"] and not view["cameraAuthoring"], view
            camera = view["camera"]
            camera["world"]["values"][12] += 1.0
            changed = completed(agent.call("view.set", document="", revision="0", camera=camera))
            assert changed["camera"] == camera, changed
            completed(agent.call("view.frame_scene", document="", revision="0"))
            light = completed(agent.call("light.main.get"))
            light["light"]["intensity"] = 2.25
            light["direction"] = {"x": 1, "y": 2, "z": 3}
            updated = completed(agent.call("light.main.set", **light))
            assert updated["light"]["intensity"] == 2.25, updated
            assert agent.call("light.main.set", **light)["error"]["code"] == "stale_revision"
            updated["direction"] = {"x": 0, "y": 0, "z": 0}
            assert agent.call("light.main.set", **updated)["error"]["code"] == "invalid_arguments"
            assert completed(agent.call("light.main.get"))["revision"] == updated["revision"]
        screenshot(agent, output / (name + ".png"))
        accepted = completed(agent.call("application.close.request"))
        assert accepted["state"] == "closing", accepted
        app.process.wait(timeout=30)
        assert app.process.returncode == 0, app.path.read_text()
        print(name + " and normal close reply passed", flush=True)
    finally:
        if agent:
            agent.close()
        app.close()


def reopen_and_discard(cli, editor, assets, output):
    app = Application(editor, output, "reopen-close", output / "ClosedScene.hasset", assets, frames=15000)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), False)
        info = ready(agent)
        nodes = completed(agent.call("scene.nodes.list", document=info["document"], revision=info["revision"]))
        assert any(item["name"] == "Close persisted node" for item in nodes["nodes"]), nodes
        completed(agent.call("scene.node.create", document=info["document"], revision=info["revision"], name="Discarded node"))
        accepted = completed(agent.call("application.close.request", action=2))
        assert accepted["state"] == "closing"
        app.process.wait(timeout=30)
        assert app.process.returncode == 0, app.path.read_text()
        print("Saved scene reopened and explicit dirty discard-close passed", flush=True)
    finally:
        if agent:
            agent.close()
        app.close()


if __name__ == "__main__":
    cli, editor, viewer, fixture, output = [pathlib.Path(value).resolve() for value in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    # Each invocation gets independent assets; re-running must also exercise the failed-open path.
    import tempfile
    with tempfile.TemporaryDirectory(prefix="parity-", dir=output) as temporary:
        assets = pathlib.Path(temporary)
        subprocess.run([str(fixture), str(assets)], check=True)
        editor_regressions(cli, editor, assets / "Game", output)
        reopen_and_discard(cli, editor, assets / "Game", output)
        model_regressions(cli, viewer, assets / "Game", output)
        model_regressions(cli, viewer, assets / "Game", output, failed=True)
