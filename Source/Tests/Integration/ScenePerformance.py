"""Exercise the real Viewer camera and verify bounded, uncapped benchmark output."""
import csv
import math
import pathlib
import subprocess
import sys


viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "scene-performance"
work.mkdir(exist_ok=True)
output = work / "Moving.csv"
args = [str(viewer), "--config", str(root / "experiments/Scene.json"),
        "--frames", "180", "--benchmark-warmup", "80", "--benchmark", str(output),
        "--benchmark-camera", "--no-vsync", "--hidden"]
result = subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=60)
log = result.stdout + result.stderr
(work / "Moving.log").write_text(log, encoding="utf-8")
assert result.returncode == 0, log
assert "78/78 models ready | 0 failed" in log and "validation errors: 0" in log, log
assert "vsync=off" in log and "Benchmark: 100 frames" in log, log
with output.open(newline="", encoding="utf-8") as stream:
    samples = list(csv.DictReader(stream))
assert [int(row["frame"]) for row in samples] == list(range(80, 180))
assert all(math.isfinite(float(row["frame_ms"])) and float(row["frame_ms"]) > 0 for row in samples)
assert all(190 <= int(row["scene_draws"]) <= 210 for row in samples), samples
invalid = subprocess.run([str(viewer), "--frames", "10", "--benchmark", str(output)],
                         cwd=root, capture_output=True, text=True, timeout=10)
assert invalid.returncode != 0 and "greater than" in invalid.stdout + invalid.stderr
print("Moving-camera benchmark: 100 populated frames, warmup excluded, VSync override and invalid options passed")
