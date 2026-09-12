"""Both startup depth conventions in all applications, plus pending-edit persistence."""
import json
import os
import pathlib
import subprocess
import sys
from NativeContent import import_asset


viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "depth-acceptance"
work.mkdir(exist_ok=True)
scene = {
    "type": "hyperion.scene", "schema_version": 1,
    "assets": [{"id": "model", "path": os.path.relpath(root / "assets/Models/Showcase.gltf", work)}],
    "instances": [{"id": "front", "asset": "model"},
                  {"id": "back", "asset": "model", "translation": [0.5, 0, -2]}],
    "camera": {"eye": [0, 1, 7], "target": [0, 0, 0], "near": 0.05, "far": 200},
}
manifest = work / "Scene.json"
manifest.write_text(json.dumps(scene), encoding="utf-8")
manifest = import_asset(viewer, manifest)


def run(name, config, application, pipeline, *extra):
    capture = work / f"{name}.png"
    args = [str(viewer), "--config", str(config), "--pipeline", pipeline,
            "--frames", "900",
            "--hidden", "--no-ui", "--no-vsync", "--capture", str(capture)]
    if application == "Model":
        args += ["--model", str(root / "out/content/Models/Showcase.hasset"), "--verify-model"]
    elif application == "Scene":
        args += ["--scene", str(manifest), "--verify-model"]
    else:
        args += ["--verify-triangle"]
    result = subprocess.run(args + list(map(str, extra)), cwd=work, capture_output=True,
                            text=True, timeout=50)
    log = result.stdout + result.stderr
    (work / f"{name}.log").write_text(log, encoding="utf-8")
    assert result.returncode == 0 and "validation errors: 0" in log, log
    assert capture.stat().st_size > 1000, "Missing or empty application image"
    if application == "Scene":
        assert "2/2 models ready" in log, log
    return log, capture.read_bytes()


baselines = {}
configs = {}
for application in ("Triangle", "Model", "Scene"):
    for reversed_z in (False, True):
        mode = "reversed" if reversed_z else "standard"
        settings = json.loads((root / "experiments" / f"{application}.json").read_text())
        settings["properties"].update(reversed_z=reversed_z, width=640, height=480,
                                      main_render_lead=2, render_rhi_lead=2)
        config = work / f"{application}-{mode}.json"
        config.write_text(json.dumps(settings), encoding="utf-8")
        configs[application, mode] = config
        for pipeline in ("forward", "deferred"):
            log, image = run(f"{application}-{mode}-{pipeline}", config, application, pipeline)
            assert f"Depth convention: active={mode}; configured={mode}" in log, log
            baselines[application, mode, pipeline] = image

# Pending settings edits cannot alter projection, compare, clear, or clip-space transformation.
# Save and start a new process to demonstrate the desired value actually takes effect on restart.
for mode, pending in (("standard", "reversed"), ("reversed", "standard")):
    saved = work / f"Pending-{pending}.json"
    log, image = run(f"Pending-{mode}", configs["Triangle", mode], "Triangle", "deferred",
                     "--exercise-depth-config", "--save-config", saved)
    assert f"Depth convention: active={mode}; configured={pending}" in log, log
    assert image == baselines["Triangle", mode, "deferred"], "Pending edit changed active frame output"
    assert json.loads(saved.read_text())["properties"]["reversed_z"] == (pending == "reversed")
    log, _ = run(f"Restart-{pending}", saved, "Triangle", "deferred")
    assert f"Depth convention: active={pending}; configured={pending}" in log, log

print("Both depth modes passed for Triangle/Model/Scene and Forward/Deferred; pending edits require restart")
