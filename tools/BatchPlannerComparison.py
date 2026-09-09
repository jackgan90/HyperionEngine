"""Interleave frozen baseline/candidate planner or Viewer runs and validate equivalent work."""
import argparse
import csv
import hashlib
import io
import json
import math
import pathlib
import statistics
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def planner(args, executable, name):
    command = [str(executable), str(args.samples)]
    result = subprocess.run(command, cwd=args.output, capture_output=True, text=True, timeout=300)
    log = result.stdout + result.stderr
    (args.output / f"{name}.log").write_text(log, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Planner workload failed: {name}")
    start = result.stdout.index("workload,warmup,")
    content = result.stdout[start:]
    (args.output / f"{name}.csv").write_text(content, encoding="utf-8")
    rows = list(csv.DictReader(io.StringIO(content)))
    assert len(rows) == 8 and len({row["workload"] for row in rows}) == 8
    for row in rows:
        assert int(row["samples"]) == args.samples and int(row["warmup"]) == 80
        assert int(row["visible"]) == 1024 and int(row["draws"]) > 0
        assert math.isfinite(float(row["mean_ms"])) and float(row["mean_ms"]) > 0
    return {"name": name, "command": command, "rows": rows}


def viewer(args, executable, name, motion):
    output = args.output / f"{name}.csv"
    capture = args.output / f"{name}.png"
    command = [str(executable), "--config", str(ROOT / "experiments/Scene.json"), "--hidden", "--no-ui",
               "--no-vsync", "--frames", str(args.warmup + args.samples), "--benchmark-warmup", str(args.warmup),
               "--benchmark", str(output), "--capture", str(capture)]
    if motion != "stable":
        command += ["--benchmark-camera", "--benchmark-camera-step", "0.1" if motion == "small" else "10"]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    log = result.stdout + result.stderr
    (args.output / f"{name}.log").write_text(log, encoding="utf-8")
    count = len(json.loads((ROOT / "assets/Scenes/Showcase.json").read_text(encoding="utf-8"))["instances"])
    if result.returncode or "validation errors: 0" not in log or f"{count}/{count} models ready | 0 failed" not in log:
        raise RuntimeError(f"Viewer readiness or validation failed: {name}")
    assert "vsync=off" in log
    rows = list(csv.DictReader(io.StringIO(output.read_text(encoding="utf-8"))))
    assert [int(row["frame"]) for row in rows] == list(range(args.warmup, args.warmup + args.samples))
    for row in rows:
        assert int(row["visible_items"]) > 0 and int(row["failed_items"]) == 0 and int(row["shadow_failed"]) == 0
        assert int(row["instanced_items"]) + int(row["single_draws"]) == int(row["visible_items"])
        assert math.isfinite(float(row["frame_ms"])) and float(row["frame_ms"]) > 0
    metrics = {}
    for key in rows[0]:
        values = sorted(float(row[key]) for row in rows)
        metrics[key] = {"mean": statistics.mean(values), "median": statistics.median(values),
                        "p95": values[(len(values) - 1) * 95 // 100]}
    return {"name": name, "motion": motion, "command": command, "metrics": metrics,
            "image_sha256": digest(capture), "rows": rows}


def compare(args, before, after):
    assert len(before["rows"]) == len(after["rows"])
    if args.kind == "planner":
        keys = ("workload", "warmup", "samples", "visible", "draws")
        metrics = {left["workload"]: {"before_ms": float(left["mean_ms"]), "after_ms": float(right["mean_ms"])}
                   for left, right in zip(before["rows"], after["rows"])}
    else:
        keys = ("frame", "scene_draws", "visible_items", "instanced_items", "single_draws", "shadow_items", "shadow_draws")
        assert before["image_sha256"] == after["image_sha256"], "Rendered image changed"
        metrics = {key: {"before_ms": before["metrics"][key]["mean"], "after_ms": after["metrics"][key]["mean"]}
                   for key in ("frame_ms", "pipeline_prepare_ms", "plan_ms", "shadow_plan_ms", "prepare_ms")}
    for left, right in zip(before["rows"], after["rows"]):
        assert all(left[key] == right[key] for key in keys), (left, right)
    for metric in metrics.values():
        metric["change_percent"] = (metric["after_ms"] / metric["before_ms"] - 1) * 100 if metric["before_ms"] else 0
    return {"baseline": before["name"], "candidate": after["name"], "metrics": metrics}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=pathlib.Path, required=True)
    parser.add_argument("--candidate", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--kind", choices=("planner", "viewer"), default="planner")
    parser.add_argument("--warmup", type=int, default=200, help="Viewer warmup; planner uses its fixed 80 frames")
    parser.add_argument("--samples", type=int, default=200)
    parser.add_argument("--trials", type=int, default=2)
    args = parser.parse_args()
    if min(args.warmup, args.samples, args.trials) < 1:
        parser.error("warmup, samples and trials must be positive")
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    args.baseline = args.baseline.resolve()
    args.candidate = args.candidate.resolve()
    metadata = {"kind": args.kind, "binaries": {name: {"path": str(path), "sha256": digest(path)}
                for name, path in (("baseline", args.baseline), ("candidate", args.candidate))}, "runs": [], "pairs": []}
    motions = ("planner",) if args.kind == "planner" else ("stable", "small", "large")
    try:
        for motion in motions:
            for trial in range(args.trials):
                runs = {}
                order = ("baseline", "candidate") if trial % 2 == 0 else ("candidate", "baseline")
                for version in order:
                    name = f"{motion}-{version}-{trial}"
                    executable = getattr(args, version)
                    run = planner(args, executable, name) if args.kind == "planner" else viewer(args, executable, name, motion)
                    runs[version] = run
                    metadata["runs"].append({key: value for key, value in run.items() if key != "rows" or args.kind == "planner"})
                    print(f"Completed {name}", flush=True)
                pair = compare(args, runs["baseline"], runs["candidate"])
                metadata["pairs"].append(pair)
                print(json.dumps(pair["metrics"]), flush=True)
    finally:
        (args.output / "Summary.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
