"""Serialized warmed CSM A/B measurements and sustained resource checks on a real GPU."""
import argparse
import csv
import json
import math
import pathlib
import statistics
import subprocess


def measure(viewer, root, work, name, motion, enabled, samples, warmup, resolution):
    output = work / f"{name}.csv"
    args = [str(viewer), "--config", str(root / "experiments/Scene.json"),
            "--hidden", "--no-ui", "--no-vsync", "--frames", str(samples + warmup),
            "--benchmark-warmup", str(warmup), "--benchmark", str(output),
            "--shadow-resolution", str(resolution)]
    args.extend(motion)
    if not enabled:
        args.append("--no-shadows")
    result = subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=180)
    log = result.stdout + result.stderr
    (work / f"{name}.log").write_text(log, encoding="utf-8")
    assert result.returncode == 0 and "validation errors: 0" in log, log
    model_count = len(json.loads((root / "assets/Scenes/Showcase.json").read_text())["instances"])
    assert f"{model_count}/{model_count} models ready | 0 failed" in log, log
    with output.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == samples
    cpu_ids = [int(row["frame"]) for row in rows]
    gpu_ids = [int(row["gpu_sample_frame"]) for row in rows]
    assert cpu_ids == list(range(warmup, warmup + samples))
    assert gpu_ids == [frame + 1 for frame in cpu_ids], "GPU submissions must cover the exact interval once"
    assert all(190 <= int(row["visible_items"]) <= 210 for row in rows)
    assert all(int(row["failed_items"]) == int(row["shadow_failed"]) == 0 for row in rows)
    assert all(int(row["shadows"]) == enabled for row in rows)
    if enabled:
        assert all(int(row["shadow_items"]) > 0 and int(row["shadow_draws"]) > 0 for row in rows)
        assert all(int(row["shadow_payload_bytes"]) == 16 * resolution ** 2 for row in rows)
        assert all(float(row[f"cascade{cascade}_gpu_ms"]) > 0 for row in rows for cascade in range(4))
    fields = ("frame_ms", "pipeline_prepare_ms", "shadow_setup_ms", "shadow_material_ms",
              "shadow_plan_ms", "shadow_prepare_ms", "shadow_gpu_ms", "forward_gpu_ms")
    summary = {}
    for field in fields:
        values = sorted(float(row[field]) for row in rows)
        assert all(math.isfinite(value) and value >= 0 for value in values)
        summary[field] = {"mean": statistics.mean(values), "p95": values[(len(values) - 1) * 95 // 100]}
    for field in ("gpu_allocation_bytes", "descriptor_allocations", "pipelines_created",
                  "shadow_draws", "shadow_items", "shadow_upload_bytes"):
        values = [int(row[field]) for row in rows]
        summary[field] = {"first": values[0], "last": values[-1], "min": min(values), "max": max(values)}
    assert summary["descriptor_allocations"]["min"] == summary["descriptor_allocations"]["max"]
    assert summary["pipelines_created"]["min"] == summary["pipelines_created"]["max"]
    assert summary["gpu_allocation_bytes"]["max"] - summary["gpu_allocation_bytes"]["min"] <= 16 * 1024 * 1024
    summary["samples"] = samples
    summary["gpu_submissions"] = {"count": len(set(gpu_ids)), "first": gpu_ids[0], "last": gpu_ids[-1]}
    summary["debug_layer"] = "debug layer: enabled" in log
    summary["adapter"] = next(line.split("D3D12 adapter: ", 1)[1] for line in log.splitlines() if "D3D12 adapter:" in line)
    print(f"{name}: CPU {summary['pipeline_prepare_ms']['mean']:.3f} ms, "
          f"shadow GPU {summary['shadow_gpu_ms']['mean']:.3f} ms", flush=True)
    return summary


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer", type=pathlib.Path, default=root / "out/build/release/bin/hyperion_viewer.exe")
    parser.add_argument("--output", type=pathlib.Path, default=root / "out/shadow-performance")
    parser.add_argument("--samples", type=int, default=800)
    parser.add_argument("--warmup", type=int, default=200)
    parser.add_argument("--repeats", type=int, default=2)
    parser.add_argument("--sustained", type=int, default=5000)
    parser.add_argument("--resolution", type=int, choices=(1024, 2048), default=2048)
    options = parser.parse_args()
    if min(options.samples, options.warmup, options.repeats, options.sustained) <= 0:
        parser.error("counts must be positive")
    options.output = options.output.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    motions = {"Static": [], "Camera": ["--benchmark-camera"], "Light": ["--benchmark-light"],
               "CameraLight": ["--benchmark-camera", "--benchmark-light"]}
    report = {"resolution": options.resolution, "warmup": options.warmup, "runs": {}, "comparisons": {}}
    for mode, motion in motions.items():
        for repeat in range(options.repeats):
            pair = {}
            # Reverse A/B order on alternate repetitions to reduce clock/warmup bias.
            for enabled in ([False, True] if repeat % 2 == 0 else [True, False]):
                name = f"{mode}{'On' if enabled else 'Off'}{repeat}"
                pair[enabled] = measure(options.viewer.resolve(), root, options.output, name, motion, enabled,
                                        options.samples, options.warmup, options.resolution)
                report["runs"][name] = pair[enabled]
            report["comparisons"][f"{mode}{repeat}"] = {
                "added_cpu_ms": pair[True]["pipeline_prepare_ms"]["mean"] - pair[False]["pipeline_prepare_ms"]["mean"],
                "added_forward_gpu_ms": pair[True]["forward_gpu_ms"]["mean"] - pair[False]["forward_gpu_ms"]["mean"]}
    report["runs"]["Sustained"] = measure(options.viewer.resolve(), root, options.output, "Sustained",
                                            motions["CameraLight"], True, options.sustained,
                                            options.warmup, options.resolution)
    report_path = options.output / "Summary.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"PASS: warmed nonempty scenes, validation, stable PSOs/descriptors and bounded GPU memory. {report_path}")


if __name__ == "__main__":
    main()
