"""Repeat ready Sponza Editor/Viewer workloads and retain samples and process memory evidence."""
import argparse
import csv
import ctypes
from ctypes import wintypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time


class ProcessMemory(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("faults", wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            "peak_working_set", "working_set", "peak_paged", "paged", "peak_nonpaged",
            "nonpaged", "pagefile", "peak_pagefile", "private_bytes")]


def memory_usage(process):
    counters = ProcessMemory()
    counters.cb = ctypes.sizeof(counters)
    query = ctypes.windll.psapi.GetProcessMemoryInfo
    query.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessMemory), wintypes.DWORD]
    query.restype = wintypes.BOOL
    if not query(wintypes.HANDLE(int(process._handle)), ctypes.byref(counters), counters.cb):
        return {}
    return {key: getattr(counters, key) for key in ("peak_working_set", "working_set", "private_bytes")}


def summarize(path, sample_count):
    with path.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != sample_count:
        raise RuntimeError(f"Incorrect sample coverage: {path}: {len(rows)} != {sample_count}")
    for row in rows:
        if int(row["visible_items"]) <= 0 or int(row["failed_items"]) != 0:
            raise RuntimeError(f"Unready or failed workload: {row}")
    result = {}
    for key in rows[0]:
        try:
            values = [float(row[key]) for row in rows]
        except ValueError:
            continue
        if not all(math.isfinite(value) for value in values):
            raise RuntimeError(f"Nonfinite measurement: {path}: {key}")
        ordered = sorted(values)
        result[key] = {
            "mean": statistics.mean(values), "min": min(values), "max": max(values),
            "p95": ordered[math.ceil(len(values) * .95) - 1],
            "p99": ordered[math.ceil(len(values) * .99) - 1],
        }
    return result


def run_case(args, build, host, moving, repetition):
    name = f"{build}-{host}-{'moving' if moving else 'static'}-{repetition}"
    binary_dir = Path(getattr(args, build)).resolve()
    executable = binary_dir / f"hyperion_{host}.exe"
    csv_path = args.output / f"{name}.csv"
    command = [str(executable), "--asset-root", str(args.asset_root), "--scene", args.scene, "--benchmark", str(csv_path), "--hidden"]
    if host == "editor":
        command += ["--benchmark-warmup", "120",
                    "--benchmark-samples", str(args.samples)]

        if args.editor_outliner == "collapsed":
            command.append("--benchmark-collapsed")
    else:
        command += ["--config", str(args.root / "experiments/Scene.json"), "--pipeline", "deferred",
                    "--no-vsync", "--benchmark-warmup", str(args.viewer_warmup),
                    "--frames", str(args.viewer_warmup + args.samples)]
    if moving:
        command.append("--benchmark-camera")
    started = time.monotonic()
    peak = {}
    with (args.output / f"{name}.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen(command, cwd=args.root, stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        while process.poll() is None:
            for key, value in memory_usage(process).items():
                peak[key] = max(peak.get(key, 0), value)
            if time.monotonic() - started > args.timeout:
                process.kill()
                process.wait()
                raise RuntimeError(f"Benchmark timeout: {name}")
            time.sleep(.05)
    log_text = (args.output / f"{name}.log").read_text(encoding="utf-8")
    if process.returncode or "validation errors: 0" not in log_text:
        raise RuntimeError(f"Benchmark failed: {name}\n{log_text[-3000:]}")
    return {"name": name, "command": command, "elapsed_seconds": time.monotonic() - started,
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "memory_bytes": peak, "statistics": summarize(csv_path, args.samples)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--debug", required=True)
    parser.add_argument("--release", required=True)
    parser.add_argument("--scene", default="/Game/Scenes/Sponza.hasset")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--samples", type=int, default=300)
    parser.add_argument("--viewer-warmup", type=int, default=6000)
    parser.add_argument("--timeout", type=int, default=240)
    parser.add_argument("--hosts", nargs="+", choices=("editor", "viewer"), default=["editor", "viewer"])
    parser.add_argument("--editor-outliner", choices=("expanded", "collapsed"), default="expanded")
    parser.add_argument("--asset-root", type=Path)
    args = parser.parse_args()
    args.root = args.root.resolve()
    args.asset_root = (args.asset_root or args.root.parent / "HyperionAssets").resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"platform": platform.platform(), "processor": platform.processor(),
              "cpu_count": os.cpu_count(), "samples": args.samples, "repetitions": args.repetitions,
              "memory_scope": "process lifetime; peak working set and sampled private bytes, not allocation totals",
              "runs": []}
    for build in ("release", "debug"):
        for host in args.hosts:
            for moving in (False, True):
                for repetition in range(1, args.repetitions + 1):
                    result = run_case(args, build, host, moving, repetition)
                    report["runs"].append(result)
                    (args.output / "Results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
                    print(f"{result['name']}: {result['statistics']['frame_ms']['mean']:.3f} ms", flush=True)


if __name__ == "__main__":
    main()
