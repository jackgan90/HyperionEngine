"""Prepare optional source-import integration fixtures from external cached inputs."""
import argparse
import json
import pathlib
import shutil

from PrepareContent import ROOT, contained, restore


def prepare_sources(manifest_path, source_cache, output):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for entry in manifest["downloads"]:
        source = contained(source_cache, entry["path"])
        target = contained(output, entry["path"])
        if source.is_file() and source != target:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    restore(manifest, output, offline=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-root", type=pathlib.Path, default=ROOT.parent / "HyperionAssets")
    parser.add_argument("--engine-sky-cache", type=pathlib.Path, default=ROOT / "out/DefaultSkySources")
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "out/fixtures/Sources")
    args = parser.parse_args()
    assets = args.assets_root.resolve()
    manifest_path = assets / "Metadata/Sources.json"
    if not manifest_path.is_file():
        raise RuntimeError("Source integration tests require HyperionAssets metadata and source cache; run PrepareContent.py first")
    cache = args.output.resolve()
    try:
        prepare_sources(ROOT / "Content/Metadata/DefaultSkySources.json", args.engine_sky_cache.resolve(), cache)
    except RuntimeError as error:
        raise RuntimeError("Engine sky source fixtures are unavailable; run PrepareContent.py --engine-sky "
                           "--restore-only, or pass --engine-sky-cache with a prepared cache") from error
    prepare_sources(manifest_path, assets / ".cache/Sources", cache)


if __name__ == "__main__":
    main()
