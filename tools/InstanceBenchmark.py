"""Serial, interleaved same-executable instance A/B measurements with raw evidence."""
import argparse
import csv
import hashlib
import json
import math
import pathlib
import statistics
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]


def summarize(rows):
    times = [float(row["frame_ms"]) for row in rows]
    assert times and all(math.isfinite(value) and value > 0 for value in times)
    ordered = sorted(times)
    result = {"samples": len(rows), "mean_ms": statistics.mean(times),
              "median_ms": statistics.median(times), "p95_ms": ordered[(len(times) - 1) * 95 // 100]}
    for key in ("scene_draws", "visible_items", "instanced_items", "reused_chunks", "rebuilt_chunks",
                "packed_bytes", "instance_upload_bytes", "gpu_reuses", "plan_ms", "prepare_ms"):
        values = [float(row[key]) for row in rows]
        result[key] = {"mean": statistics.mean(values), "min": min(values), "max": max(values)}
    assert all(int(row["failed_items"]) == 0 for row in rows)
    assert all(int(row["instanced_items"]) + int(row["single_draws"]) == int(row["visible_items"]) for row in rows)
    return result


def run(args, motion, trial, enabled):
    name = f"{motion}-{trial}-{'on' if enabled else 'off'}"
    path = args.output / f"{name}.csv"
    command = [str(args.viewer), "--config", str(ROOT / "experiments/Scene.json"), "--hidden", "--no-vsync",
               "--frames", str(args.warmup + args.frames), "--benchmark-warmup", str(args.warmup),
               "--benchmark", str(path)]
    if motion == "moving":
        command.append("--benchmark-camera")
    if not enabled:
        command.append("--no-instance-batching")
    if args.no_ui:
        command.append("--no-ui")
    completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    log = completed.stdout + completed.stderr
    (args.output / f"{name}.log").write_text(log, encoding="utf-8")
    if completed.returncode or "validation errors: 0" not in log or "78/78 models ready | 0 failed" not in log:
        raise RuntimeError(f"Invalid workload: {name}; see log")
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert [int(row["frame"]) for row in rows] == list(range(args.warmup, args.warmup + args.frames))
    result = {"name": name, "motion": motion, "trial": trial, "enabled": enabled,
              "command": command, **summarize(rows)}
    print(f"{name}: {result['mean_ms']:.4f} ms, plan {result['plan_ms']['mean']:.4f} ms, "
          f"prep {result['prepare_ms']['mean']:.4f} ms, draws {result['scene_draws']['mean']:.2f}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--warmup", type=int, default=240)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--no-ui", action="store_true")
    args = parser.parse_args()
    if min(args.warmup, args.frames, args.trials) < 1:
        parser.error("warmup, frames and trials must be positive")
    args.viewer = args.viewer.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    results = []
    metadata = {"viewer": str(args.viewer), "sha256": hashlib.sha256(args.viewer.read_bytes()).hexdigest(),
                "warmup": args.warmup, "frames": args.frames, "trials": args.trials, "ui": not args.no_ui,
                "config": json.loads((ROOT / "experiments/Scene.json").read_text()), "runs": results}
    cache = args.viewer.parent.parent / "CMakeCache.txt"
    if cache.is_file():
        (args.output / "CMakeCache.txt").write_text(cache.read_text(), encoding="utf-8")
    try:
        for motion in ("static", "moving"):
            for trial in range(1, args.trials + 1):
                for enabled in ((False, True) if trial % 2 else (True, False)):
                    results.append(run(args, motion, trial, enabled))
    finally:
        (args.output / "Summary.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
