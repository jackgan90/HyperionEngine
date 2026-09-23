"""Recover source cache from versioned recipes and publish through the C++ AssetTool."""
import argparse
import base64
import hashlib
import json
import pathlib
import subprocess
import urllib.request

from GenerateModelFixtures import build_showcase, generate
from GenerateShadowFixtures import generate as generate_shadows

ROOT = pathlib.Path(__file__).resolve().parents[1]


def contained(root, name):
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()) or path == root.resolve():
        raise ValueError(f"Manifest path escapes root: {name}")
    return path


def restore(manifest, cache, offline=False):
    for name, expected in manifest.get("generator_sha256", {}).items():
        generator = contained(ROOT / "tools", name)
        if hashlib.sha256(generator.read_text(encoding="utf-8").encode("utf-8")).hexdigest() != expected:
            raise RuntimeError(f"Generator version mismatch: {name}; use the matching engine checkout")
    cache.mkdir(parents=True, exist_ok=True)
    generate(cache / "Models")
    generate_shadows(cache)
    _, geometry = build_showcase()
    for name, recipe in manifest["recipes"].items():
        path = contained(cache, name)
        value = json.loads(json.dumps(recipe))
        if name == "Models/Ground.gltf":
            value["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(geometry[:912]).decode()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    for entry in manifest["downloads"]:
        path = contained(cache, entry["path"])
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]:
            continue
        if offline:
            raise RuntimeError(f"Missing or changed cached source: {entry['path']}; rerun without --offline")
        path.parent.mkdir(parents=True, exist_ok=True)
        request = urllib.request.Request(entry["url"], headers={"User-Agent": "HyperionContent/1.0"})
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise RuntimeError(f"Source hash mismatch: {entry['path']}")
        path.write_bytes(data)


def run_tool(tool, assets, *arguments):
    subprocess.run([str(tool), "--asset-root", str(assets), *map(str, arguments)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-root", type=pathlib.Path, default=ROOT.parent / "HyperionAssets")
    parser.add_argument("--cache", type=pathlib.Path)
    parser.add_argument("--tool", type=pathlib.Path, default=ROOT / "out/build/release/bin/hyperion_asset_tool.exe")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--restore-only", action="store_true")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    assets = args.assets_root.resolve()
    cache = (args.cache or assets / ".cache/Sources").resolve()
    manifest = json.loads((assets / "Metadata/Sources.json").read_text(encoding="utf-8"))
    if manifest["version"] != 1:
        raise ValueError("Unsupported source manifest version")
    restore(manifest, cache, args.offline)
    if args.restore_only:
        return
    tool = args.tool.resolve()
    if not (ROOT / "Content/Textures/EnvironmentBrdf.hasset").is_file():
        run_tool(tool, assets, "--authoring", "build-brdf", "/Engine/Textures/EnvironmentBrdf.hasset")
    for entry in manifest["outputs"]:
        output = "/Game/" + entry["path"]
        options = ["--library", "/Game", "--source-root", str(cache), "--source-id", manifest["source_id"]]
        if args.force:
            options.append("--force")
        run_tool(tool, assets, "import", contained(cache, entry["source"]), output, *options)
    run_tool(tool, assets, "validate-library", "/Game")


if __name__ == "__main__":
    main()
