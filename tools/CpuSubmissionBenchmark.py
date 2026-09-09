"""Serial, alternating binary A/B CPU submission benchmarks with strict workload checks."""
import argparse
import csv
import hashlib
import json
import math
import os
import pathlib
import re
import statistics
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]


def distribution(values):
    if not values or not all(math.isfinite(value) for value in values):
        raise ValueError("Missing or nonfinite benchmark samples")
    ordered = sorted(values)
    return {"mean": statistics.mean(values), "p50": statistics.median(values),
            "p95": ordered[(len(values) - 1) * 95 // 100],
            "p99": ordered[(len(values) - 1) * 99 // 100],
            "min": ordered[0], "max": ordered[-1]}


def validate(rows, log, warmup, samples, count, shadows):
    if "validation errors: 0" not in log:
        raise ValueError("Missing clean native validation result")
    if [int(row["frame"]) for row in rows] != list(range(warmup, warmup + samples)):
        raise ValueError("Incomplete CPU frame coverage")
    if count != 0:
        ready = re.findall(r"(\d+)/(\d+) models ready \| (\d+) failed", log)
        if not ready or any(int(a) != int(b) or int(c) for a, b, c in ready[-1:]):
            raise ValueError("Scene is not ready")
        if count is not None and int(ready[-1][0]) != count:
            raise ValueError("Incorrect model count")
    for row in rows:
        if int(row["gpu_sample_frame"]) != int(row["frame"]) + 1:
            raise ValueError("GPU submission coverage mismatch")
        if int(row["failed_items"]) or int(row["shadow_failed"]):
            raise ValueError("Failed source items")
        visible = int(row["visible_items"])
        if int(row["instanced_items"]) + int(row["single_draws"]) != visible:
            raise ValueError("Incomplete source item coverage")
        if count is not None and (visible != count or int(row["scene_draws"]) != count):
            raise ValueError("Incorrect ordinary draw workload")
        if count is None and (visible <= 0 or int(row["scene_draws"]) <= 0):
            raise ValueError("Empty scene workload")
        if bool(int(row["shadows"])) != shadows:
            raise ValueError("Incorrect shadow mode")
        if shadows and (int(row["shadow_items"]) <= 0 or int(row["shadow_draws"]) <= 0):
            raise ValueError("Empty shadow workload")
        if float(row["frame_ms"]) <= 0:
            raise ValueError("Invalid frame duration")
    for key in ("descriptor_allocations", "pipelines_created"):
        if len({row[key] for row in rows}) != 1:
            raise ValueError(f"Unstable warmed resource count: {key}")
    return {key: distribution([float(row[key]) for row in rows]) for key in rows[0]}


def generate(output, counts):
    config = json.loads((ROOT / "experiments/Scene.json").read_text(encoding="utf-8"))
    config["properties"]["plugins"] = []
    config["properties"].pop("scene_source", None)
    (output / "Clear.json").write_text(json.dumps(config, indent=2), encoding="utf-8")
    for count in counts:
        if count == 0:
            continue
        scene = {"type": "hyperion.scene", "schema_version": 1,
                 "assets": [{"id": "triangle", "path": os.path.relpath(
                     ROOT / "assets/Models/Interleaved.gltf", output)}],
                 "instances": [{"id": f"triangle-{i}", "asset": "triangle",
                                "scale": [.05, .05, .05]} for i in range(count)],
                 "camera": {"eye": [0, 0, 10], "target": [0, 0, 0], "near": .05, "far": 100}}
        (output / f"Triangles-{count}.json").write_text(json.dumps(scene), encoding="utf-8")


def run(args, label, viewer, trial, count, moving, shadows):
    name = f"{label}-{'scene' if count is None else count}-{'moving' if moving else 'static'}"
    name += f"-{'csm' if shadows else 'forward'}-R{trial}"
    csv_path = args.output / f"{name}.csv"
    config = args.output / "Clear.json" if count == 0 else ROOT / "experiments/Scene.json"
    command = [str(viewer), "--config", str(config), "--no-vsync", "--frames",
               str(args.warmup + args.samples), "--benchmark-warmup", str(args.warmup),
               "--benchmark", str(csv_path)]
    if not args.visible:
        command.append("--hidden")
    if not args.ui:
        command.append("--no-ui")
    if count:
        command += ["--scene", str(args.output / f"Triangles-{count}.json"),
                    "--scene-culling", "none", "--no-instance-batching"]
    if moving:
        command += ["--benchmark-camera", "--benchmark-camera-step", str(getattr(args, "camera_step", .1))]
    if getattr(args, "fixed_visibility", False):
        command += ["--scene-culling", "none"]
    if not shadows:
        command.append("--no-shadows")
    startup = None
    if os.name == "nt" and not args.visible:
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    try:
        completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                   timeout=args.timeout, startupinfo=startup)
    except subprocess.TimeoutExpired as error:
        def text(value):
            return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else value or ""
        (args.output / f"{name}.log").write_text(text(error.stdout) + text(error.stderr), encoding="utf-8")
        raise RuntimeError(f"Benchmark timed out: {name}; see log") from error
    log = completed.stdout + completed.stderr
    (args.output / f"{name}.log").write_text(log, encoding="utf-8")
    if completed.returncode:
        raise RuntimeError(f"Benchmark failed: {name}; see log")
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    metrics = validate(rows, log, args.warmup, args.samples, count, shadows)
    print(f"{name}: frame={metrics['frame_ms']['mean']:.3f} ms "
          f"P95={metrics['frame_ms']['p95']:.3f} ms "
          f"pipeline={metrics['pipeline_prepare_ms']['mean']:.3f} ms", flush=True)
    return {"name": name, "command": command, "metrics": metrics, "validation_errors": 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer", type=pathlib.Path, required=True)
    parser.add_argument("--baseline", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--counts", type=int, nargs="*", default=[0, 1, 100, 300, 600, 1200])
    parser.add_argument("--motion", choices=["static", "moving", "both"], default="both")
    parser.add_argument("--scene", action="store_true", help="Also measure default scene with and without CSM")
    parser.add_argument("--warmup", type=int, default=200)
    parser.add_argument("--samples", type=int, default=400)
    parser.add_argument("--trials", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=240)
    parser.add_argument("--camera-step", type=float, default=.1, help="Input-driven orbit step in pixels")
    parser.add_argument("--fixed-visibility", action="store_true", help="Disable culling for scene workloads")
    parser.add_argument("--scene-shadows", choices=["both", "on", "off"], default="both")
    parser.add_argument("--visible", action="store_true")
    parser.add_argument("--ui", action="store_true")
    args = parser.parse_args()
    if min(args.warmup, args.samples, args.trials) < 1 or any(count < 0 for count in args.counts):
        parser.error("Positive warmup/samples/trials and nonnegative counts are required")
    if not math.isfinite(args.camera_step) or args.camera_step <= 0:
        parser.error("camera-step must be finite and positive")
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    generate(args.output, args.counts)
    binaries = [("candidate", args.viewer.resolve())]
    if args.baseline:
        binaries.insert(0, ("baseline", args.baseline.resolve()))
    metadata = {"settings": {key: str(value) if isinstance(value, pathlib.Path) else value
                             for key, value in vars(args).items()},
                "binaries": {label: {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                             for label, path in binaries}, "runs": []}
    for label, path in binaries:
        cache = path.parent.parent / "CMakeCache.txt"
        if cache.is_file():
            (args.output / f"{label}-CMakeCache.txt").write_bytes(cache.read_bytes())
    workloads = [(count, False) for count in args.counts]
    if args.scene:
        workloads += [(None, shadows) for shadows in ([False, True] if args.scene_shadows == "both"
                                                     else [args.scene_shadows == "on"])]
    try:
        for count, shadows in workloads:
            for moving in ([False, True] if args.motion == "both" else [args.motion == "moving"]):
                if count == 0 and moving:
                    continue
                for trial in range(args.trials):
                    for label, viewer in (binaries if trial % 2 == 0 else list(reversed(binaries))):
                        metadata["runs"].append(run(args, label, viewer, trial, count, moving, shadows))
    finally:
        (args.output / "Summary.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
