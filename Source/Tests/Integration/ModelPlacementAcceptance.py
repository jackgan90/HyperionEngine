"""Exercise native model viewport drags and equivalent live CLI/MCP placement."""
import json
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "Automation"))
from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready


def check_automation(cli, editor, output, mcp):
    name = "model-mcp" if mcp else "model-cli"
    app = Application(editor, output, name, output / "Placed.hasset", output / "Game", frames=0)
    agent = None
    operation = "scene.placement.place_model"
    try:
        agent = AttachedSession(cli, app.target(), mcp)
        info = ready(agent)
        schema = agent.request("api.describe", {"operation": operation})
        assert not schema["unavailable"] and set(schema["inputSchema"]["required"]) == {"document", "revision", "model", "position"}, schema
        search = agent.request("api.search", {"query": "place_model"})
        assert any(item["id"] == operation for item in search["items"]), search
        reference = {"path": "/Game/Model.hasset", "type": "hyperion.modelasset"}
        request = dict(document=info["document"], revision=info["revision"], model=reference, position={"x": 3, "y": 2, "z": 1})
        before = int(info["nodes"])
        stale_document = agent.wait(agent.call(operation, **dict(request, document="previous-document")))
        assert stale_document["status"] == "failed" and stale_document["error"]["code"] == "stale_document", stale_document
        for model in (dict(reference, type="hyperion.textureasset"), dict(reference, path="/Game/Broken.hasset"), dict(reference, path="/Game/Failed.hasset")):
            rejected = agent.wait(agent.call(operation, **dict(request, model=model)))
            assert rejected["status"] == "failed", rejected
            expected_code = "invalid_arguments" if model["type"] == "hyperion.textureasset" else "load_failed"
            assert rejected["error"]["code"] == expected_code, rejected
            unchanged = ready(agent)
            assert int(unchanged["nodes"]) == before and unchanged["revision"] == info["revision"], unchanged
        placed = completed(agent.wait(agent.call(operation, **request)))
        assert placed["kind"] == "Model" and placed["name"] == "Model", placed
        info = ready(agent)
        assert int(info["nodes"]) == before + 1 and info["dirty"] and info["canUndo"], info
        selected = completed(agent.call("scene.selection.get", document=info["document"], revision=info["revision"]))
        assert selected["primary"] == placed["handle"], selected
        diagnostics = completed(agent.call("render.component_diagnostics", handle=placed["handle"], component="hyperion.staticmesh", limit=10))
        assert len(diagnostics["primitives"]) == 2, diagnostics
        stale = agent.wait(agent.call(operation, **request))
        assert stale["status"] == "failed" and stale["error"]["code"] == "stale_revision", stale
        completed(agent.call("scene.undo", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        assert int(info["nodes"]) == before, info
        completed(agent.call("scene.redo", document=info["document"], revision=info["revision"]))
        info = ready(agent)
        assert int(info["nodes"]) == before + 1, info
        completed(agent.wait(agent.call("scene.save", document=info["document"], revision=info["revision"], path=str(output / (name + ".hasset")))))
        assert not ready(agent)["dirty"]
    finally:
        if agent:
            agent.close()
        app.close()


def main():
    editor, cli, root = [pathlib.Path(argument).resolve() for argument in sys.argv[1:]]
    parent = root / "out" / "editor-tests"
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix="model-placement-", dir=parent))
    report = output / "Gui.json"
    result = subprocess.run([str(editor), "--hidden", "--exercise-model-placement", str(output),
                             "--layout", str(output / "Layout.ini"), "--ui-preferences", str(output / "Scale.ini"),
                             "--editor-preferences", str(output / "Preferences.ini"), "--report", str(report)],
                            cwd=root, capture_output=True, text=True, timeout=110)
    (output / "Gui.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    assert result.returncode == 0, (output, result.stdout, result.stderr)
    data = json.loads(report.read_text())
    assert data["model_placement_verified"] and data["nodes"] == 2 and not data["document_dirty"], data
    assert data["validation_errors"] == 0 and data["failed_models"] == 0, data
    assert (output / "ModelPreview.png").is_file()
    for mcp in (False, True):
        check_automation(cli, editor, output, mcp)
    print(f"PASS: native model GUI drops, cancellation, references, history, persistence and CLI/MCP parity: {output}")


if __name__ == "__main__":
    main()
