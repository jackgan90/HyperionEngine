"""End-to-end Viewer CLI: native glTF and GLB imports, model UI and reported errors."""
import pathlib
import subprocess
import sys

viewer = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(sys.argv[2]).resolve()
work = pathlib.Path.cwd() / "model-acceptance"
work.mkdir(exist_ok=True)

for name, show_ui in [("Showcase-gltf.hasset", True), ("Showcase-glb.hasset", False), ("Missing.hasset", False)]:
    capture = root / "out/captures" / ("model-viewer-" + name + ".png")
    args = [str(viewer), "--model", str(root / "out/fixtures/native" / name),
            "--frames", "180", "--hidden", "--capture", str(capture), "--verify-model"]
    if not show_ui:
        args.append("--no-ui")
    result = subprocess.run(args, cwd=work, capture_output=True, text=True, timeout=40)
    log = result.stdout + result.stderr
    (work / (name + ".log")).write_text(log, encoding="utf-8")
    if name == "Missing.hasset":
        assert result.returncode != 0 and "Missing.hasset" in log, log
    else:
        assert result.returncode == 0 and "validation errors: 0" in log, log
        assert capture.stat().st_size > 10000, "Model capture is unexpectedly empty"
        assert "Domain IO:" in log, "No IO domain statistics"
blocked = work / "directory-not-file"
blocked.mkdir(exist_ok=True)
capture = work / "save-failure.png"
result = subprocess.run(
    [str(viewer), "--model", str(root / "out/fixtures/native/Showcase-glb.hasset"),
     "--frames", "180", "--hidden", "--no-ui", "--capture", str(capture),
     "--verify-model", "--save-config", str(blocked)],
    cwd=work, capture_output=True, text=True, timeout=40)
log = result.stdout + result.stderr
(work / "save-failure.log").write_text(log, encoding="utf-8")
assert result.returncode != 0 and "Cannot replace asset file" in log, log
assert capture.stat().st_size > 10000, "Other queued writes must drain after one save fails"
print("Native glTF and GLB imports, model UI, missing dependency and failed-save shutdown acceptance passed")
