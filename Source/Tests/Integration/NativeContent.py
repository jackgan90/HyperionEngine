"""Use the engine importer for integration-test scene sources."""
import json
import pathlib
import subprocess


def import_asset(viewer, source, output=None, *, asset_root=None):
    tool = viewer.with_name("hyperion_asset_tool" + viewer.suffix)
    source = pathlib.Path(source)
    output = pathlib.Path(output) if output else source.with_suffix(".hasset")
    arguments = [str(tool)] + (["--asset-root", str(asset_root)] if asset_root else [])
    result = subprocess.run(arguments + ["import", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return output


def set_initial_view_from_camera(viewer, scene, output=None, *, asset_root=None):
    """Explicitly retain a fixture's authored root-camera framing for browser-view tests."""
    tool = viewer.with_name("hyperion_asset_tool" + viewer.suffix)
    output = pathlib.Path(output if output else scene)
    source = output.with_name(output.stem + "InitialView.json")
    arguments = [str(tool)] + (["--asset-root", str(asset_root)] if asset_root else [])
    subprocess.run(arguments + ["export-json", str(scene), str(source)],
                   check=True, capture_output=True, text=True, timeout=60)
    document = json.loads(source.read_text(encoding="utf-8"))
    fields = document["fields"]
    camera = next(node["fields"] for node in fields["nodes"]
                  if node["fields"]["id"] == fields["defaultCamera"])
    components = {value["type"]: value["state"] for value in camera["components"]}
    transform = components["hyperion.scenetransform"]["fields"]
    assert not transform["parent"], "This fixture helper requires a root camera"
    fields["initialView"] = {
        "type": "hyperion.sceneview", "version": 1,
        "fields": {"world": transform["local"], "lens": components["hyperion.scenecamera"]},
    }
    source.write_text(json.dumps(document), encoding="utf-8")
    return import_asset(viewer, source, output, asset_root=asset_root)
