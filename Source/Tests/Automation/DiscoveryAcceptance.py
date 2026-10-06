"""Saturated discovery, process death and shared Editor/CLI/MCP run isolation."""
import json
import os
import pathlib
import subprocess
import sys
import tempfile

from AttachmentAcceptance import Application, AttachedSession
from AutomationAcceptance import Session, completed
from DiscoveryEnvironment import ensure_isolated_discovery


def check_run_isolation():
    wrapper = pathlib.Path(__file__).with_name("DiscoveryEnvironment.py")
    roots = []
    for _ in range(2):
        result = subprocess.run(
            [sys.executable, str(wrapper), sys.executable, "-c",
             "import os; print(os.environ['LOCALAPPDATA'])"],
            capture_output=True, text=True, check=True)
        roots.append(pathlib.Path(result.stdout.strip()))
    assert roots[0] != roots[1] and all(root.is_dir() for root in roots), roots
    assert all(root != pathlib.Path(os.environ["LOCALAPPDATA"]) for root in roots)
    result = subprocess.run([sys.executable, str(wrapper), sys.executable, "-c", "raise SystemExit(77)"])
    assert result.returncode == 77, result.returncode


def check_attachment(cli, instance):
    for mcp in (False, True):
        session = AttachedSession(cli, instance, mcp)
        try:
            assert session.request("engine.info", {})["target"]["instance"] == instance
            assert set(session.hello["target"]) == {"instance", "application", "build", "address", "mode", "label"}
        finally:
            session.close()
    result = subprocess.run([str(cli), "--attach", instance, "engine.info"],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    assert json.loads(result.stdout)["target"]["instance"] == instance, result.stdout


def saturation(cli, editor, root, output):
    run_root = ensure_isolated_discovery()
    session = Session(cli)
    apps = []
    try:
        completed(session.request("targets.list", {}))
        directory, = pathlib.Path(os.environ["HYP_DISCOVERY_ROOT"]).iterdir()
        for index in range(160):
            instance = f"{index + 1:032x}"
            record = {"instance": instance, "application": "LegacyFixture", "build": "old",
                      "address": {"scheme": "npipe", "address": instance}, "mode": "Test", "label": "legacy"}
            (directory / f"{instance}.json").write_text(json.dumps(record), encoding="utf-8")
        first = Application(editor, output, "first", "/Game/Scenes/DoesNotExist.hasset",
                            root.parent / "HyperionAssets", frames=100000)
        apps.append(first)
        instance = first.target()
        path = directory / f"{instance}.json"
        envelope = json.loads(path.read_text())
        assert envelope["recordVersion"] == 1 and envelope["owner"]["processId"] == first.process.pid, envelope
        assert int(envelope["owner"]["creationTime"]) > 0
        listed = completed(session.request("targets.list", {}))
        assert listed["truncated"] and listed["stopReason"] == 1 and len(listed["targets"]) == 128, listed
        check_attachment(cli, instance)

        # Endpoint failure for a demonstrably live owner must leave the record intact.
        unreachable = json.loads(json.dumps(envelope))
        unreachable_id = "e" * 32
        unreachable["target"]["instance"] = unreachable_id
        unreachable["target"]["address"]["address"] = unreachable_id
        unreachable_path = directory / f"{unreachable_id}.json"
        unreachable_path.write_text(json.dumps(unreachable), encoding="utf-8")
        failed = session.request("targets.probe", {"instance": unreachable_id, "timeoutMs": 50})
        assert failed["status"] == "failed" and unreachable_path.exists(), failed

        # Also saturate the scan budget; both frontends must still resolve the exact file.
        for file in directory.glob("*.json"):
            if file.name not in {path.name, unreachable_path.name}:
                file.unlink()
        for index in range(1100):
            (directory / f"{index:05}.tmp").write_text("foreign", encoding="utf-8")
        listed = completed(session.request("targets.list", {}))
        assert listed["truncated"] and listed["stopReason"] == 2 and listed["examined"] == 1024, listed
        check_attachment(cli, instance)
        for file in directory.glob("*.tmp"):
            file.unlink()

        first.close()
        assert path.exists(), "Forced exit should leave the versioned fixture for cleanup"
        listed = completed(session.request("targets.list", {}))
        assert listed["staleSkipped"] == 2 and not listed["targets"] and path.exists(), listed
        absent = session.request("targets.probe", {"instance": instance})
        assert absent["error"]["code"] == "not_found", absent
        second = Application(editor, output, "second", "/Game/Scenes/DoesNotExist.hasset",
                             root.parent / "HyperionAssets", frames=100000)
        apps.append(second)
        second_instance = second.target()
        assert not path.exists() and not unreachable_path.exists(), directory
        agent = AttachedSession(cli, second_instance, True)
        try:
            completed(agent.call("application.close.request"))
        finally:
            agent.close()
        second.finish()
        assert not (directory / f"{second_instance}.json").exists()
    finally:
        session.close()
        for app in apps:
            app.close()


def main():
    cli, editor, root = map(lambda value: pathlib.Path(value).resolve(), sys.argv[1:4])
    run_root = ensure_isolated_discovery()
    check_run_isolation()
    output = pathlib.Path(tempfile.mkdtemp(prefix="acceptance-", dir=run_root))
    saturation(cli, editor, root, output)
    print(f"Saturated exact CLI/MCP attachment, conservative native cleanup and isolation passed: {output}")


if __name__ == "__main__":
    main()
