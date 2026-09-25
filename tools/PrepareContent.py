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
    if manifest.get("generator_sha256"):
        generate(cache / "Models")
        generate_shadows(cache)
    for name, recipe in manifest["recipes"].items():
        path = contained(cache, name)
        value = json.loads(json.dumps(recipe))
        if name == "Models/Ground.gltf":
            _, geometry = build_showcase()
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
    root_options = ["--asset-root", str(assets)] if assets is not None else []
    subprocess.run([str(tool), *root_options, *map(str, arguments)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-root", type=pathlib.Path, default=ROOT.parent / "HyperionAssets")
    parser.add_argument("--cache", type=pathlib.Path)
    parser.add_argument("--tool", type=pathlib.Path, default=ROOT / "out/build/release/bin/hyperion_asset_tool.exe")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--restore-only", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--engine-sky", action="store_true",
                        help="Restore and rebuild only the built-in default sky; --cache selects its source cache")
    args = parser.parse_args()
    assets = args.assets_root.resolve()
    default_cache = ROOT / "out/DefaultSkySources" if args.engine_sky else assets / ".cache/Sources"
    cache = (args.cache or default_cache).resolve()
    manifest_path = (ROOT / "Content/Metadata/DefaultSkySources.json" if args.engine_sky
                     else assets / "Metadata/Sources.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest["version"] != 1:
        raise ValueError("Unsupported source manifest version")
    restore(manifest, cache, args.offline)
    if args.restore_only:
        return
    tool = args.tool.resolve()
    tool_assets = None if args.engine_sky else assets
    if not (ROOT / "Content/Textures/EnvironmentBrdf.hasset").is_file():
        run_tool(tool, tool_assets, "--authoring", "build-brdf", "/Engine/Textures/EnvironmentBrdf.hasset")
    for entry in manifest["outputs"]:
        mount = "/Engine" if args.engine_sky else "/Game"
        output = mount + "/" + entry["path"]
        options = ["--library", mount, "--source-root", str(cache), "--source-id", manifest["source_id"]]
        if entry.get("id"):
            options.extend(["--root-id", entry["id"]])
        if args.force:
            options.append("--force")
        authoring = ["--authoring"] if args.engine_sky else []
        run_tool(tool, tool_assets, *authoring, "import", contained(cache, entry["source"]), output, *options)
    run_tool(tool, tool_assets, "validate-library", "/Engine" if args.engine_sky else "/Game")


if __name__ == "__main__":
    main()
