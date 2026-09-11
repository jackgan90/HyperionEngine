"""Generate test runtime assets with the C++ importer; this script contains no wire-format logic."""
import argparse
import pathlib
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=pathlib.Path, required=True)
    parser.add_argument("--source", type=pathlib.Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    output = source / "native"
    for name in ("Showcase.gltf", "Showcase.glb", "DataUri.gltf", "Jpeg.gltf",
                 "Sparse.gltf", "Interleaved.gltf", "SparseInterleaved.gltf",
                 "Normalized.gltf", "Strip.gltf", "Fan.gltf"):
        destination = output / (name.replace(".", "-") + ".hasset")
        subprocess.run([str(args.tool.resolve()), "import", str(source / name), str(destination)],
                       check=True, timeout=60)


if __name__ == "__main__":
    main()
