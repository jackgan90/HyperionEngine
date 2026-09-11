"""Use the engine importer for integration-test scene sources."""
import pathlib
import subprocess


def import_asset(viewer, source, output=None):
    tool = viewer.with_name("hyperion_asset_tool" + viewer.suffix)
    source = pathlib.Path(source)
    output = pathlib.Path(output) if output else source.with_suffix(".hasset")
    result = subprocess.run([str(tool), "import", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return output
