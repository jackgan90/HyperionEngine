"""Verify scene lifecycle through real attached MCP/JSONL and retained asset documents."""
import pathlib
import subprocess
import sys

from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready


def request(info, **changes):
    return dict(document=info["document"], revision=info["revision"], **changes)


def lifecycle(agent, operation, info, **changes):
    return completed(agent.wait(agent.call(operation, **request(info, **changes))))["scene"]


def rejection(agent, operation, info, code, **changes):
    result = agent.wait(agent.call(operation, **request(info, **changes)))
    assert result["status"] == "failed" and result["error"]["code"] == code, result
    current = completed(agent.call("scene.info"))
    for field in ("document", "revision", "nodes", "dirty", "path", "canUndo", "canRedo"):
        assert current[field] == info[field], (field, info, current)


def workflow(cli, editor, assets, output, mcp):
    app = Application(editor, output, f"lifecycle-{mcp}", "", assets, frames=0)
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), mcp)
        info = ready(agent)
        found = agent.request("api.search", {"query": "scene document new close", "limit": 50})
        assert {"scene.new", "scene.close"} <= {item["id"] for item in found["items"]}, found
        for operation in ("scene.new", "scene.close"):
            schema = agent.request("api.describe", {"operation": operation})
            assert {"document", "revision"} <= set(schema["inputSchema"]["required"])
            assert schema["inputSchema"]["properties"]["action"]["enum"] == [0, 1, 2], schema
            assert not schema["unavailable"]
        original = info["document"]
        info = lifecycle(agent, "scene.new", info)
        info = ready(agent)
        assert info["document"] != original and info["nodes"] == "0" and not info["dirty"] and not info["path"]
        empty_path = output / f"Empty-{mcp}.hasset"
        completed(agent.wait(agent.call("scene.save", **request(info), path=str(empty_path))))
        info = ready(agent)
        closed = lifecycle(agent, "scene.close", info)
        assert not closed["loaded"] and not closed["ready"] and not closed["dirty"] and not closed["path"]
        assert not closed["canUndo"] and not closed["canRedo"]
        assert app.process.poll() is None
        denied = agent.call("scene.node.create", **request(closed), name="Must not create")
        assert denied["error"]["code"] == "unavailable", denied
        opened = completed(agent.wait(agent.call("scene.open", **request(closed), path=str(empty_path))))["scene"]
        assert opened["ready"] and opened["nodes"] == "0"
        info = ready(agent)
        node = completed(agent.call("scene.node.create", **request(info), name="Unsaved lifecycle node"))
        info = ready(agent)
        for operation in ("scene.new", "scene.close"):
            rejection(agent, operation, info, "dirty_document")
            rejection(agent, operation, info, "invalid_arguments", action=99)
        stale = agent.call("scene.close", document=original, revision=info["revision"], action=2)
        assert stale["error"]["code"] == "stale_document", stale
        stale = agent.call("scene.new", document=info["document"], revision="0", action=2)
        assert stale["error"]["code"] == "stale_revision", stale
        blocker = output / f"Blocker-{mcp}"
        blocker.write_text("force asynchronous IO failure", encoding="utf-8")
        rejection(agent, "scene.close", info, "save_failed", action=1, scenePath=str(blocker / "Scene.hasset"))
        saved_path = output / f"Saved-{mcp}.hasset"
        closed = lifecycle(agent, "scene.close", info, action=1, scenePath=str(saved_path))
        assert not closed["loaded"] and saved_path.is_file()
        info = lifecycle(agent, "scene.new", closed)
        stale = agent.call("scene.node.get", document=node["document"], handle=node["handle"])
        assert stale["error"]["code"] == "stale_document", stale
        info = ready(agent)
        completed(agent.call("scene.node.create", **request(info), name="Untitled edit"))
        info = ready(agent)
        rejection(agent, "scene.new", info, "invalid_arguments", action=1)
        untitled_path = output / f"Untitled-{mcp}.hasset"
        info = lifecycle(agent, "scene.new", info, action=1, scenePath=str(untitled_path))
        assert untitled_path.is_file() and info["nodes"] == "0"
        info = ready(agent)
        completed(agent.call("scene.node.create", **request(info), name="Discard edit"))
        info = ready(agent)
        asset = completed(agent.wait(agent.call("asset.open", path="/Game/Texture.hasset")))
        asset = completed(agent.call("asset.rename", document=asset["document"], generation=asset["generation"],
                                     name=f"Retained after scene close {mcp}"))
        info = lifecycle(agent, "scene.new", info, action=2)
        assert info["nodes"] == "0" and not info["dirty"]
        info = ready(agent)
        closed = lifecycle(agent, "scene.close", info)
        retained = completed(agent.call("asset.info", document=asset["document"]))
        assert retained["dirty"] and retained["generation"] == asset["generation"]
        completed(agent.wait(agent.call("asset.save", document=asset["document"], generation=asset["generation"])))
        assert not completed(agent.call("asset.info", document=asset["document"]))["dirty"]
        capture = output / f"Closed-{mcp}.png"
        completed(agent.wait(agent.call("render.screenshot", path=str(capture), overwrite=True)))
        assert capture.is_file() and app.process.poll() is None
        completed(agent.call("asset.close", document=asset["document"], generation=asset["generation"]))
        completed(agent.call("application.close.request"))
        app.process.wait(timeout=30)
        assert app.process.returncode == 0, app.path.read_text(encoding="utf-8", errors="replace")
        assert "Final graphics validation errors: 0" in app.path.read_text(encoding="utf-8", errors="replace")
        print(f"PASS: scene new/close, persistence, stale/dirty/failure rejection and retained assets ({mcp=})",
              flush=True)
    finally:
        if agent:
            agent.close()
        app.close()


if __name__ == "__main__":
    cli, editor, fixture, output = [pathlib.Path(argument).resolve() for argument in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    assets = output / "Assets"
    subprocess.run([str(fixture), str(assets)], check=True)
    for mcp in (True, False):
        workflow(cli, editor, assets / "Game", output, mcp)
