"""Scene manifest loading, culling equivalence, shared instances and failure isolation."""
import json
import os
import pathlib
import re
import subprocess
import sys

viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "scene-acceptance"
work.mkdir(exist_ok=True)
assets = [{"id": "showcase", "path": os.path.relpath(root / "assets/Models/Showcase.gltf", work)},
          {"id": "interleaved", "path": os.path.relpath(root / "assets/Models/Interleaved.gltf", work)}]
instances = [{"id": "center", "asset": "showcase"},
             {"id": "side", "asset": "interleaved", "translation": [2, 0, -1]}]
instances.extend({"id": f"outside-{i}", "asset": "showcase", "translation": [100 + i * 3, 0, 0]}
                 for i in range(512))
scene = {"type": "hyperion.scene", "schema_version": 1, "assets": assets, "instances": instances,
         "camera": {"eye": [0, 1, 7], "target": [0, 0, 0], "near": 0.05, "far": 1000}}
manifest = work / "Scene.json"


def run(name, mode="bvh", show_ui=False, verify=True):
    capture = work / f"{name}.png"
    args = [str(viewer), "--scene", str(manifest), "--scene-culling", mode,
            "--frames", "180", "--hidden", "--capture", str(capture)]
    if verify:
        args.append("--verify-model")
    if not show_ui:
        args.append("--no-ui")
    result = subprocess.run(args, cwd=work, capture_output=True, text=True, timeout=45)
    log = result.stdout + result.stderr
    (work / f"{name}.log").write_text(log, encoding="utf-8")
    return result.returncode, log, capture


manifest.write_text(json.dumps(scene), encoding="utf-8")
images = []
statistics = {}
for mode in ("none", "linear", "bvh"):
    code, log, capture = run(mode, mode)
    assert code == 0 and "514/514 models ready" in log and "validation errors: 0" in log, log
    values = re.search(r"groups=(\d+) visits=(\d+) group_tests=(\d+) collects=(\d+) items=(\d+) draws=(\d+)", log)
    assert values, log
    statistics[mode] = tuple(map(int, values.groups()))
    images.append(capture.read_bytes())
assert images[0] == images[1] == images[2], "Spatial culling changed the rendered image"
assert statistics["bvh"][1] < statistics["linear"][2] // 4, statistics
assert statistics["bvh"][3] == statistics["linear"][3] < statistics["none"][3], statistics
code, log, capture = run("gui", show_ui=True)
assert code == 0 and capture.stat().st_size > 10000, log
scene["assets"].append({"id": "broken", "path": "Missing.gltf"})
scene["instances"].append({"id": "failed", "asset": "broken"})
manifest.write_text(json.dumps(scene), encoding="utf-8")
code, log, capture = run("partial-failure", verify=False)
assert code == 0 and "514/515 models ready | 1 failed" in log, log
assert capture.read_bytes() == images[2], "A failed asset interrupted valid scene rendering"
scene["instances"][1]["id"] = "center"
manifest.write_text(json.dumps(scene), encoding="utf-8")
code, log, _ = run("invalid")
assert code != 0 and "Invalid scene instance" in log, log
print(f"Scene manifests, 514 instances, identical mode images, GUI and isolated failure passed: {statistics}")
