"""Measure matched HDR Forward/Deferred workloads, including readiness and submission validation."""
import argparse
import csv
import datetime
import hashlib
import json
import math
import pathlib
import statistics
import subprocess


TIMINGS = ("frame_ms", "cpu_latency_ms", "pipeline_prepare_ms", "fullscreen_prepare_ms",
           "plan_ms", "prepare_ms", "material_ms", "shadow_gpu_ms", "forward_gpu_ms",
           "base_gpu_ms", "lighting_gpu_ms", "compatibility_gpu_ms", "transparent_gpu_ms",
           "tonemap_gpu_ms", "total_gpu_pass_ms")
COUNTERS = ("visible_items", "scene_draws", "instanced_items", "instanced_draws",
            "gpu_allocation_bytes", "descriptor_allocations", "pipelines_created",
            "scene_target_bytes", "fullscreen_draws", "shadow_items", "shadow_draws",
            "instance_upload_bytes", "shadow_upload_bytes", "native_list_resets", "constant_bytes_written",
            "pipeline_binds", "geometry_binds", "dynamic_binds", "native_lists_created")


def run_case(viewer, root, output, scene, size, moving, shadows, pipeline, layout,
             repeat, samples, warmup, main_lead=1, render_lead=1):
    width, height = size
    name = f"{scene}-{width}x{height}-{'moving' if moving else 'static'}-csm{int(shadows)}-{pipeline}-{layout}-{repeat}"
    config = json.loads((root / "experiments" / f"{scene}.json").read_text(encoding="utf-8"))
    config["properties"].update(width=width, height=height)
    config_path = output / f"{name}.json"
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    csv_path = output / f"{name}.csv"
    args = [str(viewer), "--config", str(config_path), "--hidden", "--no-ui", "--no-vsync",
            "--frames", str(samples + warmup), "--benchmark-warmup", str(warmup),
            "--benchmark", str(csv_path), "--pipeline", pipeline, "--gbuffer", layout,
            "--shadow-resolution", "2048", "--main-render-lead", str(main_lead),
            "--render-rhi-lead", str(render_lead)]
    if moving:
        args.append("--benchmark-camera")
    if not shadows:
        args.append("--no-shadows")
    result = subprocess.run(args, cwd=root, capture_output=True, text=True, timeout=240)
    log = result.stdout + result.stderr
    (output / f"{name}.log").write_text(log, encoding="utf-8")
    if result.returncode or "validation errors: 0" not in log or "Rendering lifecycle completed successfully" not in log:
        raise RuntimeError(f"{name}: {log}")
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == samples, name
    assert [int(row["frame"]) for row in rows] == list(range(warmup, warmup + samples)), name
    assert [int(row["gpu_sample_frame"]) for row in rows] == list(range(warmup + 1, warmup + samples + 1)), name
    assert all(int(row["failed_items"]) == int(row["shadow_failed"]) == 0 for row in rows), name
    assert all(int(row["shadows"]) == int(shadows) for row in rows), name
    assert all(int(row["main_render_lead"]) == main_lead and int(row["render_rhi_lead"]) == render_lead
               for row in rows), name
    minimum, maximum = (190, 210) if scene == "Scene" else (4, 4)
    assert all(minimum <= int(row["visible_items"]) <= maximum and int(row["scene_draws"]) > 0 for row in rows), name
    expected_bytes = width * height * (12 if pipeline == "forward" else 44 if layout == "high" else 36)
    assert all(int(row["scene_target_bytes"]) == expected_bytes for row in rows), name
    assert all(float(row["tonemap_gpu_ms"]) > 0 for row in rows), name
    if pipeline == "deferred":
        assert all(float(row["base_gpu_ms"]) > 0 and float(row["lighting_gpu_ms"]) > 0 for row in rows), name
    else:
        assert all(float(row["forward_gpu_ms"]) > 0 and float(row["lighting_gpu_ms"]) == 0 for row in rows), name
    if shadows:
        assert all(int(row["shadow_draws"]) > 0 and float(row["shadow_gpu_ms"]) > 0 for row in rows), name
    summary = {}
    for field in TIMINGS:
        values = sorted(float(row[field]) for row in rows)
        assert all(math.isfinite(value) and value >= 0 for value in values), (name, field)
        summary[field] = {"mean": statistics.mean(values), "median": statistics.median(values), "p95": values[(len(values) - 1) * 95 // 100]}
    for field in COUNTERS:
        values = [int(row[field]) for row in rows]
        summary[field] = {"first": values[0], "last": values[-1], "min": min(values), "max": max(values)}
    for field in ("descriptor_allocations", "pipelines_created"):
        assert summary[field]["min"] == summary[field]["max"], (name, field, summary[field])
    assert summary["gpu_allocation_bytes"]["max"] - summary["gpu_allocation_bytes"]["min"] <= 16 * 1024 * 1024, name
    workload = [[int(row[field]) for field in ("visible_items", "scene_draws", "instanced_items",
                 "instanced_draws", "shadow_items", "shadow_draws")] for row in rows]
    summary["workload_sha256"] = hashlib.sha256(json.dumps(workload).encode()).hexdigest()
    summary.update(scene=scene, width=width, height=height, moving=moving, shadows=shadows,
                   pipeline=pipeline, layout=layout, repeat=repeat, samples=samples, warmup=warmup,
                   main_render_lead=main_lead, render_rhi_lead=render_lead,
                   debug_layer="debug layer: enabled" in log, vsync=False, command=args,
                   adapter=next(line.split("D3D12 adapter: ", 1)[1] for line in log.splitlines() if "D3D12 adapter:" in line))
    print(f"{name}: GPU passes {summary['total_gpu_pass_ms']['mean']:.4f} ms; "
          f"prepare {summary['pipeline_prepare_ms']['mean']:.4f} ms", flush=True)
    return name, summary


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer", type=pathlib.Path, default=root / "out/build/release/bin/hyperion_viewer.exe")
    parser.add_argument("--output", type=pathlib.Path, default=root / "out/deferred-performance")
    parser.add_argument("--samples", type=int, default=500)
    parser.add_argument("--warmup", type=int, default=300)
    parser.add_argument("--repeats", type=int, default=2)
    parser.add_argument("--main-render-lead", type=int, choices=range(17), default=1)
    parser.add_argument("--render-rhi-lead", type=int, choices=range(17), default=1)
    parser.add_argument("--sizes", nargs="+", default=["1280x720", "1920x1080"])
    parser.add_argument("--scenes", nargs="+", choices=["Scene", "Model"], default=["Scene", "Model"])
    options = parser.parse_args()
    if min(options.samples, options.warmup, options.repeats) <= 0:
        parser.error("sample, warmup and repeat counts must be positive")
    sizes = [tuple(map(int, value.split("x"))) for value in options.sizes]
    if any(len(size) != 2 or min(size) < 64 for size in sizes):
        parser.error("sizes must be WIDTHxHEIGHT with positive dimensions >=64")
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    viewer = options.viewer.resolve()
    report = {"time_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "binary": str(viewer), "binary_sha256": hashlib.sha256(viewer.read_bytes()).hexdigest(),
              "runs": {}, "comparisons": {}}
    for scene in options.scenes:
        for size in sizes:
            for moving in (False, True):
                for shadows in (False, True):
                    for repeat in range(options.repeats):
                        modes = [("forward", "compact"), ("deferred", "compact"), ("deferred", "high")]
                        if repeat % 2:
                            modes.reverse()
                        group = {}
                        for pipeline, layout in modes:
                            name, summary = run_case(viewer, root, output, scene, size, moving, shadows,
                                                     pipeline, layout, repeat, options.samples, options.warmup,
                                                     options.main_render_lead, options.render_rhi_lead)
                            report["runs"][name] = summary
                            group[pipeline, layout] = summary
                            (output / "Summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
                        assert len({entry["workload_sha256"] for entry in group.values()}) == 1, \
                            "Forward/Deferred per-frame visibility, draws or shadow workload differ"
                        for layout in ("compact", "high"):
                            forward = group["forward", "compact"]
                            deferred = group["deferred", layout]
                            key = f"{scene}-{size[0]}x{size[1]}-moving{int(moving)}-csm{int(shadows)}-{layout}-{repeat}"
                            report["comparisons"][key] = {
                                field: deferred[field]["mean"] - forward[field]["mean"]
                                for field in ("total_gpu_pass_ms", "pipeline_prepare_ms", "frame_ms", "cpu_latency_ms")}
                        (output / "Summary.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"PASS: all warmed workloads, exact GPU submissions, zero validation errors and bounded resources. {output / 'Summary.json'}")


if __name__ == "__main__":
    main()
