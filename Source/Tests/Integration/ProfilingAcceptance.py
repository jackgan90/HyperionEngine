"""Exercise real Tracy sessions, resumable tasks, GPU retirement and Editor capture windows."""
import collections
import csv
import json
import os
import pathlib
import subprocess
import sys
import time


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "tools"))
from TestEnvironment import test_output_root


def rows(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def check_cpu(path):
    events = rows(path / "Events.csv")
    names = {event["name"] for event in events}
    expected = {"TraceOuter", "TraceSecond", "TraceLeaf", "TraceException", "TraceRecovery", "TraceGateEnd",
                "WorkerParent", "TraceTaskError", "TraceTaskErrorRecovery", "TaskExecute", "TaskWait", "TaskDispatch"}
    assert expected <= names, expected - names
    assert all("ProfilingTests.cpp" in event["src_file"] for event in events if event["name"].startswith("Trace"))
    assert any(event["name"] == "TaskExecute" and int((event["value"] or "0").split()[0]) > 0 for event in events)
    by_thread = collections.defaultdict(list)
    for event in events:
        start = int(event["ns_since_start"])
        duration = int(event["exec_time_ns"])
        assert duration >= 0, event
        if duration:
            by_thread[event["thread"]].append((start, start + duration, event["name"]))
    for events_on_thread in by_thread.values():
        stack = []
        for start, end, name in sorted(events_on_thread, key=lambda event: (event[0], -event[1])):
            while stack and start >= stack[-1][0]:
                stack.pop()
            assert not stack or end <= stack[-1][0] + 2, (name, start, end, stack[-1])
            stack.append((end, name))
    plots = rows(path / "EventsAndPlots.csv")
    assert any(event["name"] == "TraceWorkItems" and not event["src_file"] for event in plots)
    return len(events)


def reconnect(profile, executable, output):
    port = profile.available_port()
    process = None
    collector = None
    summaries = []
    with (output / "Core.log").open("w", encoding="utf-8") as log:
        try:
            process = subprocess.Popen([str(executable), "--capture-workload"], cwd=profile.ROOT, stdout=log,
                                       stderr=subprocess.STDOUT, env={**os.environ, "TRACY_PORT": str(port)},
                                       **profile.hidden_process_options())
            for index in range(2):
                folder = output / f"Connection{index}"
                folder.mkdir()
                with (folder / "Collector.log").open("w", encoding="utf-8") as collector_log:
                    collector = subprocess.Popen([str(profile.TOOLS / "tracy-capture.exe"), "-p", str(port),
                                                  "-o", str(folder / "Capture.tracy"), "-s", "3"],
                                                 stdout=collector_log, stderr=subprocess.STDOUT,
                                                 **profile.hidden_process_options())
                    assert collector.wait(timeout=20) == 0
                profile.check_capture(folder)
                # Reconnect promptly; export afterward so the worker resumes inside the second connection.
                time.sleep(0.25)
            assert process.wait(timeout=20) == 0, (output / "Core.log").read_text()
        finally:
            profile.stop(collector)
            profile.stop(process)
    for index in range(2):
        folder = output / f"Connection{index}"
        profile.export(folder)
        summaries.append(check_cpu(folder))
        segments = [event for event in rows(folder / "Events.csv") if event["name"] == "TraceSuspendedWorker"]
        assert segments and all(int(event["exec_time_ns"]) < 1_000_000_000 for event in segments), segments
    return summaries


def check_editor(profile, editor, output, mode, warmup=120):
    folder = output / f"{mode}-{warmup}"
    profile.run([sys.executable, profile.ROOT / "tools/Profile.py", "--editor", editor, "--mode", mode,
                 "--out", folder, "--no-build", "--warmup", str(warmup), "--frames", "120"],
                output / f"{mode}-{warmup}.log", timeout=100)
    summary = json.loads((folder / "Metadata.json").read_text())["summary"]
    assert summary["samples"] == 120 and summary["draw_min"] > 0, summary
    events = rows(folder / "Events.csv")
    frames = [event for event in events if event["name"] == "EditorApplicationFrame"]
    assert len(frames) == 120 and sorted(int(event["value"].split()[0]) for event in frames) == list(range(warmup, warmup + 120))
    assert [int(row["frame"]) for row in rows(folder / "Frames.csv")] == list(range(warmup, warmup + 120))
    names = {event["name"] for event in events}
    assert {"EditorSceneUpdate", "EditorRenderWait", "PrepareMaterials", "PrepareDraws", "PrepareRetainedView",
            "ValidateDraws", "RecordCommands", "RecordNativeDraws", "ResetNativeCommandList", "SubmitCommandLists", "PresentWait"} <= names, names
    if mode == "basic":
        assert "BindMaterialConstants" not in names and not rows(folder / "Gpu.csv")
    else:
        assert {"BindMaterialConstants", "RecordGraphicsBindings"} <= names
        gpu = rows(folder / "Gpu.csv")
        assert gpu and all(0 <= int(event["GPU execution time"]) < 1_000_000_000 for event in gpu)
    return summary


def check_asset_load(profile, editor, output):
    summaries = {}
    for category in ("assets", "frame"):
        folder = output / ("AssetLoad-" + category)
        folder.mkdir()
        profile.capture([str(editor), "--asset-root", str(profile.ROOT.parent / "HyperionAssets"),
                         "--scene", "/Game/Scenes/Showcase.hasset", "--hidden", "--profile-wait",
                         "--profile-categories", category, "--benchmark-warmup", "30", "--benchmark-samples", "30",
                         "--benchmark", str(folder / "Frames.csv")], folder, 90)
        profile.export(folder)
        events = rows(folder / "Events.csv")
        counts = collections.Counter(event["name"] for event in events)
        if category == "assets":
            assert counts["ReadAssetBytes"] > 0 and "EditorApplicationFrame" not in counts, counts
        else:
            assert "ReadAssetBytes" not in counts and counts["EditorApplicationFrame"] >= 60, counts
        summary = profile.frame_statistics(folder / "Frames.csv")
        assert summary["samples"] == 30 and summary["draw_min"] > 0, summary
        summaries[category] = {"scope_counts": dict(counts), "frames": summary}
    return summaries


def main():
    editor, root, core, native = [pathlib.Path(value).resolve() for value in sys.argv[1:5]]
    sys.path.insert(0, str(root / "tools"))
    import Profile as profile
    if not (profile.TOOLS / "tracy-capture.exe").is_file():
        print("Build tools/BuildProfilingTools.ps1 to enable real Tracy acceptance")
        return 77
    output = test_output_root() / "Profiling/Acceptance" / time.strftime("%Y%m%d-%H%M%S")
    output.mkdir(parents=True)
    summary = {"reconnect_event_counts": reconnect(profile, core, output)}
    for name, code, timeout, exception_type in (
            ("FailedClient", "raise SystemExit(7)", 5, RuntimeError),
            ("TimedOutClient", "import time; time.sleep(20)", 0.2, TimeoutError)):
        folder = output / name
        folder.mkdir()
        try:
            profile.capture([sys.executable, "-c", code], folder, timeout)
        except exception_type:
            pass
        else:
            raise AssertionError("Capture helper failed to propagate " + name)
    gpu = output / "NativeGpu"
    gpu.mkdir()
    profile.capture([str(native), "--profile-wait"], gpu, 60, cwd=gpu)
    profile.export(gpu)
    assert len(rows(gpu / "Gpu.csv")) > 10
    summary["native_gpu_spans"] = len(rows(gpu / "Gpu.csv"))
    for mode in ("basic", "detail-gpu"):
        summary[mode] = check_editor(profile, editor, output, mode)
    summary["zero_warmup"] = check_editor(profile, editor, output, "basic", warmup=0)
    summary["asset_load"] = check_asset_load(profile, editor, output)
    for options in (("--profile-start", "2"), ("--profile-categories", "unknown"),
                    ("--profile-categories", "frame,"), ("--profile", "--profile-frames", "-1"),
                    ("--kernel-only", "--frames", "8", "--profile", "--profile-start", "10"),
                    ("--kernel-only", "--frames", "8", "--profile", "--profile-start", "6", "--profile-frames", "3"),
                    ("--kernel-only", "--scene", "/Game/Scenes/Showcase.hasset", "--benchmark", str(output / "Invalid.csv"),
                     "--benchmark-warmup", "0", "--benchmark-samples", "8", "--profile", "--profile-start", "8")):
        result = subprocess.run([str(editor), *options], capture_output=True, text=True, timeout=10,
                                **profile.hidden_process_options())
        assert result.returncode != 0, options
    (output / "Summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print("PASS: real reconnects, nested/exception/suspended task zones, GPU cancellation/fence recovery, Editor controls")
    return 0


if __name__ == "__main__":
    sys.exit(main())
