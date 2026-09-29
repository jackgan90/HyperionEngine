"""Exercise shared HUD preferences, effective state, persistence and failure via real CLI/MCP."""
import pathlib
import stat
import sys
import tempfile

from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession


def exercise(cli, editor, root, compiled):
    parent = root / "out" / "renderdoc-hud-tests"
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(dir=parent))
    assets = output / "Game"
    assets.mkdir()
    preferences = output / "Preferences.ini"
    preferences.write_text("version=1\nrenderdoc_capture=true\n", encoding="utf-8")
    runtime = pathlib.Path(r"C:\Program Files\RenderDoc\renderdoc.dll")
    with_runtime = compiled and runtime.is_file()
    for index, disabled in enumerate((False, False, True)):
        extra = ["--editor-preferences", preferences]
        if disabled:
            extra += ["--disable-plugin", "renderdoc"]
        app = Application(editor, output, f"hud-{index}", "", assets,
                          frames=0, extra=extra)
        agent = None
        peer = None
        try:
            agent = AttachedSession(cli, app.target(), True)
            peer = AttachedSession(cli, app.instance, False)
            found = agent.request("api.search", {"query": "renderdoc.hud"})
            assert {"renderdoc.hud.get", "renderdoc.hud.set"} <= {item["id"] for item in found["items"]}
            schema = agent.request("api.describe", {"operation": "renderdoc.hud.set"})
            assert not schema["unavailable"] and schema["inputSchema"]["properties"]["enabled"]["type"] == "boolean", schema
            before = completed(agent.call("renderdoc.hud.get"))
            expected = index != 0
            effective = expected if with_runtime and not disabled else None
            assert before == {"preference": expected, "enabled": effective}, before
            capture = completed(agent.call("renderdoc.status"))
            for arguments in ({}, {"enabled": "false"}, {"enabled": True, "unknown": 1}):
                assert agent.call("renderdoc.hud.set", **arguments)["error"]["code"] == "invalid_arguments"
                assert completed(peer.call("renderdoc.hud.get")) == before
            saved = preferences.read_bytes()
            preferences.chmod(stat.S_IREAD)
            try:
                failed = agent.call("renderdoc.hud.set", enabled=not expected)
                assert failed["error"]["code"] == "save_failed", failed
                assert completed(peer.call("renderdoc.hud.get")) == before
                assert preferences.read_bytes() == saved
            finally:
                preferences.chmod(stat.S_IREAD | stat.S_IWRITE)
            for enabled in (True, False, True):
                result = completed(agent.call("renderdoc.hud.set", enabled=enabled))
                assert result == {"preference": enabled,
                                  "enabled": enabled if with_runtime and not disabled else None}, result
                assert completed(peer.call("renderdoc.hud.get")) == result
                assert f"renderdoc_hud={str(enabled).lower()}" in preferences.read_text()
                assert completed(peer.call("renderdoc.status")) == capture
            completed(agent.call("application.close.request"))
            app.finish()
        finally:
            if peer:
                peer.close()
            if agent:
                agent.close()
            app.close()
    print(f"PASS: HUD discovery, CLI/MCP parity, live state, restart, disablement and save failure: {output}")


if __name__ == "__main__":
    exercise(*(pathlib.Path(value).resolve() for value in sys.argv[1:4]), sys.argv[4] == "1")
