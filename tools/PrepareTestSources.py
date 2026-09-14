"""Prepare optional source-import integration fixtures from external cached inputs."""
import argparse
import json
import pathlib
import shutil

from PrepareContent import ROOT, contained, restore


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-root", type=pathlib.Path, default=ROOT.parent / "HyperionAssets")
    args = parser.parse_args()
    assets = args.assets_root.resolve()
    manifest_path = assets / "Metadata/Sources.json"
    if not manifest_path.is_file():
        raise RuntimeError("Source integration tests require HyperionAssets metadata and source cache; run PrepareContent.py first")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    cache = ROOT / "out/fixtures/Sources"
    for entry in manifest["downloads"]:
        source = contained(assets / ".cache/Sources", entry["path"])
        target = contained(cache, entry["path"])
        if source.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    restore(manifest, cache, offline=True)


if __name__ == "__main__":
    main()
