"""Live render controls, depth switching/persistence and feature absence coverage."""
import copy
import csv
import json
import math
import pathlib
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "Automation"))
from AutomationAcceptance import completed
from AttachmentAcceptance import Application, AttachedSession, ready


def live_depth(agent, state, info):
    page = agent.request("api.search", {"query": "render.settings", "limit": 10})
    assert {item["id"] for item in page["items"]} >= {"render.settings.get", "render.settings.set", "render.settings.save"}
    for operation in ("render.settings.get", "render.settings.set"):
        description = agent.request("api.describe", {"operation": operation})
        assert description["version"] == 2, description
    assert agent.request("api.describe", {"operation": "render.settings.save"})["version"] == 1
    original = copy.deepcopy(state["values"])
    before_view = completed(agent.call("view.get"))
    for pipeline in ("forward", "deferred"):
        for reversed_z in (False, True, False, True):
            previous = copy.deepcopy(state)
            frame = int(completed(agent.call("render.statistics"))["frame"])
            candidate = dict(state["values"], pipeline=pipeline, reversedZ=reversed_z)
            state = completed(agent.call("render.settings.set", revision=state["revision"], values=candidate))
            assert state["activeReversedZ"] == reversed_z and state["values"] == candidate, state
            assert int(state["revision"]) == int(previous["revision"]) + 1
            assert completed(agent.call("render.settings.get")) == state
            assert agent.call("render.settings.set", revision=previous["revision"], values=original)["error"]["code"] == "stale_revision"
            invalid = dict(candidate, reversedZ=not reversed_z, pipeline="missing")
            assert agent.call("render.settings.set", revision=state["revision"], values=invalid)["error"]["code"] == "invalid_arguments"
            assert completed(agent.call("render.settings.get")) == state
            deadline = time.monotonic() + 15
            while True:
                stats = completed(agent.call("render.statistics"))
                assert int(stats["device"]["validationErrors"]) == 0, stats
                if int(stats["frame"]) > frame + 2:
                    break
                assert time.monotonic() < deadline, stats
                time.sleep(.02)
            current = ready(agent)
            assert current["revision"] == info["revision"] and not current["dirty"]
            assert completed(agent.call("view.get")) == before_view
    return completed(agent.call("render.settings.set", revision=state["revision"], values=original))


def render_controls(cli, editor, root, output):
    settings_path = output / "Settings.json"
    for index in range(2):
        extra = ("--render-settings", settings_path) if index else ()
        app = Application(editor, output, f"depth-{index}", "/Game/Scenes/Showcase.hasset",
                          root.parent / "HyperionAssets", frames=15000, extra=extra)
        agent = None
        try:
            agent = AttachedSession(cli, app.target(), True)
            info = ready(agent)
            state = completed(agent.call("render.settings.get"))
            assert state["activeReversedZ"] == (index == 0), state
            if index:
                assert not state["values"]["reversedZ"]
            for pipeline, layout in (("forward", "compact"), ("deferred", "compact"), ("deferred", "high")):
                state["values"].update(pipeline=pipeline, gbuffer=layout)
                state = completed(agent.call("render.settings.set", revision=state["revision"], values=state["values"]))
                deadline = time.monotonic() + 15
                expected = "Forward/" if pipeline == "forward" else "Deferred/BasePass/"
                while True:
                    stats = completed(agent.call("render.statistics"))
                    if any(name.startswith(expected) for name in stats["gpuPassMilliseconds"]):
                        break
                    assert time.monotonic() < deadline, stats
                    time.sleep(.02)
                assert int(stats["device"]["validationErrors"]) == 0
                assert ready(agent)["revision"] == info["revision"]
            for culling in range(3):
                view = completed(agent.call("view.set", document=info["document"], revision=info["revision"],
                                             options={"culling": culling, "frozen": True, "instanceBatching": False,
                                                      "modelBounds": True, "lightBounds": True}))
                assert view["options"]["culling"] == culling and view["options"]["frozen"]
            saved_before = settings_path.read_bytes() if settings_path.exists() else None
            state = live_depth(agent, state, info)
            assert (settings_path.read_bytes() if settings_path.exists() else None) == saved_before
            completed(agent.call("view.set", document=info["document"], revision=info["revision"],
                                 options={"frozen": False, "instanceBatching": True}))
            state["values"]["contact"].update(enabled=True, debugMode=1)
            state = completed(agent.call("render.settings.set", revision=state["revision"], values=state["values"]))
            deadline = time.monotonic() + 15
            while not completed(agent.call("render.statistics"))["pipeline"]["contactShadows"]:
                assert time.monotonic() < deadline
                time.sleep(.02)
            screenshot = completed(agent.wait(agent.call("render.screenshot", path=str(output / f"Depth{index}.png"), overwrite=True)))
            assert int(screenshot["bytes"]) > 1000
            for depth_preview in (True, False):
                state["values"]["contact"].update(enabled=True, debugMode=2 if depth_preview else 0)
                state["values"]["shadows"]["debugMode"] = 0 if depth_preview else 2
                state = completed(agent.call("render.settings.set", revision=state["revision"], values=state["values"]))
                completed(agent.wait(agent.call("render.screenshot", path=str(output / f"Preview{index}-{depth_preview}.png"), overwrite=True)))
                assert int(completed(agent.call("render.statistics"))["device"]["validationErrors"]) == 0
            state["values"]["shadows"]["debugMode"] = 0
            state["values"]["contact"].update(enabled=False, debugMode=0)
            state["values"]["reversedZ"] = False
            state = completed(agent.call("render.settings.set", revision=state["revision"], values=state["values"]))
            assert not state["activeReversedZ"]
            completed(agent.call("render.settings.save", revision=state["revision"], path=str(settings_path)))
            before = settings_path.read_bytes()
            invalid = dict(state["values"], pipeline="missing")
            assert agent.call("render.settings.set", revision=state["revision"], values=invalid)["error"]["code"] == "invalid_arguments"
            assert completed(agent.call("render.settings.get"))["revision"] == state["revision"]
            assert settings_path.read_bytes() == before and not ready(agent)["dirty"]
            completed(agent.call("application.close.request"))
            app.finish()
        except Exception:
            try:
                app.process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                pass
            print("Editor exit:", app.process.poll(), app.path.read_text(encoding="utf-8", errors="replace"), flush=True)
            raise
        finally:
            if agent:
                agent.close()
            app.close()
    app = Application(editor, output, "contact-disabled", "", root.parent / "HyperionAssets",
                      frames=15000, extra=("--disable-plugin", "contact-shadows"))
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), False)
        ready(agent)
        state = completed(agent.call("render.settings.get"))
        state["values"]["contact"]["enabled"] = True
        assert agent.call("render.settings.set", revision=state["revision"], values=state["values"])["error"]["code"] == "unavailable"
        assert completed(agent.call("render.settings.get"))["revision"] == state["revision"]
        state["values"]["contact"]["enabled"] = False
        for reversed_z in (False, True):
            state["values"]["reversedZ"] = reversed_z
            state = completed(agent.call("render.settings.set", revision=state["revision"], values=state["values"]))
            assert state["activeReversedZ"] == reversed_z
            completed(agent.wait(agent.call("render.screenshot", path=str(output / f"Empty-{reversed_z}.png"), overwrite=True)))
            assert int(completed(agent.call("render.statistics"))["device"]["validationErrors"]) == 0
        completed(agent.call("application.close.request"))
        app.finish()
    finally:
        if agent:
            agent.close()
        app.close()


def disabled_saved_contact(cli, editor, root, output):
    settings_path = output / "ContactEnabled.json"
    settings = json.loads((output / "Settings.json").read_text(encoding="utf-8"))
    settings["fields"]["contact"]["fields"].update(enabled=True, debugMode=1)
    settings_path.write_text(json.dumps(settings), encoding="utf-8")
    app = Application(editor, output, "saved-contact-disabled", "/Game/Scenes/Showcase.hasset",
                      root.parent / "HyperionAssets", frames=0,
                      extra=("--render-settings", settings_path, "--disable-plugin", "contact-shadows"))
    agent = None
    try:
        agent = AttachedSession(cli, app.target(), False)
        ready(agent)
        state = completed(agent.call("render.settings.get"))
        original = dict(state["values"]["contact"])
        assert original["enabled"] and original["debugMode"] == 1, state
        state["values"]["vsync"] = not state["values"]["vsync"]
        state = completed(agent.call("render.settings.set", revision=state["revision"], values=state["values"]))
        assert state["values"]["contact"] == original
        stats = completed(agent.call("render.statistics"))
        assert not stats["pipeline"]["contactShadows"] and int(stats["device"]["validationErrors"]) == 0, stats
        completed(agent.call("render.settings.save", revision=state["revision"], path=str(settings_path)))
        assert json.loads(settings_path.read_text())["fields"]["contact"]["fields"]["enabled"]
        completed(agent.call("application.close.request"))
        app.finish()
    finally:
        if agent:
            agent.close()
        app.close()


def benchmarks(editor, root, output, warmup):
    results = []
    for batched in (False, True):
        path = output / f"Benchmark{warmup}-{batched}.csv"
        arguments = [str(editor), "--asset-root", str(root.parent / "HyperionAssets"), "--scene", "/Game/Scenes/Showcase.hasset",
                     "--hidden", "--benchmark", str(path), "--benchmark-warmup", str(warmup), "--benchmark-samples", "40",
                     "--benchmark-camera", "--layout", str(output / "BenchmarkLayout.ini"),
                     "--editor-preferences", str(output / "BenchmarkPreferences.ini")]
        if not batched:
            arguments.append("--no-instance-batching")
        result = subprocess.run(arguments, cwd=root, capture_output=True, text=True, timeout=100)
        (output / f"Benchmark{warmup}-{batched}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == 0, result.stdout + result.stderr
        with path.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        assert len(rows) == 40 and [int(row["frame"]) for row in rows] == list(range(warmup, warmup + 40))
        gpu_frames = [int(row["gpu_sample_frame"]) for row in rows]
        assert all(b == a + 1 for a, b in zip(gpu_frames, gpu_frames[1:]))
        for row in rows:
            assert int(row["visible_items"]) > 0 and int(row["failed_items"]) == int(row["shadow_failed"]) == 0
            assert math.isfinite(float(row["frame_ms"])) and float(row["frame_ms"]) > 0
            assert int(row["fallback_disabled"]) == (0 if batched else int(row["visible_items"]))
            assert float(row["total_gpu_pass_ms"]) > 0
        results.append(rows)
    for ordinary, batched in zip(*results):
        assert ordinary["visible_items"] == batched["visible_items"]
        assert int(batched["scene_draws"]) < int(ordinary["scene_draws"])


if __name__ == "__main__":
    cli, editor, root = [pathlib.Path(value).resolve() for value in sys.argv[1:]]
    output = root / "out" / "editor-render-acceptance"
    output.mkdir(parents=True, exist_ok=True)
    render_controls(cli, editor, root, output)
    disabled_saved_contact(cli, editor, root, output)
    for warmup in (0, 20):
        benchmarks(editor, root, output, warmup)
    print("Editor live depth switching, persistence, feature absence and deterministic benchmarks passed")
