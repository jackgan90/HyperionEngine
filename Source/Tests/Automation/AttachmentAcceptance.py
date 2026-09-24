"""Real CLI/MCP attachment to owned Editor/Viewer processes and isolated scene files."""
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import time

from AutomationAcceptance import Session, completed


class AttachedSession(Session):
    def __init__(self, executable, instance, mcp):
        self.connection = None
        super().__init__(executable, mcp=mcp)
        probe = completed(self.request("targets.probe", {"instance": instance, "timeoutMs": 5000}))
        assert probe["reachable"] and "connection" not in probe, probe
        assert probe["target"]["instance"] == instance and probe["target"]["label"], probe
        self.hello = completed(self.request("targets.connect", {"instance": instance}))
        self.connection = self.hello["connection"]

    def request(self, method, parameters):
        parameters = dict(parameters)
        if self.connection and not method.startswith("targets."):
            parameters.setdefault("connection", self.connection)
        return super().request(method, parameters)


class Application:
    def __init__(self, executable, output, name, scene, asset_root, disabled=False, frames=2400, model=False):
        self.path = output / (name + ".log")
        self.log = self.path.open("w", encoding="utf-8")
        self.instance = None
        self.process = None
        args = [str(executable), "--hidden", "--frames", str(frames),
                "--asset-root", str(asset_root), "--model" if model else "--scene", str(scene)]
        if "editor" in executable.stem:
            args += ["--layout", str(output / (name + "-layout.ini")),
                     "--ui-preferences", str(output / (name + "-scale.ini")),
                     "--editor-preferences", str(output / (name + "-preferences.ini"))]
        else:
            args += ["--no-ui"]
        if disabled:
            args += ["--disable-plugin", "automation-local"]
        self.process = subprocess.Popen(args, stdout=self.log, stderr=subprocess.STDOUT)

    def target(self):
        deadline = time.monotonic() + 40
        while time.monotonic() < deadline:
            text = self.path.read_text(encoding="utf-8", errors="replace")
            match = re.search(r"Automation target: ([0-9a-f]{32})", text)
            if match:
                self.instance = match[1]
                return self.instance
            assert self.process.poll() is None, text
            time.sleep(0.02)
        raise TimeoutError(self.path.read_text(encoding="utf-8", errors="replace"))

    def finish(self):
        try:
            self.process.wait(timeout=75)
            assert self.process.returncode == 0, self.path.read_text(encoding="utf-8", errors="replace")
        finally:
            self.close()

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=10)
        self.log.close()


def ready(session):
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        info = completed(session.call("scene.info"))
        if info["ready"] and not info["busy"]:
            return info
        time.sleep(0.02)
    raise TimeoutError(info)


def node(session, info):
    page = completed(session.call("scene.nodes.list", document=info["document"],
                                  revision=info["revision"], limit=100))
    return next(item for item in page["nodes"] if item["kind"] == "Model")


def inspect(session, info, handle):
    return completed(session.call("scene.node.get", document=info["document"], handle=handle))


def transform(session, info, handle, matrix):
    return session.call("scene.nodes.set_transform", document=info["document"],
                        revision=info["revision"], transforms=[{"handle": handle, "local": matrix}])


def workflow(cli, editor, viewer, root, output):
    assets = root.parent / "HyperionAssets"
    apps = []
    sessions = []
    try:
        first = Application(editor, output, "editor", "/Game/Scenes/Sponza.hasset", assets)
        apps.append(first)
        second = Application(viewer, output, "viewer", "/Game/Scenes/Sponza.hasset", assets)
        apps.append(second)
        agent = AttachedSession(cli, first.target(), True)
        sessions.append(agent)
        peer = AttachedSession(cli, second.target(), False)
        sessions.append(peer)
        candidates = completed(agent.request("targets.list", {}))["targets"]
        assert {first.instance, second.instance} <= {item["instance"] for item in candidates}
        tools = agent.rpc("tools/list", {})["tools"]
        expected_tools = {"engine.info", "api.search", "api.describe", "types.describe", "api.call",
                          "jobs.get", "jobs.cancel", "targets.list", "targets.probe", "targets.connect", "targets.disconnect"}
        assert {tool["name"] for tool in tools} == expected_tools, tools
        assert "connection" in next(tool for tool in tools if tool["name"] == "api.call")["inputSchema"]["properties"]
        info = ready(agent)
        target_root = completed(agent.call("content.root.get"))
        assert pathlib.Path(target_root["directory"]) == assets
        unchanged = completed(agent.call("content.root.set", directory=str(assets), generation=target_root["generation"]))
        assert unchanged == target_root and ready(agent)["document"] == info["document"]
        viewer_info = ready(peer)
        assert info["history"] and not viewer_info["history"]

        # Asynchronous geometry/material preparation must not mark a freshly loaded document dirty.
        assert not info["dirty"] and not viewer_info["dirty"]
        assert info["document"] != viewer_info["document"]
        schema = agent.request("api.describe", {"operation": "scene.nodes.set_transform"})
        assert "revision" in schema["inputSchema"]["required"]
        definition = peer.request("api.describe", {"operation": "scene.undo"})
        assert definition["unavailable"], definition
        selected = node(agent, info)
        viewer_node = node(peer, viewer_info)
        original = selected["local"]
        matrix = {"values": list(original["values"])}
        matrix["values"][12] += 1.25
        changed = completed(transform(agent, info, selected["handle"], matrix))
        assert changed["dirty"] and changed["canUndo"]
        assert inspect(agent, changed, selected["handle"])["local"] == matrix
        assert transform(agent, info, selected["handle"], matrix)["error"]["code"] == "stale_revision"
        assert inspect(peer, viewer_info, viewer_node["handle"])["local"] == viewer_node["local"]
        foreign = peer.call("scene.node.get", document=info["document"], handle=selected["handle"])
        assert foreign["error"]["code"] == "stale_document"
        undone = completed(agent.call("scene.undo", document=changed["document"], revision=changed["revision"]))
        assert not undone["dirty"] and inspect(agent, undone, selected["handle"])["local"] == original
        redone = completed(agent.call("scene.redo", document=undone["document"], revision=undone["revision"]))
        destination = output / "Edited.hasset"
        saved = completed(agent.wait(agent.call("scene.save", document=redone["document"],
                                                revision=redone["revision"], path=str(destination))))
        assert not saved["dirty"] and destination.is_file()
        once = subprocess.run([str(cli), "--attach", first.instance, "api.call", "--json",
                               json.dumps({"operation": "scene.node.get", "arguments": {
                                   "document": saved["document"], "handle": selected["handle"]}})],
                              capture_output=True, text=True, timeout=20)
        assert once.returncode == 0, once.stderr
        assert completed(json.loads(once.stdout))["local"] == matrix
        # Viewer supports the same transform and save domain without advertising history.
        viewer_matrix = {"values": list(viewer_node["local"]["values"])}
        viewer_matrix["values"][13] += 2
        viewer_changed = completed(transform(peer, viewer_info, viewer_node["handle"], viewer_matrix))
        assert viewer_changed["dirty"] and not viewer_changed["canUndo"]
        rejected = peer.call("scene.undo", document=viewer_changed["document"], revision=viewer_changed["revision"])
        assert rejected["error"]["code"] == "unavailable"
        completed(agent.request("targets.disconnect", {"connection": agent.connection}))
        assert agent.call("scene.info")["error"]["code"] == "disconnected"
        assert first.process.poll() is None
        # A new connection receives a new session and observes the existing shared document.
        reattached = AttachedSession(cli, first.instance, False)
        sessions.append(reattached)
        assert reattached.hello["session"] != agent.hello["session"]
        assert ready(reattached)["document"] == saved["document"]
        for session in sessions:
            session.close()
        sessions.clear()
        first.finish()
        second.finish()
        # Reopen the saved file in a newly launched application and query actual loaded values.
        reopened = Application(viewer, output, "reopened", destination, assets, frames=900)
        apps.append(reopened)
        reader = AttachedSession(cli, reopened.target(), True)
        sessions.append(reader)
        reopened_info = ready(reader)
        assert reopened_info["document"] != saved["document"]
        reopened_node = node(reader, reopened_info)
        assert reopened_node["id"] == selected["id"] and reopened_node["local"] == matrix
        reader.close()
        sessions.clear()
        reopened.finish()
        disabled = Application(editor, output, "disabled", "/Game/Scenes/Sponza.hasset", assets, True, frames=16)
        apps.append(disabled)
        disabled.finish()
        assert "Automation target:" not in disabled.path.read_text(encoding="utf-8")
    finally:
        for session in sessions:
            session.close()
        for app in apps:
            app.close()


def failure_paths(editor, viewer, root, output):
    # A broken discovery directory must isolate the attachment plugin, not the application.
    blocker = output / "not-a-directory"
    blocker.write_text("discovery startup failure fixture", encoding="utf-8")
    environment = dict(os.environ, LOCALAPPDATA=str(blocker))
    for executable in (editor, viewer):
        name = executable.stem
        args = [str(executable), "--hidden", "--frames", "16", "--asset-root",
                str(root.parent / "HyperionAssets"), "--scene", "/Game/Scenes/Sponza.hasset"]
        if "editor" in name:
            args += ["--layout", str(output / "failed-layout.ini"),
                     "--ui-preferences", str(output / "failed-scale.ini"),
                     "--editor-preferences", str(output / "failed-preferences.ini")]
        else:
            args += ["--no-ui"]
        result = subprocess.run(args, env=environment, capture_output=True, text=True, timeout=45)
        log = result.stdout + result.stderr
        (output / (name + "-listener-failure.log")).write_text(log, encoding="utf-8")
        assert result.returncode == 0, log
        assert "Plugin automation-local:" in log and "Automation target:" not in log, log
        assert "validation errors: 0" in log, log


def main():
    cli, editor, viewer, root = [pathlib.Path(value).resolve() for value in sys.argv[1:5]]
    parent = root / "out" / "attachment-tests"
    parent.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix="acceptance-", dir=parent))
    workflow(cli, editor, viewer, root, output)
    failure_paths(editor, viewer, root, output)
    print(f"Live Editor/Viewer CLI+MCP, isolation, history, save/reopen and disablement passed: {output}")


if __name__ == "__main__":
    main()
