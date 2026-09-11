"""Native bounded CPU frames: zero/mixed limits, skipped ticks, owned results and capture."""
import csv
import json
import math
import os
import pathlib
import re
import subprocess
import sys
from NativeContent import import_asset

viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "cpu-frame-acceptance"
work.mkdir(exist_ok=True)


def run(name, frames, a, b, *arguments, configured=False):
    limits = [] if configured else ["--main-render-lead", str(a), "--render-rhi-lead", str(b)]
    result = subprocess.run(
        [str(viewer), "--frames", str(frames), *limits, "--no-vsync", "--hidden", *map(str, arguments)],
        cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
    log = result.stdout + result.stderr
    (work / f"{name}.log").write_text(log, encoding="utf-8")
    assert result.returncode == 0, log
    assert "validation errors: 0" in log, log
    assert f"CPU frames: submitted={frames}; render={frames}; rhi={frames}; leads={a},{b}" in log, log
    return log


def capture_only():
    runtime = pathlib.Path(os.environ.get("HYP_RENDERDOC_RUNTIME", r"C:\Program Files\RenderDoc"))
    library = runtime / "renderdoc.dll"
    if not library.is_file():
        print("SKIP: RenderDoc runtime unavailable")
        raise SystemExit(77)
    output = work / "captures"
    log = run("Capture", 64, 2, 3, "--renderdoc-library", library, "--rdc-output", output,
              "--capture-rdc", "40", "--capture-rdc", "41")
    assert re.findall(r"RenderDoc target CPU frame: (\d+)", log) == ["40", "41"], log
    captures = re.findall(r"RenderDoc capture saved: (.+)", log)
    assert len(captures) == 2 and len(set(captures)) == 2, log
    assert all(pathlib.Path(path.strip()).stat().st_size > 0 for path in captures)
    print("Consecutive queued frames captured on their requested CPU ticks")


def check_rendering():
    images = []
    for a, b in ((0, 0), (1, 0), (0, 1), (1, 1), (2, 3)):
        image = work / f"Triangle-{a}-{b}.png"
        run(f"Triangle-{a}-{b}", 80, a, b, "--no-ui", "--capture", image, "--verify-triangle",
            configured=(a, b) == (1, 1))
        images.append(image.read_bytes())
    assert all(image == images[0] for image in images), "Async limits changed Triangle pixels"

    config = json.loads((root / "experiments/Triangle.json").read_text(encoding="utf-8"))
    config["properties"].update(rhi_threads=4, main_render_lead=2, render_rhi_lead=3)
    config_path = work / "Settings.json"
    config_path.write_text(json.dumps(config), encoding="utf-8")
    saved = work / "Saved.json"
    log = run("Window", 32, 2, 3, "--config", config_path, "--exercise-window", "--save-config", saved,
              "--capture", work / "Window.png", "--verify-triangle", "--verify-ui",
              "--benchmark", work / "Window.csv", "--benchmark-warmup", "16", configured=True)
    submitted = int(re.search(r"GPU frames: (\d+)", log).group(1))
    assert submitted < 32, "Minimized ticks unexpectedly drew GPU frames"
    with (work / "Window.csv").open(newline="", encoding="utf-8") as stream:
        samples = list(csv.DictReader(stream))
    assert len(samples) == 16
    assert all(int(row["gpu_sample_frame"]) == int(row["frame"]) + 1 - (32 - submitted) for row in samples)
    settings = json.loads(saved.read_text(encoding="utf-8"))["properties"]
    assert (settings["main_render_lead"], settings["render_rhi_lead"]) == (2, 3)
    assert (settings["width"], settings["height"]) == (960, 540)

    config["properties"].update(rhi_threads=1, plugins=[])
    config_path.write_text(json.dumps(config), encoding="utf-8")
    run("Empty", 32, 3, 2, "--config", config_path, "--capture", work / "Empty.png", "--verify-clear")


def scene_fixture():
    source = root / "assets/Scenes/Showcase.json"
    scene = json.loads(source.read_text(encoding="utf-8"))
    for asset in scene["assets"]:
        asset["path"] = str((source.parent / asset["path"]).resolve())
    # The stock ground meets model bottoms at the same depth. Publication/draw order
    # can change their shared edge pixels even with synchronous frames.
    for instance in scene["instances"]:
        if instance["asset"] == "ground":
            instance["translation"][1] -= .01
    path = work / "Scene.json"
    path.write_text(json.dumps(scene), encoding="utf-8")
    path = import_asset(viewer, path)
    config = json.loads((root / "experiments/Scene.json").read_text(encoding="utf-8"))
    config["properties"]["scene_source"] = str(path)
    config_path = work / "SceneSettings.json"
    config_path.write_text(json.dumps(config), encoding="utf-8")
    return config_path


def check_scene_results():
    config = scene_fixture()
    snapshots = []
    rows = []
    for a, b in ((0, 0), (2, 3)):
        path = work / f"Scene-{a}-{b}.csv"
        image = work / f"Scene-{a}-{b}.png"
        log = run(f"Scene-{a}-{b}", 220, a, b, "--config", config, "--no-ui",
                  "--benchmark", path, "--benchmark-warmup", "160", "--benchmark-camera",
                  "--capture", image, "--verify-model")
        assert "models ready | 0 failed" in log, log
        with path.open(newline="", encoding="utf-8") as stream:
            samples = list(csv.DictReader(stream))
        assert [int(row["frame"]) for row in samples] == list(range(160, 220)), samples
        for row in samples:
            assert int(row["gpu_sample_frame"]) == int(row["frame"]) + 1, row
            assert int(row["visible_items"]) > 0 and int(row["scene_draws"]) > 0, row
            assert int(row["failed_items"]) == 0 and int(row["shadow_failed"]) == 0, row
            assert int(row["shadows"]) == 1, row
            assert (int(row["main_render_lead"]), int(row["render_rhi_lead"])) == (a, b), row
            for key in ("frame_ms", "cpu_latency_ms"):
                assert math.isfinite(float(row[key])) and float(row[key]) > 0, row
        snapshots.append(image.read_bytes())
        rows.append(samples)
    assert snapshots[0] == snapshots[1], "Async frames changed moving-camera CSM pixels"
    for before, after in zip(*rows):
        for key in ("scene_draws", "visible_items", "shadow_items", "shadow_draws"):
            assert before[key] == after[key], (key, before, after)


def check_invalid_options():
    for option in ("--main-render-lead", "--render-rhi-lead"):
        for value in ("-1", "17", "1.5", "1x", "999999999999999999999"):
            result = subprocess.run([str(viewer), "--frames", "1", option, value], cwd=root,
                                    capture_output=True, text=True, timeout=10)
            assert result.returncode != 0 and "integer from 0 to 16" in result.stderr, result.stderr


if "--capture-only" in sys.argv[3:]:
    capture_only()
else:
    check_rendering()
    check_scene_results()
    check_invalid_options()
    print("Async CPU limits preserved pixels, skipped ticks, frame statistics and lifecycle")
