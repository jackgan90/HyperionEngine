"""Editor benchmark inputs and coverage checks shared by measurement tools."""
import json
import pathlib
import subprocess


def render_settings(output, *, pipeline="deferred", layout="compact", shadows=True, resolution=2048):
    fields = {"pipeline": pipeline, "gbuffer": layout, "vsync": False,
              "shadows": {"type": "hyperion.shadow.settings", "version": 1, "fields": {
                  "enabled": shadows, "resolution": resolution, "distance": 100.0, "splitLambda": .6,
                  "normalOffset": .6, "receiverBias": .15, "blendFraction": .1, "fadeFraction": .1, "debugMode": 0}}}
    output.write_text(json.dumps({"type": "hyperion.render.settings", "version": 1, "fields": fields}), encoding="utf-8")
    return output


def scene_path(editor, root, output, scene="Scene", asset_root=None):
    if scene == "Scene":
        return "/Game/Scenes/Showcase.hasset"
    source = output / "ModelWorkload.json"
    target = output / "ModelWorkload.hasset"
    source.write_text(json.dumps({"type": "hyperion.scene", "schema_version": 1,
        "assets": [{"id": "model", "path": "/Game/Models/Showcase.hasset"}],
        "instances": [{"id": "model", "asset": "model"}],
        "camera": {"eye": [2, 2, 7], "target": [0, 0, 0], "near": .01, "far": 200}}), encoding="utf-8")
    tool = pathlib.Path(editor).with_name("hyperion_asset_tool.exe")
    result = subprocess.run([str(tool), "--asset-root", str((asset_root or root.parent / "HyperionAssets").resolve()),
                             "import", str(source), str(target)], capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return str(target)


def validate_workload(log, rows, scene="Scene"):
    if "Editor scene ready:" not in log or "validation errors: 0" not in log or not rows:
        raise RuntimeError("Editor benchmark did not complete a ready, validated workload")
    for row in rows:
        visible = int(row["visible_items"])
        if visible <= 0 or int(row["scene_draws"]) <= 0:
            raise RuntimeError("Benchmark has empty coverage")
        if scene == "Model" and visible != 4:
            raise RuntimeError("Single Showcase model must contain four primitives")
        if int(row["failed_items"]) or int(row["shadow_failed"]):
            raise RuntimeError("Benchmark contains failed items")
        if int(row["instanced_items"]) + int(row["single_draws"]) != visible:
            raise RuntimeError("Benchmark item accounting is incomplete")
        if not int(row["viewport_width"]) or not int(row["viewport_height"]):
            raise RuntimeError("Missing viewport dimensions")
