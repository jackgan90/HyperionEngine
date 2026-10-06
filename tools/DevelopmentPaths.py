"""Shared developer-tool paths; runtime storage never depends on these outputs."""
import argparse
import hashlib
import json
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]


def output_root(root=ROOT):
    return pathlib.Path(os.environ.get("HYP_OUT_ROOT", root / "out")).resolve()


def test_root(root=ROOT):
    return output_root(root) / "tests"


def tool_cache_root():
    explicit = os.environ.get("HYP_TOOL_CACHE")
    if explicit:
        return pathlib.Path(explicit).resolve()
    local = os.environ.get("LOCALAPPDATA")
    if not local:
        raise RuntimeError("LOCALAPPDATA is unavailable; set HYP_TOOL_CACHE to a writable local directory")
    return pathlib.Path(local).resolve() / "Hyperion" / "ToolCache"


def dependency_root(lock=None):
    if lock is None:
        lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))
    identity = hashlib.sha256(json.dumps(lock, sort_keys=True, separators=(",", ":")).encode()).hexdigest()[:20]
    return tool_cache_root() / "Dependencies" / identity


def source_cache(name):
    if name not in ("EngineSky", "Game"):
        raise ValueError("Unknown source cache identity")
    return tool_cache_root() / "SourceAssets" / name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--field", choices=("out", "tests", "tool-cache", "deps"))
    args = parser.parse_args()
    paths = {"out": output_root, "tests": test_root, "tool-cache": tool_cache_root, "deps": dependency_root}
    if args.field:
        print(paths[args.field]().as_posix())
    else:
        print(json.dumps({name: resolve().as_posix() for name, resolve in paths.items()}))


if __name__ == "__main__":
    main()
