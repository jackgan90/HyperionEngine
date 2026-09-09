"""Real moving-camera A/B: equal pixels and coverage, bounded draws and cache work."""
import csv
import json
import math
import pathlib
import subprocess
import sys


viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "scene-performance"
work.mkdir(exist_ok=True)
model_count = len(json.loads((root / "assets/Scenes/Showcase.json").read_text(encoding="utf-8"))["instances"])


def run(name, batched):
    output = work / f"{name}.csv"
    capture = work / f"{name}.png"
    args = [str(viewer), "--config", str(root / "experiments/Scene.json"),
            "--frames", "180", "--benchmark-warmup", "80", "--benchmark", str(output),
            "--capture", str(capture), "--benchmark-camera", "--no-vsync", "--hidden", "--no-ui"]
    if not batched:
        args.append("--no-instance-batching")
    result = subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=60)
    log = result.stdout + result.stderr
    (work / f"{name}.log").write_text(log, encoding="utf-8")
    assert result.returncode == 0, log
    assert f"{model_count}/{model_count} models ready | 0 failed" in log and "validation errors: 0" in log, log
    assert "vsync=off" in log and "Benchmark: 100 frames" in log, log
    with output.open(newline="", encoding="utf-8") as stream:
        samples = list(csv.DictReader(stream))
    assert [int(row["frame"]) for row in samples] == list(range(80, 180))
    assert all(math.isfinite(float(row["frame_ms"])) and float(row["frame_ms"]) > 0 for row in samples)
    assert all(190 <= int(row["visible_items"]) <= 210 for row in samples), samples
    assert all(int(row["failed_items"]) == 0 for row in samples), samples
    assert all(int(row["instanced_items"]) + int(row["single_draws"]) == int(row["visible_items"]) for row in samples)
    reasons = ("disabled", "shader", "device", "ordering", "singleton", "preparation")
    assert all(int(row[f"fallback_{reason}"]) >= 0 for row in samples for reason in reasons)
    assert all(int(row["fallback_disabled"]) == (0 if batched else int(row["visible_items"])) for row in samples)
    return samples, capture.read_bytes()


ordinary, original = run("MovingOrdinary", False)
batched, compact = run("MovingInstance", True)
assert original == compact, "Instance batching changed the moving-camera image"
for before, after in zip(ordinary, batched):
    assert before["visible_items"] == after["visible_items"], (before, after)
    assert int(before["scene_draws"]) == int(before["visible_items"]), before
    assert 1 <= int(after["scene_draws"]) <= 10, after
    assert int(after["scene_draws"]) < int(before["scene_draws"]) / 10, after
assert sum(int(row["reused_chunks"]) for row in batched) > 400
assert sum(int(row["rebuilt_chunks"]) for row in batched) <= 20
for row in batched:
    assert int(row["instance_upload_bytes"]) <= int(row["rebuilt_chunks"]) * 128 * (144 + 96), row
invalid = subprocess.run([str(viewer), "--frames", "10", "--benchmark", str(work / "Invalid.csv")],
                         cwd=root, capture_output=True, text=True, timeout=10)
assert invalid.returncode != 0 and "greater than" in invalid.stdout + invalid.stderr
print("Moving-camera A/B: equal pixels and coverage, >90% fewer draws, bounded uploads and valid options")
