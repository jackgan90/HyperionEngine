"""Exercise real Tracy sessions, resumable tasks, GPU retirement and Viewer capture windows."""
import collections
import csv
import json
import os
import pathlib
import subprocess
import sys
import time


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


def check_viewer(profile, viewer, output, mode):
    folder = output / mode
    profile.run([sys.executable, profile.ROOT / "tools/Profile.py", "--viewer", viewer, "--mode", mode,
                 "--out", folder, "--no-build", "--warmup", "120", "--frames", "120"],
                output / (mode + ".log"), timeout=60)
    summary = json.loads((folder / "Metadata.json").read_text())["summary"]
    assert 1 <= summary["draw_min"] <= summary["draw_max"] <= 10, summary
    events = rows(folder / "Events.csv")
    frames = [event for event in events if event["name"] == "ApplicationFrame"]
    assert len(frames) == 120 and sorted(int(event["value"].split()[0]) for event in frames) == list(range(120, 240))
    names = {event["name"] for event in events}
    assert {"PrepareMaterials", "PrepareDraws", "ValidateDraws", "RecordCommands", "SubmitCommandLists", "PresentWait"} <= names
    if mode == "basic":
        assert "BindMaterialConstants" not in names and not rows(folder / "Gpu.csv")
    else:
        assert {"BindMaterialConstants", "RefreshMaterialEvaluation", "RecordGraphicsBindings"} <= names
        gpu = rows(folder / "Gpu.csv")
        assert len(gpu) == sum(event["name"] == "RecordPass" for event in events), len(gpu)
        assert all(0 <= int(event["GPU execution time"]) < 1_000_000_000 for event in gpu)
    plots = collections.defaultdict(list)
    for event in rows(folder / "EventsAndPlots.csv"):
        if not event["src_file"]:
            plots[event["name"]].append(float(event["value"]))
    for name in ("SceneDraws", "MaterialEvaluationFull", "MaterialEvaluationReuses", "MaterialEvaluationRefreshes",
                 "ProviderEvaluations", "ProviderReuses", "ConstantPacks", "ConstantUploadBytes", "ConstantReuses",
                 "BindingSetsCreated", "BindingSetReuses", "PipelinesCreated", "PipelineReuses"):
        assert len(plots[name]) == 120 and min(plots[name]) >= 0, (name, len(plots[name]))
    assert sum(plots["MaterialEvaluationRefreshes"]) > 0 and max(plots["SceneDraws"]) <= 210
    return summary


def check_asset_load(profile, viewer, output):
    sources = {"ReadAssetBytes": "IOService.cpp", "ImportGltf": "GltfImport.cpp",
               "ParseGltf": "GltfImport.cpp", "ConvertGltfMeshes": "GltfImport.cpp",
               "DecodeImage": "Images.cpp"}
    summaries = {}
    for category in ("assets", "frame"):
        folder = output / ("AssetLoad-" + category)
        folder.mkdir()
        profile.capture([str(viewer), "--config", str(profile.ROOT / "experiments/Scene.json"),
                         "--frames", "240", "--hidden", "--no-vsync", "--profile-wait",
                         "--profile-categories", category, "--benchmark-warmup", "120",
                         "--benchmark", str(folder / "Frames.csv")], folder, 60)
        profile.export(folder)
        events = rows(folder / "Events.csv")
        counts = collections.Counter(event["name"] for event in events)
        if category == "assets":
            for name, source in sources.items():
                matches = [event for event in events if event["name"] == name]
                assert matches and all(event["src_file"].endswith(source) and
                                       int(event["exec_time_ns"]) >= 0 for event in matches), (name, matches)
            assert "ApplicationFrame" not in counts
        else:
            assert not sources.keys() & counts.keys(), counts
            assert counts["ApplicationFrame"] == 240, counts
        summary = profile.frame_statistics(folder / "Frames.csv")
        assert summary["samples"] == 120 and 1 <= summary["draw_min"] <= summary["draw_max"] <= 10, summary
        summaries[category] = {"scope_counts": dict(counts), "frames": summary}
    return summaries


def main():
    viewer, root, core, native = [pathlib.Path(value).resolve() for value in sys.argv[1:5]]
    sys.path.insert(0, str(root / "tools"))
    import Profile as profile
    if not (profile.TOOLS / "tracy-capture.exe").is_file():
        print("Build tools/BuildProfilingTools.ps1 to enable real Tracy acceptance")
        return 77
    output = root / "out/Profiling/Acceptance" / time.strftime("%Y%m%d-%H%M%S")
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
        summary[mode] = check_viewer(profile, viewer, output, mode)
    summary["asset_load"] = check_asset_load(profile, viewer, output)
    for options in (("--profile", "--frames", "10", "--profile-start", "10"),
                    ("--profile-start", "2"), ("--profile-categories", "unknown"),
                    ("--profile-categories", "frame,"), ("--profile", "--profile-frames", "-1")):
        result = subprocess.run([str(viewer), *options], capture_output=True, text=True, timeout=10,
                                **profile.hidden_process_options())
        assert result.returncode != 0, options
    (output / "Summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print("PASS: real reconnects, nested/exception/suspended task zones, GPU cancellation/fence recovery, Viewer controls")
    return 0


if __name__ == "__main__":
    sys.exit(main())
