"""Preserve preview schemas and numeric choices through attached clients."""
import argparse
import json
import pathlib
import subprocess
import tempfile
import time

from AutomationAcceptance import Session, completed
from AttachmentAcceptance import Application, AttachedSession, ready


def await_preview(agent, document):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        result = completed(agent.call("asset.preview.get", document=document))
        assert not result["error"], result
        if result["ready"]:
            return result
        time.sleep(0.01)
    raise TimeoutError(result)


def preview_schema(agent):
    operations = ("asset.preview.get", "asset.preview.set")
    found = agent.request(
        "api.search", {"query": "asset.preview", "limit": 50})
    assert set(operations) <= {item["id"] for item in found["items"]}, found
    result = {
        name: agent.request("api.describe", {"operation": name})
        for name in operations
    }
    for name in ("hyperion.asset.preview.settings",
                 "hyperion.asset.preview.state",
                 "automation.asset.preview.edit"):
        result[name] = agent.request("types.describe", {"type": name})
    return result


def check_options(agent, peer, path, field, values, invalid):
    opened = completed(agent.wait(agent.call("asset.open", path=path)))
    document = opened["document"]
    query = {"document": document, "generation": opened["generation"]}
    original = completed(agent.call("asset.info", document=document))
    initial = await_preview(agent, document)
    assert initial["settings"][field] == 0, initial
    for value in values:
        changed = completed(agent.call(
            "asset.preview.set", **query, settings={field: value}))
        assert changed["settings"][field] == value, changed
        current = await_preview(peer, document)
        assert current["settings"][field] == value, current
        assert current["generation"] == query["generation"], current
        # Same-value and null patches keep the prepared preview ready.
        repeated = completed(peer.call(
            "asset.preview.set", **query, settings={field: value}))
        assert repeated["ready"], repeated
        assert repeated["settings"] == current["settings"], repeated
        unchanged = completed(agent.call(
            "asset.preview.set", **query, settings={field: None}))
        assert unchanged["ready"], unchanged
        assert unchanged["settings"] == current["settings"], unchanged
    before = await_preview(agent, document)
    for value in invalid:
        settings = {field: value}
        if field == "channel":
            settings["checker"] = not before["settings"]["checker"]
        else:
            settings["exposure"] = 2.0
        rejected = agent.call("asset.preview.set", **query, settings=settings)
        assert rejected["status"] == "failed", rejected
        assert rejected["error"]["code"] == "invalid_arguments", rejected
        current = completed(peer.call("asset.preview.get", document=document))
        assert current == before
    unsupported = "shape" if field == "channel" else "channel"
    rejected = peer.call(
        "asset.preview.set", **query, settings={unsupported: 0})
    assert rejected["status"] == "failed", rejected
    assert rejected["error"]["code"] == "invalid_arguments", rejected
    current = completed(agent.call("asset.preview.get", document=document))
    assert current == before
    after = completed(agent.call("asset.info", document=document))
    for key in ("generation", "dirty", "canUndo", "canRedo"):
        assert after[key] == original[key], (key, original, after)
    completed(agent.call("asset.close", **query))


def run(cli, editor, fixture, output, record_schema):
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
            prefix="preview-", dir=output) as temporary:
        assets = pathlib.Path(temporary)
        subprocess.run([str(fixture), str(assets)], check=True)
        app = Application(
            editor, output, "preview-options", "", assets / "Game", frames=0)
        sessions = []
        try:
            agent = AttachedSession(cli, app.target(), True)
            sessions.append(agent)
            peer = AttachedSession(cli, app.instance, False)
            sessions.append(peer)
            ready(agent)
            schema = preview_schema(agent)
            if record_schema:
                record_schema.write_text(
                    json.dumps(schema, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")
            else:
                baseline = pathlib.Path(__file__).with_name(
                    "AssetPreviewSchema.json")
                expected = json.loads(baseline.read_text(encoding="utf-8"))
                assert schema == expected, "Preview schema changed"
            assert preview_schema(peer) == schema
            check_options(agent, peer, "/Game/Texture.hasset", "channel",
                          (0, 1, 2, 3, 4), (5, 4294967295))
            check_options(agent, peer,
                          "/Engine/Materials/DefaultPrimitive.hasset", "shape",
                          (0, 1, 2), (3, 4294967295))
            closed = completed(agent.call("application.close.request"))
            assert closed["state"] == "closing", closed
            app.process.wait(timeout=30)
            assert app.process.returncode == 0, app.path.read_text(
                encoding="utf-8")
        finally:
            for session in sessions:
                session.close()
            app.close()
    standalone = Session(cli)
    try:
        result = standalone.call("asset.preview.get", document="unavailable")
        assert result["status"] == "failed", result
        assert result["error"]["code"] == "unavailable", result
    finally:
        standalone.close()
    print("Preview schemas, MCP/JSONL choices, rejection, readiness and "
          "document state passed", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("cli", "editor", "fixture", "output"):
        parser.add_argument(
            name, type=lambda value: pathlib.Path(value).resolve())
    parser.add_argument("--record-schema", type=pathlib.Path)
    args = parser.parse_args()
    run(args.cli, args.editor, args.fixture, args.output, args.record_schema)
