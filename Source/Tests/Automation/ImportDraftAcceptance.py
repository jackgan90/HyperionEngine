"""Unpublished drafts through the shipped JSONL/MCP catalog, against isolated files."""
import copy
import json
import pathlib
import subprocess
import sys
import tempfile
import time

from AutomationAcceptance import Session, completed
from ImportAcceptance import generate, prepare


def wait_draft(session, state, expected="ready"):
    deadline = time.monotonic() + 30
    while state["status"] in ("preparing", "publishing"):
        assert time.monotonic() < deadline, state
        time.sleep(0.01)
        state = completed(session.call("asset.import.draft.get", draft=state["draft"]))
    assert state["status"] == expected, state
    return state


def edit(session, state, properties):
    return completed(session.call("asset.import.draft.edit", draft=state["draft"],
                                  generation=state["generation"], properties=properties))


def submit(session, state):
    task = completed(session.call("asset.import.draft.submit", draft=state["draft"], generation=state["generation"]))
    state = wait_draft(session, completed(session.call("asset.import.draft.get", draft=state["draft"])))
    task = completed(session.call("asset.import.task", task=task["task"]))
    assert task["status"] == "completed", task
    return state, task["result"]


def discard(session, state):
    completed(session.call("asset.import.draft.discard", draft=state["draft"],
                           generation=state["generation"], discard=True))


def check_history_rejections(session, state):
    history_error = "History requires available undo, redo or reset"
    cases = [({"action": action}, "invalid_arguments", history_error)
             for action in ("", "unknown", "UNDO", " undo", "undo", "redo")]
    cases.extend([
        ({}, "invalid_arguments", history_error),
        ({"action": "unknown", "draft": "missing"}, "not_found",
         "Import draft was discarded or belongs to another root"),
        ({"action": "unknown", "generation": str(int(state["generation"]) - 1)}, "stale_revision",
         "Import draft changed before the operation"),
    ])
    tasks = completed(session.call("asset.import.tasks"))
    for overrides, code, message in cases:
        request = {"draft": state["draft"], "generation": state["generation"]} | overrides
        rejected = session.call("asset.import.draft.history", **request)
        assert rejected["status"] == "failed" and rejected["error"]["code"] == code, rejected
        assert rejected["error"]["message"] == message, rejected
        assert completed(session.call("asset.import.draft.get", draft=state["draft"])) == state
    rejected = session.call("asset.import.draft.history", draft=state["draft"],
                            generation=state["generation"], action=1)
    assert rejected["status"] == "failed" and rejected["error"]["code"] == "invalid_arguments", rejected
    assert completed(session.call("asset.import.draft.get", draft=state["draft"])) == state
    assert completed(session.call("asset.import.tasks")) == tasks


def check_inspection_failure(session, state):
    edits = []
    for node in state["nodes"]:
        matrix = copy.deepcopy(node["local"])
        for index in (0, 5, 10, 12, 13, 14):
            matrix["values"][index] = 3e38
        edits.append({"id": node["id"], "local": matrix})
    rejected = session.call("asset.import.draft.edit", draft=state["draft"], generation=state["generation"],
                            properties={"nodes": edits})
    assert rejected["status"] == "failed" and "bounds" in rejected["error"]["message"], rejected
    assert completed(session.call("asset.import.draft.get", draft=state["draft"])) == state
    undone = completed(session.call("asset.import.draft.history", draft=state["draft"],
                                    generation=state["generation"], action="undo"))
    restored = completed(session.call("asset.import.draft.history", draft=state["draft"],
                                      generation=undone["generation"], action="redo"))
    assert restored["properties"] == state["properties"] and restored["nodes"] == state["nodes"]
    return restored


def check_failed_preparation(session, directory, request):
    source = directory / "Models/InvalidBounds.gltf"
    model = json.loads((directory / "Models/Showcase.gltf").read_text(encoding="utf-8"))
    for node in model["nodes"]:
        for field in ("translation", "rotation", "scale"):
            node.pop(field, None)
        node["matrix"] = [3e38, 0, 0, 0, 0, 3e38, 0, 0, 0, 0, 3e38, 0, 3e38, 3e38, 3e38, 1]
    source.write_text(json.dumps(model), encoding="utf-8")
    state = wait_draft(session, completed(session.call("asset.import.draft.prepare", **(request | {
        "source": str(source), "output": "/Game/InvalidBounds.hasset"}))), expected="failed")
    assert "bounds" in state["error"], state
    assert completed(session.call("asset.import.draft.get", draft=state["draft"])) == state
    assert not (directory / "Game/InvalidBounds.hasset").exists()
    discard(session, state)


def workflow(cli, tool, directory, mcp):
    prepare(directory)
    generate(directory / "Models")
    session = Session(cli, directory / "Game", mcp=mcp)
    try:
        generation = completed(session.call("content.root.get"))["generation"]
        request = dict(generation=generation, source=str(directory / "Color.png"),
                       output="/Game/Draft.hasset", library="/Game")
        description = session.request("api.describe", {"operation": "asset.import.draft.edit"})
        assert "properties" in description["inputSchema"]["properties"]
        discovered = session.request("api.search", {"query": "import draft", "limit": 50})
        assert {"asset.import.drafts", *("asset.import.draft." + action for action in
                ("prepare", "get", "edit", "history", "submit", "discard"))} <= {item["id"] for item in discovered["items"]}
        state = wait_draft(session, completed(session.call("asset.import.draft.prepare", **request)))
        check_history_rejections(session, state)
        description = session.request("api.describe", {"operation": "asset.import.draft.get"})
        metadata = description["outputSchema"]["properties"]
        assert metadata["pixelBytes"]["type"] == "string"
        assert len(metadata["dimension"]["enum"]) == 2
        assert state["dimension"] == 0 and state["width"] == 8 and state["height"] == 8, state
        assert state["pixelBytes"] == "340" and state["details"] == ["Texture2D | pixel bytes: 340"], state
        assert completed(session.call("asset.import.draft.get", draft=state["draft"])) == state
        assert not list((directory / "Game").rglob("*.hasset"))
        assert session.call("asset.import.draft.get", draft=state["draft"], limit=65)["error"]["code"] == "invalid_arguments"
        old = state
        state = edit(session, state, {"name": "Edited texture"})
        assert state["name"] == "Edited texture" and state["dirty"]
        assert session.call("asset.import.draft.edit", draft=state["draft"], generation=old["generation"],
                            properties={})["error"]["code"] == "stale_revision"
        assert session.call("content.root.clear", generation=generation)["error"]["code"] == "dirty_document"
        for action, name in (("undo", "Color"), ("redo", "Edited texture"), ("reset", "Color"), ("undo", "Edited texture")):
            state = completed(session.call("asset.import.draft.history", draft=state["draft"],
                                           generation=state["generation"], action=action))
            assert state["name"] == name
        state, result = submit(session, state)
        assert result["writtenAssets"] == "1"
        discard(session, state)
        state = wait_draft(session, completed(session.call("asset.import.draft.prepare", **request)))
        state = edit(session, state, {"name": "Edited texture"})
        state, result = submit(session, state)
        assert result["upToDate"] and result["writtenAssets"] == "0"
        discard(session, state)

        model = request | {"source": str(directory / "Models/Showcase.gltf"), "output": "/Game/Model.hasset"}
        state = wait_draft(session, completed(session.call("asset.import.draft.prepare", **model)))
        assert state["nodes"] and state["primitives"] and state["products"]
        node = state["nodes"][0]
        primitive = state["primitives"][0]
        matrix = copy.deepcopy(node["local"])
        matrix["values"][12] += 2
        properties = {"name": "Draft model", "nodes": [{"id": node["id"], "name": "Edited node", "local": matrix}],
                      "primitives": [{"id": primitive["id"], "name": "Edited primitive", "material": primitive["material"]}]}
        state = edit(session, state, properties)
        assert state["nodes"][0]["local"] == matrix and state["nodes"][0]["id"] == node["id"]
        state = check_inspection_failure(session, state)
        bad = session.call("asset.import.draft.edit", draft=state["draft"], generation=state["generation"],
                           properties={"nodes": [{"id": "missing", "name": "bad"}]})
        assert bad["status"] == "failed"
        state, result = submit(session, state)
        assert int(result["writtenAssets"]) > 1
        export = directory / "Model.json"
        subprocess.run([str(tool), "--asset-root", str(directory / "Game"), "export-json",
                        "/Game/Model.hasset", str(export)], check=True, capture_output=True)
        assert "Edited node" in export.read_text(encoding="utf-8")
        state, result = submit(session, state)
        assert result["upToDate"]
        discard(session, state)
        check_failed_preparation(session, directory, request)

        # Wrapping a prepared model into a scene retains root edits on the model product.
        state = wait_draft(session, completed(session.call("asset.import.draft.prepare", **(model | {"scene": True, "output": "/Game/Wrapped.hasset"}))))
        state = edit(session, state, {"name": "Wrapped model"})
        state, result = submit(session, state)
        assert result["asset"]["type"] == "hyperion.scene"
        discard(session, state)
        for source in ("/Game/Wrapped.hasset", "/Engine/Materials/DefaultPrimitive.hasset"):
            assert session.call("asset.import.draft.prepare", **(request | {
                "source": source, "output": "/Game/RejectedNative.hasset"}))["status"] == "failed"
        assert not (directory / "Game/RejectedNative.hasset").exists()

        sky_request = request | {"source": str(directory / "Sky.hdr"), "output": "/Game/Sky.hasset",
                                 "sky": {"radianceSize": 8, "specularSize": 4, "samples": 8}}
        state = wait_draft(session, completed(session.call("asset.import.draft.prepare", **sky_request)))
        assert state["products"][0]["width"] > 0
        assert state["sourceWidth"] == 2 and state["sky"]["radianceSize"] == 8
        state = edit(session, state, {"name": "Draft sky"})
        state, result = submit(session, state)
        state, result = submit(session, state)
        assert result["upToDate"]
        # Even a current output must not bypass draft source fingerprint checks.
        (directory / "Sky.hdr").write_bytes((directory / "Sky.hdr").read_bytes()[:-1] + bytes([130]))
        task = completed(session.call("asset.import.draft.submit", draft=state["draft"], generation=state["generation"]))
        state = wait_draft(session, completed(session.call("asset.import.draft.get", draft=state["draft"])))
        assert state["error"]
        assert completed(session.call("asset.import.task", task=task["task"]))["status"] == "failed"
        discard(session, state)
        assert completed(session.call("asset.import.drafts"))["drafts"] == []
    finally:
        session.close()


if __name__ == "__main__":
    cli, tool, output = map(lambda value: pathlib.Path(value).resolve(), sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    run = pathlib.Path(tempfile.mkdtemp(prefix="Draft-", dir=output))
    for mcp in (False, True):
        workflow(cli, tool, run / ("Mcp" if mcp else "Jsonl"), mcp)
    print(f"Draft preparation/edit/history/publication parity passed: {run}")
