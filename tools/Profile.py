"""Build, capture and export a bounded Viewer workload using the locked Tracy source."""
import argparse
import csv
import json
import math
import os
import pathlib
import socket
import statistics
import subprocess
import time


ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = ROOT / "out/build/profiling-tools"


def hidden_process_options():
    options = {}
    if hasattr(subprocess, "STARTUPINFO"):
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        options = {"startupinfo": startup, "creationflags": subprocess.CREATE_NO_WINDOW}
    return options


def run(command, output, timeout=300, cwd=ROOT):
    with pathlib.Path(output).open("w", encoding="utf-8") as stream:
        subprocess.run([str(value) for value in command], cwd=cwd, stdout=stream,
                       stderr=subprocess.STDOUT, check=True, timeout=timeout,
                       **hidden_process_options())


def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def available_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def check_capture(output):
    text = (output / "Collector.log").read_text(encoding="utf-8", errors="replace")
    trace = output / "Capture.tracy"
    # The pinned vendor capture returns zero even after an instrumentation or output error.
    if "Instrumentation failure:" in text or " done!" not in text or not trace.is_file() or trace.stat().st_size == 0:
        raise RuntimeError("Invalid or incomplete Tracy capture; see Collector.log")


def capture(command, output, timeout, seconds=None, cwd=ROOT):
    """Start only our collector/client, preserve logs on every failure, then join both."""
    collector = None
    client = None
    port = available_port()
    collector_args = [str(TOOLS / "tracy-capture.exe"), "-o", str(output / "Capture.tracy"),
                      "-a", "127.0.0.1", "-p", str(port)]
    if seconds is not None:
        collector_args += ["-s", str(seconds)]
    with (output / "Collector.log").open("w", encoding="utf-8") as collector_log, \
            (output / "Viewer.log").open("w", encoding="utf-8") as client_log:
        try:
            collector = subprocess.Popen(collector_args, cwd=ROOT, stdout=collector_log,
                                         stderr=subprocess.STDOUT, **hidden_process_options())
            client = subprocess.Popen(command, cwd=cwd, stdout=client_log,
                                      stderr=subprocess.STDOUT, env={**os.environ, "TRACY_PORT": str(port)},
                                      **hidden_process_options())
            deadline = time.monotonic() + timeout
            while client.poll() is None:
                # Tracy may finish its trace while the client is still joining shutdown threads.
                # A successful collector exit is valid; keep the application bounded and inspect the trace.
                if collector.poll() not in (None, 0):
                    raise RuntimeError(f"Collector failed ({collector.returncode})")
                if time.monotonic() > deadline:
                    raise TimeoutError("Viewer capture exceeded its time limit")
                time.sleep(0.05)
            if client.returncode:
                raise RuntimeError(f"Viewer failed ({client.returncode}); see Viewer.log")
            if collector.wait(timeout=max(1, deadline - time.monotonic())):
                raise RuntimeError("Tracy capture failed; see Collector.log")
        finally:
            stop(client)
            stop(collector)
    check_capture(output)
    return port


def export(output):
    exporter = TOOLS / "tracy-csvexport.exe"
    trace = output / "Capture.tracy"
    for name, switches in (("Zones.csv", []), ("Events.csv", ["-u"]),
                           ("EventsAndPlots.csv", ["-u", "-p"]), ("Gpu.csv", ["-g"])):
        # Keep CSV stdout separate from progress/errors.
        with (output / name).open("w", encoding="utf-8") as stream, \
                (output / (name + ".log")).open("w", encoding="utf-8") as log:
            subprocess.run([str(exporter), *switches, str(trace)], stdout=stream, stderr=log,
                           check=True, timeout=120, **hidden_process_options())


def frame_statistics(path):
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    times = [float(row["frame_ms"]) for row in rows]
    draws = [int(row["scene_draws"]) for row in rows]
    if not times or not all(math.isfinite(value) and value > 0 for value in times):
        raise RuntimeError("Missing or invalid frame timing samples")
    ordered = sorted(times)
    return {"samples": len(times), "mean_ms": statistics.mean(times),
            "median_ms": statistics.median(times), "p95_ms": ordered[(len(times) - 1) * 95 // 100],
            "draw_min": min(draws), "draw_max": max(draws)}


def check_frame_window(output, warmup, count):
    with (output / "Events.csv").open(newline="", encoding="utf-8") as stream:
        frames = [int(row["value"].split()[0]) for row in csv.DictReader(stream)
                  if row["name"] == "ApplicationFrame" and row["src_file"].endswith("ViewerFrame.cpp")]
    if sorted(frames) != list(range(warmup, warmup + count)):
        raise RuntimeError("Trace does not contain the complete requested frame window")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("off", "basic", "detail", "gpu", "detail-gpu", "sampling"),
                        default="basic")
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--viewer", type=pathlib.Path)
    parser.add_argument("--config", type=pathlib.Path, default=ROOT / "experiments/Scene.json")
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--static", action="store_true")
    parser.add_argument("--no-instance-batching", action="store_true")
    parser.add_argument("--visible", action="store_true")
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args()
    if args.warmup < 0 or args.frames <= 0 or args.timeout <= 0:
        parser.error("warmup must be nonnegative; frames and timeout must be positive")
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    if not args.no_build:
        if args.viewer:
            parser.error("--viewer requires --no-build to preserve the chosen build")
        run(["powershell", "-NoProfile", "-File", ROOT / "tools/Build.ps1", "-Preset", "profile",
             "-Target", "hyperion_viewer"], output / "Build.log", timeout=900)
        if args.mode != "off":
            run(["powershell", "-NoProfile", "-File", ROOT / "tools/BuildProfilingTools.ps1"],
                output / "ToolsBuild.log", timeout=900)
    viewer = (args.viewer or ROOT / "out/build/profile/bin/hyperion_viewer.exe").resolve()
    command = [str(viewer), "--config", str(args.config.resolve()), "--frames", str(args.warmup + args.frames),
               "--benchmark-warmup", str(args.warmup), "--benchmark", str(output / "Frames.csv"), "--no-vsync"]
    if not args.static:
        command.append("--benchmark-camera")
    if args.no_instance_batching:
        command.append("--no-instance-batching")
    if not args.visible:
        command.append("--hidden")
    if args.mode != "off":
        command += ["--profile", "--profile-wait", "--profile-start", str(args.warmup),
                    "--profile-frames", str(args.frames)]
        for token in args.mode.split("-"):
            if token in ("detail", "gpu", "sampling"):
                command.append("--profile-" + token)
    revision = subprocess.run(["git", "-c", f"safe.directory={ROOT.as_posix()}", "rev-parse", "HEAD"],
                              cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()
    metadata = {"revision": revision, "mode": args.mode, "command": command,
                "warmup": args.warmup, "frames": args.frames,
                "tracy": json.loads((ROOT / "dependencies.lock.json").read_text())["tracy"],
                "status": "running"}
    run(["git", "-c", f"safe.directory={ROOT.as_posix()}", "status", "--short"], output / "Workspace.log")
    build_dir = viewer.parent.parent
    if not (build_dir / "CMakeCache.txt").is_file():
        build_dir = build_dir.parent
    for source in [build_dir / "CMakeCache.txt", *build_dir.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")]:
        if source.is_file():
            (output / source.name).write_text(source.read_text(encoding="utf-8"), encoding="utf-8")
    try:
        if args.mode == "off":
            run(command, output / "Viewer.log", args.timeout)
        else:
            metadata["collector_port"] = capture(command, output, args.timeout)
            export(output)
            check_frame_window(output, args.warmup, args.frames)
        metadata["summary"] = frame_statistics(output / "Frames.csv")
        if metadata["summary"]["samples"] != args.frames:
            raise RuntimeError("Viewer produced an unexpected number of samples")
        metadata["status"] = "passed"
        print(json.dumps(metadata["summary"], indent=2))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["error"] = str(error)
        raise
    finally:
        (output / "Metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
