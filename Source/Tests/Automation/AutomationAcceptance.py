"""Exercise the shipped CLI/JSONL/MCP host against isolated native asset files."""
import json
import pathlib
import queue
import subprocess
import sys
import tempfile
import threading
import time


class Session:
    def __init__(self, executable, asset_root=None, mcp=False):
        self.mcp = mcp
        self.serial = 0
        self.messages = queue.Queue()
        self.errors = tempfile.TemporaryFile(mode="w+b")
        self.process = subprocess.Popen(
            [str(executable), "--mcp" if mcp else "--stdio"] +
            (["--asset-root", str(asset_root)] if asset_root else []),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.errors,
            text=True, encoding="utf-8")
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()
        if mcp:
            reply = self.rpc("initialize", {"protocolVersion": "2025-11-25",
                             "capabilities": {}, "clientInfo": {"name": "acceptance", "version": "1"}})
            assert reply["protocolVersion"] == "2025-11-25"
            self.send({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def read(self):
        for line in self.process.stdout:
            self.messages.put(line)

    def send(self, message):
        self.process.stdin.write(json.dumps(message, ensure_ascii=False) + "\n")
        self.process.stdin.flush()

    def rpc(self, method, parameters):
        self.serial += 1
        request = {"id": self.serial, "method": method, "params": parameters}
        if self.mcp:
            request["jsonrpc"] = "2.0"
        self.send(request)
        response = json.loads(self.messages.get(timeout=15))
        assert response["id"] == self.serial, response
        assert "error" not in response, response
        return response["result"]

    def request(self, method, parameters):
        if self.mcp:
            return self.rpc("tools/call", {"name": method, "arguments": parameters})["structuredContent"]
        return self.rpc(method, parameters)

    def call(self, operation, **arguments):
        return self.request("api.call", {"operation": operation, "arguments": arguments})

    def wait(self, result):
        deadline = time.monotonic() + 15
        if result.get("status") == "running":
            job = result["job"]
            while result["status"] == "running":
                assert time.monotonic() < deadline
                time.sleep(0.005)
                result = self.request("jobs.get", {"job": job})
            return result["outcome"]
        return result

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=15)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait(timeout=5)
            self.reader.join(timeout=5)
            self.process.stdout.close()
            self.errors.seek(0)
            errors = self.errors.read().decode("utf-8")
            self.errors.close()
        assert self.process.returncode in (0, 1), errors


def completed(result):
    assert result["status"] == "completed", result
    return result["result"]


def check_workflow(executable, asset_root, mcp):
    session = Session(executable, mcp=mcp)
    try:
        root_state = completed(session.call("content.root.get"))
        assert root_state["directory"] == "" and root_state["generation"] == "0"
        unset = session.call("asset.open", path="/Game/Texture.hasset")
        assert unset["error"]["code"] == "root_unset"
        root_schema = session.request("api.describe", {"operation": "content.root.set"})
        assert "generation" in root_schema["inputSchema"]["required"]
        root_state = completed(session.call("content.root.set", directory=str(asset_root), generation="0"))
        assert pathlib.Path(root_state["directory"]) == asset_root
        before = session.rpc("tools/list", {}) if mcp else None
        found = session.request("api.search", {"query": "texture encoding"})
        assert "texture.set_encoding" in [item["id"] for item in found["items"]], found
        definition = session.request("api.describe", {"operation": "asset.rename"})
        assert definition["inputSchema"]["properties"]["generation"]["type"] == "string"
        if mcp:
            assert before == session.rpc("tools/list", {})
        state = completed(session.wait(session.call("asset.open", path="/Game/Texture.hasset")))
        document = state["document"]
        state = completed(session.call("asset.rename", document=document, generation=state["generation"], name="测试纹理"))
        assert state["dirty"] and state["name"] == "测试纹理"
        denied_root = session.call("content.root.clear", generation=root_state["generation"])
        assert denied_root["error"]["code"] == "dirty_document"
        same = completed(session.call("content.root.set", directory=str(asset_root), generation=root_state["generation"]))
        assert same == root_state
        bad = session.call("content.root.set", directory=str(asset_root / "Missing"), generation=root_state["generation"], discard=True)
        assert bad["error"]["code"] == "invalid_root"
        assert completed(session.call("asset.info", document=document))["dirty"]
        stale = session.call("asset.rename", document=document, generation="1", name="rejected")
        assert stale["error"]["code"] == "stale_revision"
        state = completed(session.call("asset.undo", document=document, generation=state["generation"]))
        state = completed(session.call("asset.redo", document=document, generation=state["generation"]))
        state = completed(session.wait(session.call("texture.set_encoding", document=document, generation=state["generation"], encoding=1)))
        state = completed(session.wait(session.call("asset.save", document=document, generation=state["generation"])))
        assert not state["dirty"] and state["name"] == "测试纹理"
        root_state = completed(session.call("content.root.set", directory=str(asset_root.parent / "ReadOnly"),
                                            generation=root_state["generation"], readOnly=True))
        assert session.call("asset.info", document=document)["error"]["code"] == "not_found"
        readonly = completed(session.wait(session.call("asset.open", path="/Game/Texture.hasset")))
        assert readonly["readOnly"]
        denied = session.call("asset.rename", document=readonly["document"], generation=readonly["generation"], name="blocked")
        assert denied["error"]["code"] == "read_only"
        stale_root = session.call("content.root.clear", generation="1")
        assert stale_root["error"]["code"] == "stale_revision"
        cleared = completed(session.call("content.root.clear", generation=root_state["generation"]))
        assert not cleared["directory"]
        assert session.call("asset.info", document=readonly["document"])["error"]["code"] == "not_found"
        session.process.stdin.write('{"method":')
        session.process.stdin.flush()
    finally:
        session.close()
    return document


def check_response_limits(executable, asset_root, mcp):
    session = Session(executable, asset_root, mcp=mcp)
    try:
        state = completed(session.wait(session.call("asset.open", path="/Game/Texture.hasset")))
        document = state["document"]
        # The MCP text fallback duplicates and escapes the structured result.
        for name in ("N" * 600000, '"\\' * 160000):
            state = completed(session.call("asset.rename", document=document,
                                           generation=state["generation"], name=name))
            assert state["name"] == name and state["dirty"]
            reopened = completed(session.wait(session.call("asset.open", path="/Game/Texture.hasset")))
            assert reopened == state
            state = completed(session.call("asset.undo", document=document, generation=state["generation"]))
        # An operation may run successfully but produce a result larger than its payload budget.
        # Report that outcome explicitly, without presenting it as a rejected input.
        method = "tools/call" if mcp else "api.call"
        params = {"operation": "asset.rename", "arguments": {
            "document": document, "generation": state["generation"], "name": ""}}
        if mcp:
            params = {"name": "api.call", "arguments": params}
        request = {"id": session.serial + 1, "method": method, "params": params}
        if mcp:
            request["jsonrpc"] = "2.0"
        count = 1024 * 1024 - 32 - len(json.dumps(request).encode("utf-8"))
        renamed = session.call("asset.rename", document=document,
                               generation=state["generation"], name="X" * count)
        assert renamed["error"]["code"] == "result_unavailable"
        restored = completed(session.call("asset.undo", document=document,
                                          generation=str(int(state["generation"]) + 1)))
        assert restored["name"] == state["name"]
    finally:
        session.close()


def check_correlation_limits(executable, mcp):
    session = Session(executable, mcp=mcp)
    try:
        request = {"id": "", "method": "engine.info"}
        if mcp:
            request.update(jsonrpc="2.0", method="tools/call", params={"name": "engine.info"})
        request["id"] = "I" * (1024 * 1024 - 32 - len(json.dumps(request).encode("utf-8")))
        session.send(request)
        response = json.loads(session.messages.get(timeout=15))
        assert response["id"] == request["id"] and "result" in response
        # The session still accepts subsequent calls, even after a failed request with a large ID.
        request["method"] = "not.a.method"
        session.send(request)
        response = json.loads(session.messages.get(timeout=15))
        assert response["id"] == request["id"]
        assert "error" in response if mcp else response["result"]["status"] == "failed"
        assert "session" in session.request("engine.info", {})
    finally:
        session.close()


def run(executable, *arguments, **options):
    return subprocess.run([str(executable), *map(str, arguments)], capture_output=True,
                          encoding="utf-8", timeout=20, **options)


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    fixture = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="HyperionAutomation-") as temporary:
        root = pathlib.Path(temporary)
        assert run(fixture, root).returncode == 0
        asset_root = root / "Game"
        document = check_workflow(executable, asset_root, False)
        check_workflow(executable, asset_root, True)
        for mcp in (False, True):
            check_response_limits(executable, asset_root, mcp)
            check_correlation_limits(executable, mcp)
        opened = run(executable, "api.call", "--asset-root", asset_root, "--json",
                     json.dumps({"operation": "asset.open", "arguments": {"path": "/Game/Texture.hasset"}}))
        assert opened.returncode == 0, opened.stderr
        assert completed(json.loads(opened.stdout))["name"] == "测试纹理"
        unmounted = run(executable, "api.call", "--json", json.dumps({"operation": "content.root.get", "arguments": {}}))
        assert completed(json.loads(unmounted.stdout))["directory"] == ""
        legacy = run(executable, "engine.info", "--mounts", root / "Unused.json")
        assert legacy.returncode == 1 and not legacy.stdout
        # Explicit attachment never falls back to a standalone session, including empty variables.
        for mode in ("engine.info", "--stdio", "--mcp"):
            empty = run(executable, "--attach", "", mode, input="")
            assert empty.returncode == 1 and not empty.stdout and "non-empty" in empty.stderr
        duplicate = run(executable, "--attach", "first", "--attach", "second", "engine.info")
        assert duplicate.returncode == 1 and not duplicate.stdout and "once" in duplicate.stderr
        # One-shot IDs expire with the process; no accidental cross-session addressing.
        foreign = run(executable, "api.call", "--asset-root", asset_root, "--json",
                      json.dumps({"operation": "asset.info", "arguments": {"document": document}}))
        assert foreign.returncode == 1 and json.loads(foreign.stdout)["error"]["code"] == "not_found"
        missing = run(executable, "api.search", "--disable-plugin", "assets")
        assert missing.returncode == 0 and all(not item["available"] for item in json.loads(missing.stdout)["items"])
        failed = run(executable, "api.search", "--engine-content", root / "Missing")
        assert failed.returncode == 0 and all(not item["available"] for item in json.loads(failed.stdout)["items"])
        absent = run(executable, "api.search", "--disable-plugin", "automation-assets", "--disable-plugin", "assets")
        assert absent.returncode == 0 and json.loads(absent.stdout)["total"] == 0
        stopped = run(executable, "engine.info", "--disable-plugin", "automation-session")
        assert stopped.returncode == 1 and not stopped.stdout
        # A redirected file and a pipe obey the same newline framing, including silent MCP notifications.
        commands = '\n'.join(json.dumps({"id": i, "method": "engine.info"}) for i in range(40)) + '\n'
        batch = run(executable, "--stdio", "--asset-root", asset_root, input=commands)
        assert batch.returncode == 0 and len(batch.stdout.splitlines()) == 40, batch.stderr
        stream = root / "Requests.jsonl"
        stream.write_text(commands, encoding="utf-8")
        with stream.open(encoding="utf-8") as source:
            regular = run(executable, "--stdio", "--asset-root", asset_root, stdin=source)
        assert regular.returncode == 0 and len(regular.stdout.splitlines()) == 40
        oversized = run(executable, "--stdio", "--asset-root", asset_root, input='x' * (1024 * 1024 + 1))
        assert oversized.returncode == 1
        # Limits apply to each line, even when one pipe read also contains the next request.
        prefix = '{"id":1,"method":"engine.info","params":{"padding":"'
        suffix = '"}}'
        near_limit = prefix + 'x' * (1024 * 1024 - 10 - len(prefix) - len(suffix)) + suffix
        framed = run(executable, "--stdio", "--asset-root", asset_root,
                     input=near_limit + '\n' + json.dumps({"id": 2, "method": "engine.info"}) + '\n')
        replies = [json.loads(line) for line in framed.stdout.splitlines()]
        assert framed.returncode == 1 and len(replies) == 2, framed.stderr
        assert replies[0]["result"]["error"]["code"] == "invalid_arguments" and "session" in replies[1]["result"]
        # Disconnect immediately after admitting a save; it must finish before process exit.
        session = Session(executable, asset_root)
        state = completed(session.wait(session.call("asset.open", path="/Game/Texture.hasset")))
        state = completed(session.call("asset.rename", document=state["document"], generation=state["generation"], name="EOF save"))
        admitted = session.call("asset.save", document=state["document"], generation=state["generation"])
        assert admitted["status"] == "running"
        session.close()
        opened = run(executable, "api.call", "--asset-root", asset_root, "--json",
                     json.dumps({"operation": "asset.open", "arguments": {"path": "/Game/Texture.hasset"}}))
        assert completed(json.loads(opened.stdout))["name"] == "EOF save"
    print("Automation CLI, JSONL, MCP, absence, read-only and disconnect contracts passed")


if __name__ == "__main__":
    main()
